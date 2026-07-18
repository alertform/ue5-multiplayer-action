#pragma once

#include "CoreMinimal.h"
#include "MAQuestTypes.generated.h"

UENUM()
enum class EMAQuestType : uint8
{
	None,
	KillCount,
	TimedKill,
};

UENUM()
enum class EMAQuestPhase : uint8
{
	None,
	Active,
	Completed,
	Failed,
};

/**
 * 一个玩家的当前任务。挂 AMAPlayerState 整结构复制；服务器写，客户端 HUD 轮询读。
 * DeadlineServerTime 基于 GetServerWorldTimeSeconds 时基，两端可直接比较。
 */
USTRUCT()
struct FMAQuestState
{
	GENERATED_BODY()

	UPROPERTY()
	EMAQuestType Type = EMAQuestType::None;

	UPROPERTY()
	EMAQuestPhase Phase = EMAQuestPhase::None;

	UPROPERTY()
	int32 TargetKills = 0;

	UPROPERTY()
	int32 Progress = 0;

	/** 0 = 不限时。 */
	UPROPERTY()
	float DeadlineServerTime = 0.f;
};

/** give_quest 钳制后的合法参数。 */
struct FMAGiveQuestParams
{
	int32 KillCount = 0;
	int32 TimeLimitSeconds = 0;
	FString QuestLine;
};
