#include "Narrative/MANarrativeSubsystem.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystem/MAAttributeSet.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/GameStateBase.h"
#include "GameplayEffect.h"
#include "Narrative/MAQuestRules.h"
#include "Player/MAPlayerController.h"
#include "Player/MAPlayerState.h"

DEFINE_LOG_CATEGORY_STATIC(LogMANarrative, Log, All);

FMALLMToolSpec UMANarrativeSubsystem::GetGiveQuestToolSpec()
{
	FMALLMToolSpec Spec;
	Spec.Name = TEXT("give_quest");
	Spec.Description = TEXT("给当前对话的玩家发布一个击杀任务。玩家没有进行中任务时才可用。");
	Spec.ParametersSchemaJson = TEXT(
		"{\"type\":\"object\",\"properties\":{"
		"\"kill_count\":{\"type\":\"integer\",\"description\":\"需要击杀的数量，1到10\"},"
		"\"time_limit_seconds\":{\"type\":\"integer\",\"description\":\"限时秒数，60到600；0表示不限时\"},"
		"\"quest_line\":{\"type\":\"string\",\"description\":\"发布任务时说的一句台词，不超过60字\"}"
		"},\"required\":[\"kill_count\",\"time_limit_seconds\",\"quest_line\"]}");
	return Spec;
}

FString UMANarrativeSubsystem::ExecuteToolCall(AMAPlayerController* PC,
	const FMALLMToolCall& Call, FString& OutSpokenLine)
{
	OutSpokenLine.Reset();

	// 防线 1：白名单。
	if (Call.Name != TEXT("give_quest"))
	{
		return FString::Printf(TEXT("失败：未知工具 %s。"), *Call.Name);
	}

	AMAPlayerState* PS = PC ? PC->GetPlayerState<AMAPlayerState>() : nullptr;
	if (!PS)
	{
		return TEXT("失败：找不到玩家状态。");
	}

	// 防线 2/3/4：参数钳制 + 单任务门槛 + 每局预算（纯逻辑，见 QuestRules 测试）。
	FMAGiveQuestParams Params;
	FString Reject;
	if (!MAQuestRules::ParseAndValidateGiveQuest(Call.ArgumentsJson,
		PS->GetActiveQuest(), PS->GetQuestsIssued(), Params, Reject))
	{
		UE_LOG(LogMANarrative, Log, TEXT("give_quest 被拒：%s"), *Reject);
		return FString::Printf(TEXT("失败：%s"), *Reject);
	}

	PS->SetActiveQuest(MAQuestRules::MakeActiveQuest(Params, NowServerTime()));
	PS->IncrementQuestsIssued();
	OutSpokenLine = Params.QuestLine;
	UE_LOG(LogMANarrative, Display, TEXT("已发布任务：击杀 %d，限时 %d 秒（%s）"),
		Params.KillCount, Params.TimeLimitSeconds, *PS->GetPlayerName());
	return FString::Printf(TEXT("成功：已发布击杀 %d 人的任务%s。完成由系统自动判定并发放奖励。"),
		Params.KillCount,
		Params.TimeLimitSeconds > 0
			? *FString::Printf(TEXT("（限时 %d 秒）"), Params.TimeLimitSeconds) : TEXT(""));
}

void UMANarrativeSubsystem::NotifyKill(AMAPlayerState* KillerPS)
{
	if (!KillerPS)
	{
		return;
	}
	const MAQuestRules::EMAQuestKillResult Result =
		MAQuestRules::ApplyKill(KillerPS->GetMutableActiveQuest(), NowServerTime());
	if (Result == MAQuestRules::EMAQuestKillResult::JustCompleted)
	{
		GrantQuestReward(KillerPS);
	}
}

FString UMANarrativeSubsystem::DescribeQuestState(const AMAPlayerController* PC) const
{
	const AMAPlayerState* PS = PC ? PC->GetPlayerState<AMAPlayerState>() : nullptr;
	if (!PS)
	{
		return TEXT("未知");
	}
	const FMAQuestState& Q = PS->GetActiveQuest();
	switch (Q.Phase)
	{
	case EMAQuestPhase::Active:
		return Q.DeadlineServerTime > 0.f
			? FString::Printf(TEXT("进行中：击杀 %d/%d，剩 %.0f 秒"), Q.Progress, Q.TargetKills,
				FMath::Max(0.0, Q.DeadlineServerTime - NowServerTime()))
			: FString::Printf(TEXT("进行中：击杀 %d/%d"), Q.Progress, Q.TargetKills);
	case EMAQuestPhase::Completed: return TEXT("上一任务已完成");
	case EMAQuestPhase::Failed:    return TEXT("上一任务已超时失败");
	default:                       return TEXT("无任务");
	}
}

void UMANarrativeSubsystem::Tick(float DeltaTime)
{
	// 只有权威世界扫描过期；客户端世界静默跳过（状态靠复制到达）。
	if (!GetWorld() || !GetWorld()->GetAuthGameMode())
	{
		return;
	}
	ExpirySweepAccum += DeltaTime;
	if (ExpirySweepAccum < 1.f)
	{
		return;
	}
	ExpirySweepAccum = 0.f;
	if (AGameStateBase* GS = GetWorld()->GetGameState())
	{
		for (APlayerState* PS : GS->PlayerArray)
		{
			if (AMAPlayerState* MPS = Cast<AMAPlayerState>(PS))
			{
				MAQuestRules::CheckExpired(MPS->GetMutableActiveQuest(), NowServerTime());
			}
		}
	}
}

TStatId UMANarrativeSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UMANarrativeSubsystem, STATGROUP_Tickables);
}

void UMANarrativeSubsystem::GrantQuestReward(AMAPlayerState* PS)
{
	UAbilitySystemComponent* ASC = PS->GetAbilitySystemComponent();
	if (!ASC)
	{
		return;
	}
	const FMAQuestState& Q = PS->GetActiveQuest();

	// 奖励规则（spec）：限时→移速×1.3/60s；kill_count≥7→攻击×1.3/60s；其余→立即回血 50。
	// 运行时构造 transient GE：C++ 不引用内容资产，规则一眼可读。
	UGameplayEffect* GE = NewObject<UGameplayEffect>(GetTransientPackage(), TEXT("GE_QuestReward"));
	FGameplayModifierInfo Mod;
	if (Q.Type == EMAQuestType::TimedKill)
	{
		GE->DurationPolicy = EGameplayEffectDurationType::HasDuration;
		GE->DurationMagnitude = FGameplayEffectModifierMagnitude(FScalableFloat(60.f));
		Mod.Attribute = UMAAttributeSet::GetMoveSpeedAttribute();
		Mod.ModifierOp = EGameplayModOp::Multiplicitive;
		Mod.ModifierMagnitude = FGameplayEffectModifierMagnitude(FScalableFloat(1.3f));
	}
	else if (Q.TargetKills >= 7)
	{
		GE->DurationPolicy = EGameplayEffectDurationType::HasDuration;
		GE->DurationMagnitude = FGameplayEffectModifierMagnitude(FScalableFloat(60.f));
		Mod.Attribute = UMAAttributeSet::GetAttackPowerAttribute();
		Mod.ModifierOp = EGameplayModOp::Multiplicitive;
		Mod.ModifierMagnitude = FGameplayEffectModifierMagnitude(FScalableFloat(1.3f));
	}
	else
	{
		GE->DurationPolicy = EGameplayEffectDurationType::Instant;
		Mod.Attribute = UMAAttributeSet::GetHealthAttribute();
		Mod.ModifierOp = EGameplayModOp::Additive;
		Mod.ModifierMagnitude = FGameplayEffectModifierMagnitude(FScalableFloat(50.f));
	}
	GE->Modifiers.Add(Mod);

	FGameplayEffectContextHandle Ctx = ASC->MakeEffectContext();
	ASC->ApplyGameplayEffectToSelf(GE, 1.f, Ctx);
	UE_LOG(LogMANarrative, Display, TEXT("任务完成奖励已发放（%s）"), *PS->GetPlayerName());
}

double UMANarrativeSubsystem::NowServerTime() const
{
	const AGameStateBase* GS = GetWorld() ? GetWorld()->GetGameState() : nullptr;
	return GS ? GS->GetServerWorldTimeSeconds() : 0.0;
}
