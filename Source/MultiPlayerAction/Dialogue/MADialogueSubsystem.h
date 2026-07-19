#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "LLM/MALLMTypes.h"
#include "MADialogueSubsystem.generated.h"

class AMAPlayerController;
class UMADialogueComponent;
class FMALLMStreamRequest;

/**
 * 服务器端 NPC 对话会话管理 —— 本项目 LLM 代理架构的核心：
 * 只有 server 持 API key 调 Kimi；SSE 增量在这里合批（DeltaFlushIntervalSeconds），
 * 经 owning-client reliable RPC 顺序推给发起玩家。客户端永远不接触 key。
 *
 * 每个 PlayerController 至多一个会话（玩家同时只跟一个 NPC 说话）；
 * 会话历史留在 server 内存，玩家换 NPC 或断线即销毁。
 */
UCLASS()
class MULTIPLAYERACTION_API UMADialogueSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	/** server：玩家发起与 NPC 的对话。验证距离，重置会话历史为该 NPC 的 persona。 */
	void StartSession(AMAPlayerController* PC, UMADialogueComponent* Npc);

	/** server：玩家发来一条消息 → 追加历史并发起流式请求（旧请求自动打断）。 */
	void SendPlayerMessage(AMAPlayerController* PC, const FString& Text);

	/** server：玩家关闭对话窗 → 取消飞行中请求并丢弃会话。 */
	void EndSession(AMAPlayerController* PC);

	/** server：该玩家是否有进行中的对话会话（AI 目标选择用它豁免对话中的玩家）。 */
	bool IsInDialogue(const AMAPlayerController* PC) const;

	// UTickableWorldSubsystem
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual void Deinitialize() override;

private:
	struct FSession
	{
		TWeakObjectPtr<UMADialogueComponent> Npc;
		/** [0] 恒为 system persona。 */
		TArray<FMALLMMessage> History;
		TSharedPtr<FMALLMStreamRequest, ESPMode::ThreadSafe> ActiveRequest;

		/** 攒到 flush 间隔一次性 RPC 出去的增量。 */
		FString PendingDeltas;
		/** 本条回复已流出的全文（打断时以此落历史）。 */
		FString StreamedSoFar;
		/** 客户端用来丢弃打断后迟到增量的序号。 */
		int32 MessageId = 0;
		float FlushAccum = 0.f;

		/** 窗口已关但请求仍在飞行：让工具调用（发任务）执行完，完成后再销毁会话。
		 *  没有这个标记时，关窗=取消请求=玩家"以为接到了任务"（实测两次踩坑）。 */
		bool bWindowClosed = false;

		// ---- 回声剔除：k2.6 偶发在正文开头原样复述玩家的话（prompt 禁令压不净）----
		/** 本轮玩家消息原文（回声比对基准）。 */
		FString LastPlayerMessage;
		/** 回声判定是否已出结果；未出结果前增量被扣着不发。 */
		bool bEchoResolved = false;
		/** 判定为回声时，正文开头要跳过的字符数。 */
		int32 EchoSkipChars = 0;
		/** 剥掉回声后吃掉紧随的空白/换行，见到实字符即停。 */
		bool bStripLeadingWs = false;
	};

	TMap<TWeakObjectPtr<AMAPlayerController>, FSession> Sessions;

	FSession* FindSession(AMAPlayerController* PC);
	void FlushDeltas(AMAPlayerController* PC, FSession& Session);
	/** 打断飞行中的请求；已流出的部分按 assistant 消息落历史（上下文保持一致）。 */
	void InterruptActiveRequest(FSession& Session);
	void TrimHistory(FSession& Session);
};
