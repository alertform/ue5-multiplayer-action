#pragma once

#include "CoreMinimal.h"
#include "Narrative/MAQuestTypes.h"

/**
 * give_quest 的校验与任务状态机 —— 纯逻辑，LLM 输出按不可信输入处理。
 * 平衡数值全部收口在这里的常量，调整只动一处。
 * 测试：MultiPlayerAction.Narrative.QuestRules
 */
namespace MAQuestRules
{
	inline constexpr int32 MaxQuestsPerMatch = 3;
	inline constexpr int32 KillCountMin = 1;
	inline constexpr int32 KillCountMax = 10;
	inline constexpr int32 TimeLimitMin = 60;
	inline constexpr int32 TimeLimitMax = 600;

	/** 解析+钳制+门槛。拒绝时 OutReject 为回填给模型的中文原因。 */
	MULTIPLAYERACTION_API bool ParseAndValidateGiveQuest(const FString& ArgsJson,
		const FMAQuestState& Current, int32 IssuedThisMatch,
		FMAGiveQuestParams& Out, FString& OutReject);

	MULTIPLAYERACTION_API FMAQuestState MakeActiveQuest(const FMAGiveQuestParams& Params, double NowServerTime);

	enum class EMAQuestKillResult : uint8 { NotTracking, Progressed, JustCompleted };

	/** 击杀推进：非 Active 或已过期限一律 NotTracking（过期标记由 CheckExpired 负责）。 */
	MULTIPLAYERACTION_API EMAQuestKillResult ApplyKill(FMAQuestState& Quest, double NowServerTime);

	/** 限时任务过期检测；首次过期置 Failed 并返回 true（调用方触发一次性反馈）。 */
	MULTIPLAYERACTION_API bool CheckExpired(FMAQuestState& Quest, double NowServerTime);
}
