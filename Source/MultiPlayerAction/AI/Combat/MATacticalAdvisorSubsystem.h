#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "MATacticalAdvisorSubsystem.generated.h"

class AMATargetDummy;
class APawn;
class FMALLMStreamRequest;

/**
 * LLM 战术军师（server-only）：每 ma.AI.Advisor.Interval 秒把战况摘要发给 Kimi，
 * 拿回结构化决策（进攻名额/集火目标/嘲讽），消毒后写进战斗系统：
 *   - max_attackers → UMACombatDirectorSubsystem 的名额覆盖（用户 cvar=0 的和平模式优先）
 *   - focus         → BTService_UpdateTargetInfo 的优选目标（无效/对话中/死亡自动回退）
 *   - taunt         → AMAGameState::Multicast_OnTaunt → 击杀 feed 猩红行
 *
 * 分层原则：LLM 只做认知层建议，BT 反射层照常运转 —— 请求失败/超时/胡说八道时
 * 一切回退到默认策略，永不阻塞任何一帧。复用对话同一条 server 代理链路（key 不出服务器）。
 */
UCLASS()
class MULTIPLAYERACTION_API UMATacticalAdvisorSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;
	virtual void Deinitialize() override;

	/** 军师当前指定的集火目标；无 / 已失效则为 null。BT 目标服务在候选校验后采纳。 */
	APawn* GetFocusPawn() const { return FocusPawn.Get(); }

private:
	float Accum = 0.f;
	TSharedPtr<FMALLMStreamRequest, ESPMode::ThreadSafe> ActiveRequest;
	TWeakObjectPtr<APawn> FocusPawn;

	/** 组战况摘要；无仗可打（没有存活敌人或可攻击玩家）返回空串。 */
	FString BuildBattleSummary() const;
	void ApplyDecisionText(const FString& LLMText);
};
