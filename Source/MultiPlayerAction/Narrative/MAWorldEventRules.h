#pragma once

#include "CoreMinimal.h"

/**
 * 世界事件（trigger_raid / grant_blessing）的校验规则 —— 纯逻辑，LLM 输出按不可信输入处理。
 * 平衡数值收口在常量；预算/冷却状态由叙事子系统持有，这里只判定。
 * 测试：MultiPlayerAction.Narrative.WorldEventRules
 */
namespace MAWorldEventRules
{
	inline constexpr int32 MaxRaidsPerMatch = 2;
	inline constexpr int32 MaxBlessingsPerMatch = 2;
	/** 两个世界事件（不分种类）之间的最小间隔。 */
	inline constexpr float EventCooldownSeconds = 60.f;
	/** 对局最后这段时间禁发世界事件（结算前不添乱）。 */
	inline constexpr float MatchEndLockoutSeconds = 60.f;
	inline constexpr int32 RaidCountMin = 1;
	inline constexpr int32 RaidCountMax = 6;
	inline constexpr int32 BlessingDurationMin = 30;
	inline constexpr int32 BlessingDurationMax = 180;

	/** 校验时刻的世界状态快照（子系统填好递进来）。 */
	struct FMAWorldEventState
	{
		int32 RaidsUsed = 0;
		int32 BlessingsUsed = 0;
		/** 上一个世界事件的服务器时刻；从未发过 = 一个足够远的过去。 */
		double LastEventServerTime = -1.0e9;
		double NowServerTime = 0.0;
		/** 对局钟到点时刻；<=0 = 对局未开始/无限时（不触发末段锁定）。 */
		double MatchEndServerTime = -1.0;
	};

	struct FMARaidParams
	{
		int32 EnemyCount = 0;
		FString RaidLine;
	};

	enum class EMABlessingType : uint8 { Attack, Speed, Regen };

	struct FMABlessingParams
	{
		EMABlessingType Type = EMABlessingType::Attack;
		int32 DurationSeconds = 60;
		FString Line;
	};

	MULTIPLAYERACTION_API bool ParseAndValidateRaid(const FString& ArgsJson,
		const FMAWorldEventState& State, FMARaidParams& Out, FString& OutReject);

	MULTIPLAYERACTION_API bool ParseAndValidateBlessing(const FString& ArgsJson,
		const FMAWorldEventState& State, FMABlessingParams& Out, FString& OutReject);
}
