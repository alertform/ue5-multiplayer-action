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
enum class EMANarrativeFX : uint8;

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

	/** trigger_raid / grant_blessing 的工具声明（schema 与 MAWorldEventRules 一致）。 */
	static FMALLMToolSpec GetTriggerRaidToolSpec();
	static FMALLMToolSpec GetGrantBlessingToolSpec();

	/** adjust_favor 的工具声明（schema 与 MAFavorRules 一致）。 */
	static FMALLMToolSpec GetAdjustFavorToolSpec();

	/** system prompt 的叙事上下文：任务状态 + 好感态度 + （好感达标时）真实对局情报。 */
	FString BuildNarrativeContext(const AMAPlayerController* PC) const;

	/** 执行一个工具调用；返回回填给模型的结果文本。接受 give_quest 时 OutSpokenLine=quest_line。
	 *  Npc 提供刷怪配置（QuestEnemyClass）与刷怪锚点位置，可为 null（不刷怪只发任务）。 */
	FString ExecuteToolCall(AMAPlayerController* PC, const UMADialogueComponent* Npc,
		const FMALLMToolCall& Call, FString& OutSpokenLine);

	/** GameMode 击杀路由：推进 killer 的任务进度，完成即发奖励。 */
	void NotifyKill(AMAPlayerState* KillerPS);

	/** 对话窗关闭：若该玩家有"待启动"的任务，此刻才让剑客离场并起 2.5s 刷怪节拍，
	 *  限时任务的倒计时也从这里的刷怪落地时刻起算 —— 聊天时间不吃任务时限。 */
	void NotifyDialogueClosed(AMAPlayerController* PC);

	/** 对话 system prompt 用的任务状态一句话（"无任务" / "进行中 x/y 剩 z 秒"…）。 */
	FString DescribeQuestState(const AMAPlayerController* PC) const;

	// UTickableWorldSubsystem —— 限时任务过期扫描（1s 节流）。
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;

private:
	/** 全场字幕公告（server → GameState multicast → 每台机器的字幕条）。 */
	void Announce(const FString& Text);

	/** 叙事特效落点广播（server → GameState multicast → 各机器 DialogueComponent 本地播）。 */
	void BroadcastFX(EMANarrativeFX Type, const TArray<FVector>& Locations);

	FString ExecuteRaid(AMAPlayerController* PC, const UMADialogueComponent* Npc,
		const FString& ArgsJson, FString& OutSpokenLine);
	FString ExecuteBlessing(const FString& ArgsJson, FString& OutSpokenLine);
	FString ExecuteAdjustFavor(AMAPlayerState* PS, const FString& ArgsJson);

	void GrantQuestReward(AMAPlayerState* PS);
	/** 发任务时绕锚点环形刷出任务目标（SpawnDefaultController 保证 AI 上脑），并记账以便清场。 */
	void SpawnQuestEnemies(AMAPlayerState* PS, const UMADialogueComponent* Npc, int32 Count);
	/** 任务终结（完成/超时）时清掉该玩家残余的任务刷怪 —— 竞技场无任务时保持空场。 */
	void CleanupQuestEnemies(AMAPlayerState* PS);
	/** 任务终结统一入口：清残余刷怪 + 让离场的任务发布者回归。 */
	void HandleQuestTerminal(AMAPlayerState* PS);
	/** 接单后 NPC 离场（隐身+关碰撞），bHidden 复制到客户端；引用计数防多玩家任务互踩。 */
	void SetQuestGiverAway(AActor* NpcOwner, bool bAway);
	double NowServerTime() const;
	float ExpirySweepAccum = 0.f;

	/** 每玩家在场的任务刷怪（弱引用；被杀的自然失效）。 */
	TMap<TWeakObjectPtr<AMAPlayerState>, TArray<TWeakObjectPtr<ACharacter>>> QuestSpawnedEnemies;

	/** 每玩家进行中任务的发布者（终结时据此让 NPC 回归）。 */
	TMap<TWeakObjectPtr<AMAPlayerState>, TWeakObjectPtr<AActor>> QuestGiverByPlayer;

	/** NPC 离场引用计数：多个玩家同一 NPC 接单时，最后一单终结才回归。 */
	TMap<TWeakObjectPtr<AActor>, int32> QuestGiverAwayRefs;

	/** 叙事节拍：离场→现身 / 清场→回归 之间的呼吸感停顿（未来特效/动画的挂点）。 */
	static constexpr float EnemySpawnDelaySeconds = 2.5f;
	static constexpr float GiverReturnDelaySeconds = 3.f;

	/** 延迟中的刷怪定时器；任务在延迟内终结（极端：1 杀任务被 PvP 秒完成）时撤销。 */
	TMap<TWeakObjectPtr<AMAPlayerState>, FTimerHandle> PendingSpawnTimers;

	/** 已发布但等玩家关窗才启动的任务（离场/刷怪/倒计时全部锚定关窗时刻）。 */
	struct FMAPendingQuestStart
	{
		TWeakObjectPtr<const UMADialogueComponent> Npc;
		int32 KillCount = 0;
		int32 TimeLimitSeconds = 0;
	};
	TMap<TWeakObjectPtr<AMAPlayerState>, FMAPendingQuestStart> PendingQuestStarts;

	// ---- 世界事件（trigger_raid / grant_blessing）对局账本 ----
	int32 RaidsUsed = 0;
	int32 BlessingsUsed = 0;
	double LastWorldEventTime = -1.0e9;

	/** 敌袭刷怪的自然消散时限（没被杀完也不永占竞技场）。 */
	static constexpr float RaidLifetimeSeconds = 90.f;
};
