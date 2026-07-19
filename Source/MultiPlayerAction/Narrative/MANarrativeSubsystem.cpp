#include "Narrative/MANarrativeSubsystem.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystem/MAAttributeSet.h"
#include "Dialogue/MADialogueComponent.h"
#include "GameFramework/Character.h"
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
	const UMADialogueComponent* Npc, const FMALLMToolCall& Call, FString& OutSpokenLine)
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
	SpawnQuestEnemies(PS, Npc, Params.KillCount);
	OutSpokenLine = Params.QuestLine;
	UE_LOG(LogMANarrative, Display, TEXT("已发布任务：击杀 %d，限时 %d 秒（%s）"),
		Params.KillCount, Params.TimeLimitSeconds, *PS->GetPlayerName());
	return FString::Printf(TEXT("成功：已发布击杀 %d 人的任务%s，目标已在附近现身。完成由系统自动判定并发放奖励。"),
		Params.KillCount,
		Params.TimeLimitSeconds > 0
			? *FString::Printf(TEXT("（限时 %d 秒）"), Params.TimeLimitSeconds) : TEXT(""));
}

void UMANarrativeSubsystem::SpawnQuestEnemies(AMAPlayerState* PS, const UMADialogueComponent* Npc, int32 Count)
{
	UWorld* World = GetWorld();
	const AActor* Anchor = Npc ? Npc->GetOwner() : nullptr;
	if (!World || !Anchor || !Npc->QuestEnemyClass)
	{
		return; // 未配置刷怪类 = 只发任务
	}
	TArray<TWeakObjectPtr<ACharacter>>& Ledger = QuestSpawnedEnemies.FindOrAdd(PS);

	const FVector Center = Anchor->GetActorLocation();
	const float AngleStep = 2.f * PI / FMath::Max(1, Count);
	int32 Spawned = 0;
	for (int32 i = 0; i < Count; ++i)
	{
		// 环形展开 + 半径抖动：既不叠一起也不需要 navmesh 采样这种重依赖。
		const float Angle = AngleStep * i + FMath::FRandRange(-0.3f, 0.3f);
		const float Radius = FMath::FRandRange(700.f, 1100.f);
		const FVector Location = Center + FVector(FMath::Cos(Angle) * Radius, FMath::Sin(Angle) * Radius, 100.f);

		FActorSpawnParameters SpawnParams;
		SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
		ACharacter* Enemy = World->SpawnActor<ACharacter>(Npc->QuestEnemyClass, Location,
			FRotator(0.f, FMath::FRandRange(0.f, 360.f), 0.f), SpawnParams);
		if (Enemy)
		{
			++Spawned;
			Ledger.Add(Enemy);
			if (!Enemy->GetController())
			{
				Enemy->SpawnDefaultController(); // 运行时刷出的 pawn 不吃 Placed-in-World 自动上脑
			}
		}
	}
	UE_LOG(LogMANarrative, Display, TEXT("任务刷怪：%d/%d 个 %s"),
		Spawned, Count, *Npc->QuestEnemyClass->GetName());
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
		CleanupQuestEnemies(KillerPS); // 残余目标随任务一起谢幕（PvP 击杀也计进度，可能有剩）
	}
}

void UMANarrativeSubsystem::CleanupQuestEnemies(AMAPlayerState* PS)
{
	TArray<TWeakObjectPtr<ACharacter>>* Ledger = QuestSpawnedEnemies.Find(PS);
	if (!Ledger)
	{
		return;
	}
	int32 Removed = 0;
	for (const TWeakObjectPtr<ACharacter>& Enemy : *Ledger)
	{
		if (ACharacter* Alive = Enemy.Get())
		{
			Alive->Destroy();
			++Removed;
		}
	}
	QuestSpawnedEnemies.Remove(PS);
	if (Removed > 0)
	{
		UE_LOG(LogMANarrative, Display, TEXT("任务清场：移除 %d 个残余刷怪"), Removed);
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
				if (MAQuestRules::CheckExpired(MPS->GetMutableActiveQuest(), NowServerTime()))
				{
					CleanupQuestEnemies(MPS); // 超时任务的目标一并清场
				}
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
