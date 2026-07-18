#include "AI/Combat/MATacticalAdvisorSubsystem.h"

#include "AI/Combat/MAAdvisorDecision.h"
#include "AI/Combat/MACombatDirectorSubsystem.h"
#include "AI/MATargetDummy.h"
#include "Dialogue/MADialogueSubsystem.h"
#include "LLM/MALLMStreamingClient.h"
#include "MAGameState.h"
#include "Player/MAPlayerController.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystem/MAAttributeSet.h"
#include "AbilitySystem/MAGameplayTags.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerState.h"

DEFINE_LOG_CATEGORY_STATIC(LogMAAdvisor, Log, All);

static TAutoConsoleVariable<int32> CVarAdvisorEnabled(
	TEXT("ma.AI.Advisor.Enabled"), 1,
	TEXT("1 = LLM tactical advisor issues periodic orders (attack slots / focus target / taunts)."));

static TAutoConsoleVariable<float> CVarAdvisorInterval(
	TEXT("ma.AI.Advisor.Interval"), 10.0f,
	TEXT("Seconds between advisor consultations while combat is active."));

namespace
{
	// system 常量：人设 + 输出契约。每次相同 → 便于端点 context cache 命中。
	const TCHAR* GAdvisorSystemPrompt = TEXT(
		"你是多人动作游戏中敌方阵营的战术指挥官，冷酷、好胜、说话短。"
		"根据给出的战况决定下一阶段指令。"
		"只输出一个JSON对象，禁止任何其他文字、解释或代码块标记，格式："
		"{\"max_attackers\":同时进攻的敌人数量(0到3的整数),"
		"\"focus\":\"要集火的玩家名，不指定则为空字符串\","
		"\"taunt\":\"以敌方口吻喊的一句战场狠话，不超过30字\"}");

	bool IsPawnDead(const APawn* Pawn)
	{
		if (const UAbilitySystemComponent* ASC =
			UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(const_cast<APawn*>(Pawn)))
		{
			return ASC->HasMatchingGameplayTag(MAGameplayTags::State_Dead);
		}
		return false;
	}
}

bool UMATacticalAdvisorSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

TStatId UMATacticalAdvisorSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UMATacticalAdvisorSubsystem, STATGROUP_Tickables);
}

void UMATacticalAdvisorSubsystem::Deinitialize()
{
	if (ActiveRequest.IsValid())
	{
		ActiveRequest->Cancel();
		ActiveRequest.Reset();
	}
	Super::Deinitialize();
}

void UMATacticalAdvisorSubsystem::Tick(float DeltaTime)
{
	UWorld* W = GetWorld();
	if (!W || W->GetNetMode() == NM_Client)
	{
		return;
	}

	if (CVarAdvisorEnabled.GetValueOnGameThread() == 0)
	{
		// 关闭时清掉一切残留影响，战斗系统回到纯 cvar/最近目标策略。
		FocusPawn = nullptr;
		if (UMACombatDirectorSubsystem* Director = W->GetSubsystem<UMACombatDirectorSubsystem>())
		{
			Director->SetAdvisorMaxAttackers(-1);
		}
		return;
	}

	Accum += DeltaTime;
	if (Accum < CVarAdvisorInterval.GetValueOnGameThread() || ActiveRequest.IsValid())
	{
		return;
	}
	Accum = 0.f;

	const FString Summary = BuildBattleSummary();
	if (Summary.IsEmpty())
	{
		return; // 没仗可打，不花 token
	}

	TArray<FMALLMMessage> Messages;
	Messages.Emplace(EMALLMRole::System, GAdvisorSystemPrompt);
	Messages.Emplace(EMALLMRole::User, Summary);

	TWeakObjectPtr<UMATacticalAdvisorSubsystem> WeakThis(this);
	FMALLMStreamRequest::FCallbacks Callbacks;
	Callbacks.OnComplete = [WeakThis](const FString& FullText, const TArray<FMALLMToolCall>& /*ToolCalls*/)
	{
		if (UMATacticalAdvisorSubsystem* Self = WeakThis.Get())
		{
			Self->ActiveRequest.Reset();
			Self->ApplyDecisionText(FullText);
		}
	};
	Callbacks.OnError = [WeakThis](const FString& Message)
	{
		if (UMATacticalAdvisorSubsystem* Self = WeakThis.Get())
		{
			// 失败即无事发生：BT 反射层继续用当前策略跑。
			Self->ActiveRequest.Reset();
			UE_LOG(LogMAAdvisor, Warning, TEXT("军师请求失败（战斗不受影响）：%s"), *Message);
		}
	};

	FMALLMStreamRequest::FOverrides Overrides;
	// 决策 JSON 本身很小，但思考型模型（kimi-k3）会先烧内部推理预算 ——
	// 给足空间，实际输出仍只有几十 token。
	Overrides.MaxTokens = 1024;

	FString StartError;
	ActiveRequest = FMALLMStreamRequest::Start(Messages, MoveTemp(Callbacks), StartError, Overrides);
	if (!ActiveRequest.IsValid())
	{
		UE_LOG(LogMAAdvisor, Warning, TEXT("军师请求未能发出：%s"), *StartError);
	}
}

FString UMATacticalAdvisorSubsystem::BuildBattleSummary() const
{
	UWorld* W = GetWorld();
	const UMADialogueSubsystem* Dialogue = W->GetSubsystem<UMADialogueSubsystem>();

	// 敌方阵营状态。
	int32 AliveEnemies = 0;
	float EnemyHealthSum = 0.f;
	for (TActorIterator<AMATargetDummy> It(W); It; ++It)
	{
		const AMATargetDummy* Enemy = *It;
		if (!IsValid(Enemy) || IsPawnDead(Enemy))
		{
			continue;
		}
		++AliveEnemies;
		if (const UAbilitySystemComponent* ASC =
			UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(const_cast<AMATargetDummy*>(Enemy)))
		{
			const float Max = ASC->GetNumericAttribute(UMAAttributeSet::GetMaxHealthAttribute());
			if (Max > 0.f)
			{
				EnemyHealthSum += ASC->GetNumericAttribute(UMAAttributeSet::GetHealthAttribute()) / Max;
			}
		}
	}
	if (AliveEnemies == 0)
	{
		return FString();
	}

	// 可攻击的玩家（对话豁免的玩家明确标注，防止军师点名打不了的人）。
	FString PlayerLines;
	int32 TargetablePlayers = 0;
	for (FConstPlayerControllerIterator It = W->GetPlayerControllerIterator(); It; ++It)
	{
		const APlayerController* PC = It->Get();
		const APawn* Pawn = PC ? PC->GetPawn() : nullptr;
		if (!Pawn || IsPawnDead(Pawn))
		{
			continue;
		}

		FString Name = PC->PlayerState ? PC->PlayerState->GetPlayerName() : TEXT("未知");
		float HealthPct = 100.f;
		int32 Kills = 0;
		if (const UAbilitySystemComponent* ASC =
			UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(const_cast<APawn*>(Pawn)))
		{
			const float Max = ASC->GetNumericAttribute(UMAAttributeSet::GetMaxHealthAttribute());
			if (Max > 0.f)
			{
				HealthPct = 100.f * ASC->GetNumericAttribute(UMAAttributeSet::GetHealthAttribute()) / Max;
			}
		}
		if (const APlayerState* PS = PC->PlayerState)
		{
			Kills = static_cast<int32>(PS->GetScore());
		}

		const bool bInDialogue = Dialogue && Dialogue->IsInDialogue(Cast<AMAPlayerController>(PC));
		if (!bInDialogue)
		{
			++TargetablePlayers;
		}

		PlayerLines += FString::Printf(TEXT("玩家「%s」：血量%.0f%%，击杀%d%s\n"),
			*Name, HealthPct, Kills,
			bInDialogue ? TEXT("，正在对话中（不可攻击，禁止集火）") : TEXT(""));
	}
	if (TargetablePlayers == 0)
	{
		return FString();
	}

	return FString::Printf(TEXT("敌方存活%d人，平均血量%.0f%%。\n%s"),
		AliveEnemies, 100.f * EnemyHealthSum / AliveEnemies, *PlayerLines);
}

void UMATacticalAdvisorSubsystem::ApplyDecisionText(const FString& LLMText)
{
	const FMAAdvisorDecision Decision = MAAdvisorDecision::Parse(LLMText);
	if (!Decision.bValid)
	{
		UE_LOG(LogMAAdvisor, Warning, TEXT("军师输出无法解析，忽略：%s"), *LLMText.Left(200));
		return;
	}

	UWorld* W = GetWorld();

	// 进攻名额建议（用户 cvar 和平模式在 Director 侧优先）。
	if (UMACombatDirectorSubsystem* Director = W->GetSubsystem<UMACombatDirectorSubsystem>())
	{
		Director->SetAdvisorMaxAttackers(Decision.MaxAttackers);
	}

	// 集火目标：按玩家名解析成 pawn；无匹配/对话中即不指定。
	FocusPawn = nullptr;
	if (!Decision.FocusPlayerName.IsEmpty())
	{
		const UMADialogueSubsystem* Dialogue = W->GetSubsystem<UMADialogueSubsystem>();
		for (FConstPlayerControllerIterator It = W->GetPlayerControllerIterator(); It; ++It)
		{
			const APlayerController* PC = It->Get();
			if (!PC || !PC->PlayerState || !PC->GetPawn())
			{
				continue;
			}
			if (!PC->PlayerState->GetPlayerName().Equals(Decision.FocusPlayerName, ESearchCase::IgnoreCase))
			{
				continue;
			}
			if (Dialogue && Dialogue->IsInDialogue(Cast<AMAPlayerController>(PC)))
			{
				break; // 军师点了不可攻击的人 —— 无视
			}
			FocusPawn = PC->GetPawn();
			break;
		}
	}

	// 嘲讽走击杀 feed 通道推给所有客户端。
	if (!Decision.Taunt.IsEmpty())
	{
		if (AMAGameState* GS = W->GetGameState<AMAGameState>())
		{
			GS->Multicast_OnTaunt(Decision.Taunt);
		}
	}

	UE_LOG(LogMAAdvisor, Log, TEXT("军师决策：max_attackers=%d focus=%s taunt=%s"),
		Decision.MaxAttackers,
		Decision.FocusPlayerName.IsEmpty() ? TEXT("(无)") : *Decision.FocusPlayerName,
		Decision.Taunt.IsEmpty() ? TEXT("(无)") : *Decision.Taunt);
}
