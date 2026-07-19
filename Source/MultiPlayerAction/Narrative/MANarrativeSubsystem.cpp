#include "Narrative/MANarrativeSubsystem.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystem/MAAttributeSet.h"
#include "Dialogue/MADialogueComponent.h"
#include "GameFramework/Character.h"
#include "MAGameState.h"
#include "Narrative/MAFavorRules.h"
#include "Narrative/MAWorldEventRules.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/GameStateBase.h"
#include "GameplayEffect.h"
#include "Narrative/MAQuestRules.h"
#include "Player/MAPlayerController.h"
#include "TimerManager.h"
#include "Player/MAPlayerState.h"

DEFINE_LOG_CATEGORY_STATIC(LogMANarrative, Log, All);

namespace
{
	// 运行时构造 transient GE（C++ 无内容资产引用）：任务奖励与全场赐福共用。
	UGameplayEffect* MANarrative_MakeTimedMultiplierGE(const FGameplayAttribute& Attribute,
		float Multiplier, float DurationSeconds)
	{
		UGameplayEffect* GE = NewObject<UGameplayEffect>(GetTransientPackage(), TEXT("GE_NarrativeBuff"));
		GE->DurationPolicy = EGameplayEffectDurationType::HasDuration;
		GE->DurationMagnitude = FGameplayEffectModifierMagnitude(FScalableFloat(DurationSeconds));
		FGameplayModifierInfo Mod;
		Mod.Attribute = Attribute;
		Mod.ModifierOp = EGameplayModOp::Multiplicitive;
		Mod.ModifierMagnitude = FGameplayEffectModifierMagnitude(FScalableFloat(Multiplier));
		GE->Modifiers.Add(Mod);
		return GE;
	}

	UGameplayEffect* MANarrative_MakeInstantHealGE(float Amount)
	{
		UGameplayEffect* GE = NewObject<UGameplayEffect>(GetTransientPackage(), TEXT("GE_NarrativeHeal"));
		GE->DurationPolicy = EGameplayEffectDurationType::Instant;
		FGameplayModifierInfo Mod;
		Mod.Attribute = UMAAttributeSet::GetHealthAttribute();
		Mod.ModifierOp = EGameplayModOp::Additive;
		Mod.ModifierMagnitude = FGameplayEffectModifierMagnitude(FScalableFloat(Amount));
		GE->Modifiers.Add(Mod);
		return GE;
	}

	// 环形展开 + 半径抖动的刷怪（任务目标与敌袭共用）；返回实际刷出数并记入 OutLedger。
	int32 MANarrative_SpawnRing(UWorld* World, const FVector& Center, TSubclassOf<ACharacter> EnemyClass,
		int32 Count, TArray<TWeakObjectPtr<ACharacter>>& OutLedger)
	{
		const float AngleStep = 2.f * PI / FMath::Max(1, Count);
		int32 Spawned = 0;
		for (int32 i = 0; i < Count; ++i)
		{
			const float Angle = AngleStep * i + FMath::FRandRange(-0.3f, 0.3f);
			const float Radius = FMath::FRandRange(700.f, 1100.f);
			const FVector Location = Center + FVector(FMath::Cos(Angle) * Radius, FMath::Sin(Angle) * Radius, 100.f);

			FActorSpawnParameters SpawnParams;
			SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
			ACharacter* Enemy = World->SpawnActor<ACharacter>(EnemyClass, Location,
				FRotator(0.f, FMath::FRandRange(0.f, 360.f), 0.f), SpawnParams);
			if (Enemy)
			{
				++Spawned;
				OutLedger.Add(Enemy);
				if (!Enemy->GetController())
				{
					Enemy->SpawnDefaultController(); // 运行时刷出的 pawn 不吃 Placed-in-World 自动上脑
				}
			}
		}
		return Spawned;
	}
}

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

FMALLMToolSpec UMANarrativeSubsystem::GetTriggerRaidToolSpec()
{
	FMALLMToolSpec Spec;
	Spec.Name = TEXT("trigger_raid");
	Spec.Description = TEXT("召唤一波敌人袭击竞技场，影响场上所有人。每场最多 2 次，与其它世界事件共享冷却；对局临近结束时不可用。");
	Spec.ParametersSchemaJson = TEXT(
		"{\"type\":\"object\",\"properties\":{"
		"\"enemy_count\":{\"type\":\"integer\",\"description\":\"敌人数量，1到6\"},"
		"\"raid_line\":{\"type\":\"string\",\"description\":\"召唤敌袭时说的一句台词，不超过40字\"}"
		"},\"required\":[\"enemy_count\",\"raid_line\"]}");
	return Spec;
}

FMALLMToolSpec UMANarrativeSubsystem::GetGrantBlessingToolSpec()
{
	FMALLMToolSpec Spec;
	Spec.Name = TEXT("grant_blessing");
	Spec.Description = TEXT("为场上所有玩家赐福。每场最多 2 次，与其它世界事件共享冷却；对局临近结束时不可用。");
	Spec.ParametersSchemaJson = TEXT(
		"{\"type\":\"object\",\"properties\":{"
		"\"blessing_type\":{\"type\":\"string\",\"enum\":[\"attack\",\"speed\",\"regen\"],"
		"\"description\":\"attack=攻击+25%，speed=移速+25%，regen=立即回血（duration 忽略）\"},"
		"\"duration\":{\"type\":\"integer\",\"description\":\"持续秒数，30到180\"},"
		"\"line\":{\"type\":\"string\",\"description\":\"赐福时说的一句台词，不超过40字\"}"
		"},\"required\":[\"blessing_type\",\"duration\",\"line\"]}");
	return Spec;
}

FMALLMToolSpec UMANarrativeSubsystem::GetAdjustFavorToolSpec()
{
	FMALLMToolSpec Spec;
	Spec.Name = TEXT("adjust_favor");
	Spec.Description = TEXT("根据对话表现调整你对这名玩家的好感度。真诚/有礼/有趣加分，无礼/挑衅减分。好感高会解锁赐福与情报，好感太低你会拒绝一切帮助。");
	Spec.ParametersSchemaJson = TEXT(
		"{\"type\":\"object\",\"properties\":{"
		"\"delta\":{\"type\":\"integer\",\"description\":\"好感变化，-2 到 2，不能为 0\"},"
		"\"reason\":{\"type\":\"string\",\"description\":\"一句话理由\"}"
		"},\"required\":[\"delta\",\"reason\"]}");
	return Spec;
}

FString UMANarrativeSubsystem::ExecuteToolCall(AMAPlayerController* PC,
	const UMADialogueComponent* Npc, const FMALLMToolCall& Call, FString& OutSpokenLine)
{
	OutSpokenLine.Reset();

	AMAPlayerState* CallerPS = PC ? PC->GetPlayerState<AMAPlayerState>() : nullptr;

	// adjust_favor 永远可用 —— 被翻脸的玩家得有道歉挽回的通路。
	if (Call.Name == TEXT("adjust_favor"))
	{
		return CallerPS ? ExecuteAdjustFavor(CallerPS, Call.ArgumentsJson)
			: TEXT("失败：找不到玩家状态。");
	}

	// 防线 0：好感封杀线 —— 剑客翻脸后拒绝执行任何实质动作。
	if (CallerPS && MAFavorRules::IsMuted(CallerPS->GetNpcFavor()))
	{
		return TEXT("失败：你对此人好感过低，已拒绝为其效力（对方需先挽回好感）。");
	}

	// 防线 1：白名单分发。
	if (Call.Name == TEXT("trigger_raid"))
	{
		return ExecuteRaid(PC, Npc, Call.ArgumentsJson, OutSpokenLine);
	}
	if (Call.Name == TEXT("grant_blessing"))
	{
		// 赐福是信任的馈赠 —— 好感门槛。
		if (CallerPS && !MAFavorRules::IsTrusted(CallerPS->GetNpcFavor()))
		{
			return FString::Printf(TEXT("失败：好感不足（当前 %d，需要 ≥%d），赐福只给信得过的人。"),
				CallerPS->GetNpcFavor(), MAFavorRules::TrustGate);
		}
		return ExecuteBlessing(Call.ArgumentsJson, OutSpokenLine);
	}
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

	// 发布即 Active（任务条出现、可聊任务细节），但倒计时先不走 ——
	// DeadlineServerTime 在关窗触发的刷怪落地时刻才填（聊天时间不吃任务时限）。
	FMAQuestState Quest = MAQuestRules::MakeActiveQuest(Params, NowServerTime());
	Quest.DeadlineServerTime = 0.f;
	PS->SetActiveQuest(Quest);
	PS->IncrementQuestsIssued();

	// 离场/刷怪/倒计时统一锚定"玩家关闭对话窗"（NotifyDialogueClosed）——
	// 剑客留场陪聊到你走为止。
	FMAPendingQuestStart Pending;
	Pending.Npc = Npc;
	Pending.KillCount = Params.KillCount;
	Pending.TimeLimitSeconds = Params.TimeLimitSeconds;
	PendingQuestStarts.Add(PS, MoveTemp(Pending));
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

FString UMANarrativeSubsystem::ExecuteRaid(AMAPlayerController* PC,
	const UMADialogueComponent* Npc, const FString& ArgsJson, FString& OutSpokenLine)
{
	MAWorldEventRules::FMAWorldEventState State;
	State.RaidsUsed = RaidsUsed;
	State.BlessingsUsed = BlessingsUsed;
	State.LastEventServerTime = LastWorldEventTime;
	State.NowServerTime = NowServerTime();
	if (const AMAGameState* GS = GetWorld() ? GetWorld()->GetGameState<AMAGameState>() : nullptr)
	{
		State.MatchEndServerTime = GS->MatchEndServerTime;
	}

	MAWorldEventRules::FMARaidParams Params;
	FString Reject;
	if (!MAWorldEventRules::ParseAndValidateRaid(ArgsJson, State, Params, Reject))
	{
		UE_LOG(LogMANarrative, Log, TEXT("trigger_raid 被拒：%s"), *Reject);
		return FString::Printf(TEXT("失败：%s"), *Reject);
	}

	// 刷怪类沿用 NPC 配置；锚点优先发起对话的玩家（敌袭冲着人来），退回 NPC。
	const AActor* Anchor = (PC && PC->GetPawn()) ? static_cast<const AActor*>(PC->GetPawn())
		: (Npc ? Npc->GetOwner() : nullptr);
	if (!Anchor || !Npc || !Npc->QuestEnemyClass)
	{
		return TEXT("失败：敌袭没有可用的刷怪配置。");
	}

	TArray<TWeakObjectPtr<ACharacter>> RaidLedger;
	const int32 Spawned = MANarrative_SpawnRing(GetWorld(), Anchor->GetActorLocation(),
		Npc->QuestEnemyClass, Params.EnemyCount, RaidLedger);

	// 没被杀完的敌袭到点自然消散 —— 不永占"空场"竞技场。
	TWeakObjectPtr<UMANarrativeSubsystem> WeakThis(this);
	FTimerHandle ExpireHandle;
	GetWorld()->GetTimerManager().SetTimer(ExpireHandle,
		FTimerDelegate::CreateLambda([WeakThis, RaidLedger]()
		{
			if (!WeakThis.IsValid())
			{
				return;
			}
			int32 Removed = 0;
			for (const TWeakObjectPtr<ACharacter>& Enemy : RaidLedger)
			{
				if (ACharacter* Alive = Enemy.Get())
				{
					Alive->Destroy();
					++Removed;
				}
			}
			if (Removed > 0)
			{
				UE_LOG(LogMANarrative, Display, TEXT("敌袭消散：移除 %d 个残余"), Removed);
			}
		}),
		RaidLifetimeSeconds, false);

	++RaidsUsed;
	LastWorldEventTime = State.NowServerTime;
	OutSpokenLine = Params.RaidLine;
	Announce(FString::Printf(TEXT("敌袭！%d 名刺客现身竞技场——"), Spawned));
	UE_LOG(LogMANarrative, Display, TEXT("敌袭触发：%d/%d 个（%s）"),
		Spawned, Params.EnemyCount, *GetNameSafe(PC));
	return FString::Printf(TEXT("成功：已召唤 %d 名敌人袭击竞技场，%.0f 秒后未被消灭则自行散去。"),
		Spawned, RaidLifetimeSeconds);
}

FString UMANarrativeSubsystem::ExecuteBlessing(const FString& ArgsJson, FString& OutSpokenLine)
{
	MAWorldEventRules::FMAWorldEventState State;
	State.RaidsUsed = RaidsUsed;
	State.BlessingsUsed = BlessingsUsed;
	State.LastEventServerTime = LastWorldEventTime;
	State.NowServerTime = NowServerTime();
	if (const AMAGameState* GS = GetWorld() ? GetWorld()->GetGameState<AMAGameState>() : nullptr)
	{
		State.MatchEndServerTime = GS->MatchEndServerTime;
	}

	MAWorldEventRules::FMABlessingParams Params;
	FString Reject;
	if (!MAWorldEventRules::ParseAndValidateBlessing(ArgsJson, State, Params, Reject))
	{
		UE_LOG(LogMANarrative, Log, TEXT("grant_blessing 被拒：%s"), *Reject);
		return FString::Printf(TEXT("失败：%s"), *Reject);
	}

	UGameplayEffect* GE = nullptr;
	const TCHAR* BlessingName = TEXT("");
	switch (Params.Type)
	{
	case MAWorldEventRules::EMABlessingType::Attack:
		GE = MANarrative_MakeTimedMultiplierGE(UMAAttributeSet::GetAttackPowerAttribute(), 1.25f, Params.DurationSeconds);
		BlessingName = TEXT("攻势如虹（攻击+25%）");
		break;
	case MAWorldEventRules::EMABlessingType::Speed:
		GE = MANarrative_MakeTimedMultiplierGE(UMAAttributeSet::GetMoveSpeedAttribute(), 1.25f, Params.DurationSeconds);
		BlessingName = TEXT("身轻如燕（移速+25%）");
		break;
	case MAWorldEventRules::EMABlessingType::Regen:
		GE = MANarrative_MakeInstantHealGE(50.f);
		BlessingName = TEXT("气血调息（立即回复）");
		break;
	}

	int32 Blessed = 0;
	if (const AGameStateBase* GS = GetWorld() ? GetWorld()->GetGameState() : nullptr)
	{
		for (APlayerState* PS : GS->PlayerArray)
		{
			const AMAPlayerState* MPS = Cast<AMAPlayerState>(PS);
			UAbilitySystemComponent* ASC = MPS ? MPS->GetAbilitySystemComponent() : nullptr;
			if (ASC && GE)
			{
				FGameplayEffectContextHandle Ctx = ASC->MakeEffectContext();
				ASC->ApplyGameplayEffectToSelf(GE, 1.f, Ctx);
				++Blessed;
			}
		}
	}

	++BlessingsUsed;
	LastWorldEventTime = State.NowServerTime;
	OutSpokenLine = Params.Line;
	Announce(FString::Printf(TEXT("剑客赐福全场：%s"), BlessingName));
	UE_LOG(LogMANarrative, Display, TEXT("赐福生效：%s，覆盖 %d 名玩家"), BlessingName, Blessed);
	return FString::Printf(TEXT("成功：全场 %d 名玩家获得%s。"), Blessed, BlessingName);
}

FString UMANarrativeSubsystem::ExecuteAdjustFavor(AMAPlayerState* PS, const FString& ArgsJson)
{
	MAFavorRules::FMAAdjustFavorParams Params;
	FString Reject;
	if (!MAFavorRules::ParseAndValidateAdjustFavor(ArgsJson, Params, Reject))
	{
		UE_LOG(LogMANarrative, Log, TEXT("adjust_favor 被拒：%s"), *Reject);
		return FString::Printf(TEXT("失败：%s"), *Reject);
	}
	const int32 NewFavor = MAFavorRules::ApplyDelta(PS->GetNpcFavor(), Params.Delta);
	PS->SetNpcFavor(NewFavor);
	UE_LOG(LogMANarrative, Display, TEXT("好感调整 %+d（%s）→ %d：%s"),
		Params.Delta, *PS->GetPlayerName(), NewFavor, *Params.Reason);
	return FString::Printf(TEXT("成功：好感 %+d，当前 %d（%s）。"),
		Params.Delta, NewFavor, *MAFavorRules::DescribeAttitude(NewFavor));
}

FString UMANarrativeSubsystem::BuildNarrativeContext(const AMAPlayerController* PC) const
{
	const AMAPlayerState* PS = PC ? PC->GetPlayerState<AMAPlayerState>() : nullptr;
	if (!PS)
	{
		return FString();
	}
	const int32 Favor = PS->GetNpcFavor();
	FString Context = FString::Printf(TEXT("\n当前玩家任务状态：%s。你对此玩家好感度 %d（%s）。"),
		*DescribeQuestState(PC), Favor, *MAFavorRules::DescribeAttitude(Favor));

	// 好感达标才把真实对局数据递给模型 —— 情报是信任的奖励，不做成工具，
	// 模型拿到数据后在台词里自然转述。
	if (MAFavorRules::IsTrusted(Favor))
	{
		const AMAGameState* GS = GetWorld() ? GetWorld()->GetGameState<AMAGameState>() : nullptr;
		if (GS)
		{
			const AMAPlayerState* Leader = nullptr;
			for (APlayerState* Other : GS->PlayerArray)
			{
				const AMAPlayerState* MPS = Cast<AMAPlayerState>(Other);
				if (MPS && (!Leader || MPS->GetKills() > Leader->GetKills()))
				{
					Leader = MPS;
				}
			}
			const int32 Remain = GS->MatchEndServerTime > 0.f
				? FMath::Max(0, FMath::FloorToInt(GS->MatchEndServerTime - NowServerTime())) : -1;
			Context += FString::Printf(TEXT("你掌握的对局情报（可向他透露）：领先者是「%s」（%d 杀，目标 %d 杀）；"
				"该玩家自己 %d 杀 %d 死%s。"),
				Leader ? *Leader->GetPlayerName() : TEXT("无"),
				Leader ? Leader->GetKills() : 0,
				GS->KillTarget,
				PS->GetKills(), PS->GetDeaths(),
				Remain >= 0 ? *FString::Printf(TEXT("；对局还剩约 %d 秒"), Remain) : TEXT(""));
		}
	}
	return Context;
}

void UMANarrativeSubsystem::Announce(const FString& Text)
{
	if (AMAGameState* GS = GetWorld() ? GetWorld()->GetGameState<AMAGameState>() : nullptr)
	{
		GS->Multicast_OnNarrativeAnnounce(Text);
	}
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
		Announce(FString::Printf(TEXT("「%s」完成了剑客的委托，奖励已发放。"), *KillerPS->GetPlayerName()));
		HandleQuestTerminal(KillerPS);
	}
}

void UMANarrativeSubsystem::NotifyDialogueClosed(AMAPlayerController* PC)
{
	AMAPlayerState* PS = PC ? PC->GetPlayerState<AMAPlayerState>() : nullptr;
	if (!PS)
	{
		return;
	}
	FMAPendingQuestStart Pending;
	if (!PendingQuestStarts.RemoveAndCopyValue(PS, Pending))
	{
		return;
	}
	// 任务在关窗前已终结（极端：PvP 击杀秒完成）—— 不再启动。
	if (PS->GetActiveQuest().Phase != EMAQuestPhase::Active)
	{
		return;
	}

	// 此刻剑客才拂袖而去；任务终结时回归（HandleQuestTerminal）。
	const UMADialogueComponent* Npc = Pending.Npc.Get();
	if (AActor* NpcOwner = Npc ? Npc->GetOwner() : nullptr)
	{
		QuestGiverByPlayer.Add(PS, NpcOwner);
		SetQuestGiverAway(NpcOwner, true);
		Announce(TEXT("云游剑客隐入风中，猎物将至……"));
	}

	// 离场与敌人现身之间停顿几秒 —— 叙事呼吸感；未来在此挂现身特效/预警圈。
	// 刷怪落地同时给限时任务上表。
	TWeakObjectPtr<UMANarrativeSubsystem> WeakThis(this);
	TWeakObjectPtr<AMAPlayerState> WeakPS(PS);
	TWeakObjectPtr<const UMADialogueComponent> WeakNpc(Pending.Npc);
	const int32 SpawnCount = Pending.KillCount;
	const int32 TimeLimit = Pending.TimeLimitSeconds;
	FTimerHandle& Handle = PendingSpawnTimers.FindOrAdd(PS);
	GetWorld()->GetTimerManager().SetTimer(Handle,
		FTimerDelegate::CreateLambda([WeakThis, WeakPS, WeakNpc, SpawnCount, TimeLimit]()
		{
			UMANarrativeSubsystem* Self = WeakThis.Get();
			AMAPlayerState* Player = WeakPS.Get();
			if (!Self || !Player)
			{
				return;
			}
			Self->PendingSpawnTimers.Remove(Player);
			Self->SpawnQuestEnemies(Player, WeakNpc.Get(), SpawnCount);
			FMAQuestState& Quest = Player->GetMutableActiveQuest();
			if (Quest.Phase == EMAQuestPhase::Active && TimeLimit > 0)
			{
				Quest.DeadlineServerTime = static_cast<float>(Self->NowServerTime() + TimeLimit);
			}
			Self->Announce(TimeLimit > 0
				? FString::Printf(TEXT("「%s」的猎物现身：%d 个目标，限时 %d 秒！"),
					*Player->GetPlayerName(), SpawnCount, TimeLimit)
				: FString::Printf(TEXT("「%s」的猎物现身：%d 个目标！"),
					*Player->GetPlayerName(), SpawnCount));
		}),
		EnemySpawnDelaySeconds, false);
}

void UMANarrativeSubsystem::HandleQuestTerminal(AMAPlayerState* PS)
{
	// 撤销还没触发的延迟刷怪与待启动记录（终结后不该再有任何东西冒出来）。
	if (FTimerHandle* Pending = PendingSpawnTimers.Find(PS))
	{
		GetWorld()->GetTimerManager().ClearTimer(*Pending);
		PendingSpawnTimers.Remove(PS);
	}
	PendingQuestStarts.Remove(PS);

	CleanupQuestEnemies(PS); // 残余目标随任务一起谢幕（PvP 击杀也计进度，可能有剩）

	// 清场之后停顿几秒剑客再归位 —— 未来在此挂回归动画/特效。
	TWeakObjectPtr<AActor> Giver;
	if (QuestGiverByPlayer.RemoveAndCopyValue(PS, Giver))
	{
		TWeakObjectPtr<UMANarrativeSubsystem> WeakThis(this);
		FTimerHandle ReturnHandle;
		GetWorld()->GetTimerManager().SetTimer(ReturnHandle,
			FTimerDelegate::CreateLambda([WeakThis, Giver]()
			{
				UMANarrativeSubsystem* Self = WeakThis.Get();
				AActor* NpcOwner = Giver.Get();
				if (Self && NpcOwner)
				{
					Self->SetQuestGiverAway(NpcOwner, false);
					if (!NpcOwner->IsHidden())
					{
						Self->Announce(TEXT("云游剑客归来了。"));
					}
				}
			}),
			GiverReturnDelaySeconds, false);
	}
}

void UMANarrativeSubsystem::SetQuestGiverAway(AActor* NpcOwner, bool bAway)
{
	int32& Refs = QuestGiverAwayRefs.FindOrAdd(NpcOwner);
	Refs += bAway ? 1 : -1;
	const bool bShouldHide = Refs > 0;
	if (Refs <= 0)
	{
		QuestGiverAwayRefs.Remove(NpcOwner);
	}
	if (NpcOwner->IsHidden() != bShouldHide)
	{
		NpcOwner->SetActorHiddenInGame(bShouldHide);
		NpcOwner->SetActorEnableCollision(!bShouldHide);
		UE_LOG(LogMANarrative, Display, TEXT("任务发布者%s：%s"),
			bShouldHide ? TEXT("离场") : TEXT("回归"), *NpcOwner->GetName());
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
					Announce(FString::Printf(TEXT("「%s」的委托超时了……"), *MPS->GetPlayerName()));
					HandleQuestTerminal(MPS); // 超时：清场 + 剑客回归
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
