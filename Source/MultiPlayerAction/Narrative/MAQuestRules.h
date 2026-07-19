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
	/** 剧情模式（对局不重启）下形同不限；真正的节流是单任务门槛 + NPC 自己的分寸。
	 *  若切回 PvP 竞技模式（限时对局），收紧回个位数。 */
	inline constexpr int32 MaxQuestsPerMatch = 99;
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

	/**
	 * 重开对话时的状态感知开场白（固定台词，零 LLM 成本）。
	 * 任务状态已复制，客户端展示与服务器历史用同一函数计算 —— 两端必然一致。
	 * 无任务返回 DefaultGreeting 原样。
	 */
	MULTIPLAYERACTION_API FString MakeReturnGreeting(const FMAQuestState& Quest, const FString& DefaultGreeting);
}
