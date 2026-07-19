#pragma once

#include "CoreMinimal.h"

/**
 * 好感度规则（adjust_favor 工具 + 各工具的好感门槛）—— 纯逻辑。
 * 好感存 AMAPlayerState（局内即弃），这里只做钳制与门槛判定。
 * 测试：MultiPlayerAction.Narrative.FavorRules
 */
namespace MAFavorRules
{
	inline constexpr int32 FavorMin = -10;
	inline constexpr int32 FavorMax = 10;
	/** 单次 adjust_favor 的 delta 钳制（模型一轮最多 ±2）。 */
	inline constexpr int32 DeltaClampAbs = 2;
	/** 好感达到该值解锁赐福与对局情报。 */
	inline constexpr int32 TrustGate = 3;
	/** 好感跌破该值剑客翻脸，所有工具拒绝执行。 */
	inline constexpr int32 MuteGate = -3;

	struct FMAAdjustFavorParams
	{
		int32 Delta = 0;
		FString Reason;
	};

	/** 解析+钳制 adjust_favor 参数。 */
	MULTIPLAYERACTION_API bool ParseAndValidateAdjustFavor(const FString& ArgsJson,
		FMAAdjustFavorParams& Out, FString& OutReject);

	/** 应用 delta 后的新好感（带边界钳制）。 */
	MULTIPLAYERACTION_API int32 ApplyDelta(int32 CurrentFavor, int32 Delta);

	inline bool IsTrusted(int32 Favor) { return Favor >= TrustGate; }
	inline bool IsMuted(int32 Favor) { return Favor <= MuteGate; }

	/** 好感值的态度描述（注入 system prompt 让模型演对戏）。 */
	MULTIPLAYERACTION_API FString DescribeAttitude(int32 Favor);
}
