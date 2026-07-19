#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "LLM/MALLMTypes.h"
#include "Narrative/MAQuestTypes.h"
#include "MANarrativeSubsystem.generated.h"

class ACharacter;
class AMAPlayerController;
class AMAPlayerState;
class UMADialogueComponent;

/**
 * LLM 叙事动作的服务器权威执行器（Phase 1：give_quest）。
 * LLM 的 tool_calls 按不可信输入过 MAQuestRules 校验管线；拒绝原因回填模型，
 * 台词层面由模型自然圆场 —— 玩家永远不见系统报错。
 * 只应在 server world 使用（对话子系统本身就是 server-only）。
 */
UCLASS()
class MULTIPLAYERACTION_API UMANarrativeSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	/** give_quest 的 OpenAI 工具声明（schema 与 MAQuestRules 钳制范围一致）。 */
	static FMALLMToolSpec GetGiveQuestToolSpec();

	/** 执行一个工具调用；返回回填给模型的结果文本。接受 give_quest 时 OutSpokenLine=quest_line。
	 *  Npc 提供刷怪配置（QuestEnemyClass）与刷怪锚点位置，可为 null（不刷怪只发任务）。 */
	FString ExecuteToolCall(AMAPlayerController* PC, const UMADialogueComponent* Npc,
		const FMALLMToolCall& Call, FString& OutSpokenLine);

	/** GameMode 击杀路由：推进 killer 的任务进度，完成即发奖励。 */
	void NotifyKill(AMAPlayerState* KillerPS);

	/** 对话 system prompt 用的任务状态一句话（"无任务" / "进行中 x/y 剩 z 秒"…）。 */
	FString DescribeQuestState(const AMAPlayerController* PC) const;

	// UTickableWorldSubsystem —— 限时任务过期扫描（1s 节流）。
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;

private:
	void GrantQuestReward(AMAPlayerState* PS);
	/** 发任务时绕锚点环形刷出任务目标（SpawnDefaultController 保证 AI 上脑），并记账以便清场。 */
	void SpawnQuestEnemies(AMAPlayerState* PS, const UMADialogueComponent* Npc, int32 Count);
	/** 任务终结（完成/超时）时清掉该玩家残余的任务刷怪 —— 竞技场无任务时保持空场。 */
	void CleanupQuestEnemies(AMAPlayerState* PS);
	double NowServerTime() const;
	float ExpirySweepAccum = 0.f;

	/** 每玩家在场的任务刷怪（弱引用；被杀的自然失效）。 */
	TMap<TWeakObjectPtr<AMAPlayerState>, TArray<TWeakObjectPtr<ACharacter>>> QuestSpawnedEnemies;
};
