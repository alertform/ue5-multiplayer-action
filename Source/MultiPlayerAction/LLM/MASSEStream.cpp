#include "LLM/MASSEStream.h"

#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

void FMASSEStreamParser::Append(const uint8* Data, int64 Len, TArray<FString>& OutEvents)
{
	if (!Data || Len <= 0)
	{
		return;
	}

	Buffer.Append(Data, Len);

	// 逐行消费：每遇到 '\n' 切出一行（剥掉行尾 '\r'），剩余字节留在 Buffer 里等下一批。
	int32 LineStart = 0;
	for (int32 i = 0; i < Buffer.Num(); ++i)
	{
		if (Buffer[i] != '\n')
		{
			continue;
		}
		int32 LineLen = i - LineStart;
		if (LineLen > 0 && Buffer[LineStart + LineLen - 1] == '\r')
		{
			--LineLen;
		}
		ConsumeLine(Buffer.GetData() + LineStart, LineLen, OutEvents);
		LineStart = i + 1;
	}
	if (LineStart > 0)
	{
		Buffer.RemoveAt(0, LineStart, EAllowShrinking::No);
	}
}

void FMASSEStreamParser::ConsumeLine(const uint8* LineStart, int32 LineLen, TArray<FString>& OutEvents)
{
	// 空行 = 帧结束，dispatch 攒下的 data 行。
	if (LineLen == 0)
	{
		if (PendingDataLines.Num() > 0)
		{
			OutEvents.Add(FString::Join(PendingDataLines, TEXT("\n")));
			PendingDataLines.Reset();
		}
		return;
	}

	// 完整行才做 UTF-8 解码 —— 行边界是 ASCII '\n'，不会劈开多字节字符。
	const FUTF8ToTCHAR Converted(reinterpret_cast<const ANSICHAR*>(LineStart), LineLen);
	const FString Line(Converted.Length(), Converted.Get());

	if (Line.StartsWith(TEXT(":")))
	{
		return; // 注释 / keep-alive
	}

	// 只认 data 字段；"data:" 与 "data: " 两种写法都合法。
	if (Line.StartsWith(TEXT("data:")))
	{
		FString Payload = Line.Mid(5);
		if (Payload.StartsWith(TEXT(" ")))
		{
			Payload.RightChopInline(1);
		}
		PendingDataLines.Add(MoveTemp(Payload));
	}
	// event: / id: / retry: 等字段对本用途无意义，忽略。
}

namespace MAOpenAISSE
{

bool IsDoneMarker(const FString& Payload)
{
	return Payload.TrimStartAndEnd() == TEXT("[DONE]");
}

FMAOpenAIChunk ParseChunk(const FString& Payload)
{
	FMAOpenAIChunk Out;

	TSharedPtr<FJsonObject> Root;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Payload);
	if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
	{
		return Out;
	}

	const TArray<TSharedPtr<FJsonValue>>* Choices = nullptr;
	if (!Root->TryGetArrayField(TEXT("choices"), Choices) || Choices->Num() == 0)
	{
		return Out;
	}

	const TSharedPtr<FJsonObject>* Choice = nullptr;
	if (!(*Choices)[0]->TryGetObject(Choice))
	{
		return Out;
	}

	Out.bValid = true;

	const TSharedPtr<FJsonObject>* Delta = nullptr;
	if ((*Choice)->TryGetObjectField(TEXT("delta"), Delta))
	{
		(*Delta)->TryGetStringField(TEXT("content"), Out.Content);

		// tool_calls 增量：首帧带 index/id/name，续帧只有 arguments 片段（k2.6 实测格式）。
		const TArray<TSharedPtr<FJsonValue>>* ToolCallArr = nullptr;
		if ((*Delta)->TryGetArrayField(TEXT("tool_calls"), ToolCallArr))
		{
			for (const TSharedPtr<FJsonValue>& Val : *ToolCallArr)
			{
				const TSharedPtr<FJsonObject>* CallObj = nullptr;
				if (!Val->TryGetObject(CallObj))
				{
					continue;
				}
				FMAToolCallDelta ToolDelta;
				(*CallObj)->TryGetNumberField(TEXT("index"), ToolDelta.Index);
				(*CallObj)->TryGetStringField(TEXT("id"), ToolDelta.Id);
				const TSharedPtr<FJsonObject>* FnObj = nullptr;
				if ((*CallObj)->TryGetObjectField(TEXT("function"), FnObj))
				{
					(*FnObj)->TryGetStringField(TEXT("name"), ToolDelta.Name);
					(*FnObj)->TryGetStringField(TEXT("arguments"), ToolDelta.ArgumentsFragment);
				}
				Out.ToolCallDeltas.Add(MoveTemp(ToolDelta));
			}
		}
	}
	(*Choice)->TryGetStringField(TEXT("finish_reason"), Out.FinishReason);

	// 部分端点在末尾 chunk 附带 usage —— 有则捕获，供成本日志用。
	const TSharedPtr<FJsonObject>* Usage = nullptr;
	if (Root->TryGetObjectField(TEXT("usage"), Usage))
	{
		double Prompt = 0.0, Completion = 0.0;
		if ((*Usage)->TryGetNumberField(TEXT("prompt_tokens"), Prompt))
		{
			Out.PromptTokens = static_cast<int32>(Prompt);
		}
		if ((*Usage)->TryGetNumberField(TEXT("completion_tokens"), Completion))
		{
			Out.CompletionTokens = static_cast<int32>(Completion);
		}
	}

	return Out;
}

FString ExtractErrorMessage(const FString& Body)
{
	TSharedPtr<FJsonObject> Root;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Body);
	if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
	{
		return FString();
	}

	const TSharedPtr<FJsonObject>* Error = nullptr;
	if (!Root->TryGetObjectField(TEXT("error"), Error))
	{
		return FString();
	}

	FString Message;
	(*Error)->TryGetStringField(TEXT("message"), Message);
	return Message;
}

} // namespace MAOpenAISSE

void FMAToolCallAggregator::Consume(const FMAToolCallDelta& Delta)
{
	// 缺 index 的异常帧按 0 处理；稀疏 index 直接补位 —— 防御模型/端点的畸形输出。
	const int32 Index = FMath::Max(0, Delta.Index);
	while (Calls.Num() <= Index)
	{
		Calls.AddDefaulted();
	}
	FMALLMToolCall& Call = Calls[Index];
	if (!Delta.Id.IsEmpty())
	{
		Call.Id = Delta.Id;
	}
	if (!Delta.Name.IsEmpty())
	{
		Call.Name = Delta.Name;
	}
	Call.ArgumentsJson += Delta.ArgumentsFragment;
}
