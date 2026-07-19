#pragma once

#include "CoreMinimal.h"
#include "MAQuestTypes.generated.h"

UENUM(BlueprintType)
enum class EMAQuestType : uint8
{
	None,
	KillCount,
	TimedKill,
};

UENUM(BlueprintType)
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
 * BlueprintType/BlueprintReadOnly：lua UI（任务日志）只读展示用 —— 表现层可见，权威只在服务器。
 */
USTRUCT(BlueprintType)
struct FMAQuestState
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Quest")
	EMAQuestType Type = EMAQuestType::None;

	UPROPERTY(BlueprintReadOnly, Category = "Quest")
	EMAQuestPhase Phase = EMAQuestPhase::None;

	UPROPERTY(BlueprintReadOnly, Category = "Quest")
	int32 TargetKills = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Quest")
	int32 Progress = 0;

	/** 0 = 不限时。 */
	UPROPERTY(BlueprintReadOnly, Category = "Quest")
	float DeadlineServerTime = 0.f;
};

/** give_quest 钳制后的合法参数。 */
struct FMAGiveQuestParams
{
	int32 KillCount = 0;
	int32 TimeLimitSeconds = 0;
	FString QuestLine;
};
