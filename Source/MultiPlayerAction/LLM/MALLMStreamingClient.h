#pragma once

#include "CoreMinimal.h"
#include "Interfaces/IHttpRequest.h"
#include "LLM/MALLMTypes.h"
#include "LLM/MASSEStream.h"

/**
 * 一次流式 chat completion 请求（server 端使用）。
 *
 * 线程模型：HTTP 线程只拷贝字节并 AsyncTask 回游戏线程；解析、状态、全部回调
 * 都发生在游戏线程 —— 调用方无需加锁。
 *
 * 生命周期：Start 返回 ThreadSafe shared ptr，调用方持有；HTTP 侧回调只持 weak，
 * 调用方丢弃指针（或 Cancel）后飞行中的数据安全落空。
 */
class MULTIPLAYERACTION_API FMALLMStreamRequest : public TSharedFromThis<FMALLMStreamRequest, ESPMode::ThreadSafe>
{
public:
	struct FCallbacks
	{
		/** 每批增量文本（游戏线程）。 */
		TFunction<void(const FString&)> OnDelta;
		/** 生成完整结束，参数为全文（游戏线程）。 */
		TFunction<void(const FString&)> OnComplete;
		/** 失败，参数为可直接展示给玩家的中文信息（游戏线程）。 */
		TFunction<void(const FString&)> OnError;
	};

	/** 按调用覆盖 UMALLMSettings 的采样参数（<0/空 = 用配置值）。军师类小输出场景用。 */
	struct FOverrides
	{
		int32 MaxTokens = -1;
		float Temperature = -1.f;
		FString Model;
	};

	/**
	 * 发起流式请求。配置无效（缺 API key 等）时返回 nullptr 并填 OutError，
	 * 此时不会有任何回调。只能在游戏线程调用。
	 */
	static TSharedPtr<FMALLMStreamRequest, ESPMode::ThreadSafe> Start(
		const TArray<FMALLMMessage>& Messages, FCallbacks InCallbacks, FString& OutError,
		const FOverrides& Overrides = FOverrides());

	/** 取消：立刻停止回调（包括正在飞行的数据），随后中止 HTTP 请求。 */
	void Cancel();

	bool IsActive() const { return bActive; }

private:
	FMALLMStreamRequest() = default;

	/** 游戏线程：喂解析器、派发 delta。 */
	void ProcessBytes(TArray<uint8> Bytes);
	void HandleRequestComplete(FHttpRequestPtr Req, FHttpResponsePtr Resp, bool bConnectedOk);

	TSharedPtr<IHttpRequest, ESPMode::ThreadSafe> HttpRequest;
	FCallbacks Callbacks;
	FMASSEStreamParser Parser;

	/** 已流出的全文（OnComplete 的参数）。 */
	FString Accumulated;

	/** 末尾 chunk 捕获的 usage（-1 = 服务端未提供），完成时打进成本日志。 */
	int32 UsagePromptTokens = -1;
	int32 UsageCompletionTokens = -1;

	/** 最后见到的 finish_reason（诊断：length = 输出被 max_tokens 截断）。 */
	FString LastFinishReason;

	/** 响应体开头的原始字节（错误体诊断用；SSE 流不会走到解析它那步）。 */
	TArray<uint8> RawHead;

	bool bActive = true;
	bool bCanceled = false;
	/** OnComplete/OnError 只许发一次（[DONE] 与 RequestComplete 都可能触发收尾）。 */
	bool bFinished = false;
};
