#pragma once

#include "CoreMinimal.h"

/**
 * Server-Sent Events 字节级增量解析器。
 *
 * HTTP 线程按任意 chunk 边界送来裸字节 —— 边界可能落在一行中间、甚至一个多字节
 * UTF-8 字符中间，所以必须先在字节层攒帧（空行分隔），凑齐一帧后才做 UTF-8 解码。
 * 帧内只认 data: 字段（多行 data 按 SSE 规范以 \n 连接），event:/id:/注释行忽略。
 *
 * 纯逻辑无 UObject，游戏线程独占使用（调用方负责把字节 marshal 回游戏线程）。
 * 测试：MultiPlayerAction.LLM.SSEParser
 */
class MULTIPLAYERACTION_API FMASSEStreamParser
{
public:
	/** 喂入一段字节；每凑齐一个完整 SSE 帧就向 OutEvents 追加其 data 载荷（已解码）。 */
	void Append(const uint8* Data, int64 Len, TArray<FString>& OutEvents);

	void Reset() { Buffer.Reset(); PendingDataLines.Reset(); }

private:
	/** 尚未凑成完整行的字节。 */
	TArray<uint8> Buffer;

	/** 当前帧已收到的 data 行（等待空行触发 dispatch）。 */
	TArray<FString> PendingDataLines;

	void ConsumeLine(const uint8* LineStart, int32 LineLen, TArray<FString>& OutEvents);
};

/** delta.content 为一个流式 chunk 解出的增量文本；FinishReason 非空表示生成结束。
 *  末尾 chunk 可能携带 usage 统计（-1 = 本 chunk 未携带）。 */
struct FMAOpenAIChunk
{
	FString Content;
	FString FinishReason;
	int32 PromptTokens = -1;
	int32 CompletionTokens = -1;
	bool bValid = false;
};

/**
 * OpenAI 兼容流式响应（Kimi/Moonshot 同格式）的载荷解析。
 * 测试：MultiPlayerAction.LLM.OpenAIChunk
 */
namespace MAOpenAISSE
{
	/** 流结束哨兵 "[DONE]"（非 JSON，单独判断）。 */
	MULTIPLAYERACTION_API bool IsDoneMarker(const FString& Payload);

	/** 解析一个 chunk JSON，取 choices[0].delta.content 与 finish_reason。 */
	MULTIPLAYERACTION_API FMAOpenAIChunk ParseChunk(const FString& Payload);

	/** 从 {"error":{"message":...}} 错误体提取人类可读信息；无则返回空串。 */
	MULTIPLAYERACTION_API FString ExtractErrorMessage(const FString& Body);
}
