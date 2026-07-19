#include "LLM/MALLMStreamingClient.h"

#include "LLM/MALLMRequestBody.h"
#include "LLM/MALLMSettings.h"
#include "Async/Async.h"
#include "Dom/JsonObject.h"
#include "HttpModule.h"
#include "Interfaces/IHttpResponse.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#if PLATFORM_WINDOWS
#include "Windows/WindowsHWrapper.h"
#include "Windows/WindowsPlatformMisc.h"
#endif

DEFINE_LOG_CATEGORY_STATIC(LogMALLM, Log, All);

namespace
{
	// 错误体只留头部这么多字节用于诊断，防止异常大响应吃内存。
	constexpr int32 GMaxRawHeadBytes = 8 * 1024;

}

TSharedPtr<FMALLMStreamRequest, ESPMode::ThreadSafe> FMALLMStreamRequest::Start(
	const TArray<FMALLMMessage>& Messages, FCallbacks InCallbacks, FString& OutError,
	const FOverrides& Overrides)
{
	check(IsInGameThread());

	const UMALLMSettings* S = UMALLMSettings::Get();

	FString ApiKey = FPlatformMisc::GetEnvironmentVariable(*S->ApiKeyEnvVar).TrimStartAndEnd();
#if PLATFORM_WINDOWS
	// 进程环境是启动时的快照：setx 之后没重启的父进程（或忘了注入的脚本启动链）
	// 读不到新变量。Windows 上回退现读注册表的用户级环境（setx 的落点）。
	if (ApiKey.IsEmpty())
	{
		FString RegValue;
		if (FWindowsPlatformMisc::QueryRegKey(HKEY_CURRENT_USER, TEXT("Environment"), *S->ApiKeyEnvVar, RegValue))
		{
			ApiKey = RegValue.TrimStartAndEnd();
		}
	}
#endif
	if (ApiKey.IsEmpty())
	{
		OutError = FString::Printf(TEXT("服务器未配置环境变量 %s，无法连接对话服务"), *S->ApiKeyEnvVar);
		return nullptr;
	}
	if (Messages.Num() == 0)
	{
		OutError = TEXT("对话内容为空");
		return nullptr;
	}

	TSharedPtr<FMALLMStreamRequest, ESPMode::ThreadSafe> Self =
		MakeShareable(new FMALLMStreamRequest());
	Self->Callbacks = MoveTemp(InCallbacks);

	FString BaseUrl = S->BaseUrl;
	while (BaseUrl.EndsWith(TEXT("/")))
	{
		BaseUrl.LeftChopInline(1);
	}

	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = FHttpModule::Get().CreateRequest();
	Request->SetVerb(TEXT("POST"));
	Request->SetURL(BaseUrl + TEXT("/chat/completions"));
	Request->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
	Request->SetHeader(TEXT("Authorization"), FString::Printf(TEXT("Bearer %s"), *ApiKey));
	Request->SetHeader(TEXT("Accept"), TEXT("text/event-stream"));
	Request->SetTimeout(S->TotalTimeoutSeconds);
	Request->SetActivityTimeout(S->ActivityTimeoutSeconds);
	FMALLMRequestParams BodyParams;
	BodyParams.Model = Overrides.Model.IsEmpty() ? S->Model : Overrides.Model;
	BodyParams.MaxTokens = Overrides.MaxTokens > 0 ? Overrides.MaxTokens : S->MaxTokens;
	BodyParams.bDisableThinking = S->bDisableThinking;
	if (Overrides.Temperature >= 0.f)
	{
		BodyParams.bSendTemperature = true;
		BodyParams.Temperature = Overrides.Temperature;
	}
	else if (S->bSendTemperature)
	{
		BodyParams.bSendTemperature = true;
		BodyParams.Temperature = S->Temperature;
	}
	Request->SetContentAsString(MALLM::BuildChatRequestBody(Messages, BodyParams, Overrides.Tools));

	TWeakPtr<FMALLMStreamRequest, ESPMode::ThreadSafe> WeakSelf = Self;

	// HTTP 线程回调：只拷贝字节 + marshal，不碰任何解析状态。
	Request->SetResponseBodyReceiveStreamDelegateV2(FHttpRequestStreamDelegateV2::CreateLambda(
		[WeakSelf](void* Ptr, int64& InOutLength)
		{
			if (!Ptr || InOutLength <= 0)
			{
				return;
			}
			TArray<uint8> Bytes(static_cast<const uint8*>(Ptr), static_cast<int32>(InOutLength));
			AsyncTask(ENamedThreads::GameThread, [WeakSelf, Bytes = MoveTemp(Bytes)]() mutable
			{
				if (TSharedPtr<FMALLMStreamRequest, ESPMode::ThreadSafe> Pinned = WeakSelf.Pin())
				{
					Pinned->ProcessBytes(MoveTemp(Bytes));
				}
			});
		}));

	// 默认线程策略 = 完成回调在游戏线程。
	Request->OnProcessRequestComplete().BindLambda(
		[WeakSelf](FHttpRequestPtr Req, FHttpResponsePtr Resp, bool bConnectedOk)
		{
			if (TSharedPtr<FMALLMStreamRequest, ESPMode::ThreadSafe> Pinned = WeakSelf.Pin())
			{
				Pinned->HandleRequestComplete(Req, Resp, bConnectedOk);
			}
		});

	if (!Request->ProcessRequest())
	{
		OutError = TEXT("无法发起网络请求");
		return nullptr;
	}

	Self->HttpRequest = Request;
	UE_LOG(LogMALLM, Log, TEXT("流式请求已发出：model=%s messages=%d"), *S->Model, Messages.Num());
	return Self;
}

void FMALLMStreamRequest::Cancel()
{
	check(IsInGameThread());
	if (bCanceled || bFinished)
	{
		return;
	}
	bCanceled = true;
	bActive = false;
	if (HttpRequest.IsValid())
	{
		HttpRequest->CancelRequest();
	}
}

void FMALLMStreamRequest::ProcessBytes(TArray<uint8> Bytes)
{
	if (bCanceled || bFinished)
	{
		return;
	}

	if (RawHead.Num() < GMaxRawHeadBytes)
	{
		RawHead.Append(Bytes.GetData(),
			FMath::Min(Bytes.Num(), GMaxRawHeadBytes - RawHead.Num()));
	}

	TArray<FString> Events;
	Parser.Append(Bytes.GetData(), Bytes.Num(), Events);

	for (const FString& Event : Events)
	{
		if (MAOpenAISSE::IsDoneMarker(Event))
		{
			bFinished = true;
			bActive = false;
			if (UsagePromptTokens >= 0)
			{
				UE_LOG(LogMALLM, Log, TEXT("token 用量：prompt=%d completion=%d finish=%s"),
					UsagePromptTokens, UsageCompletionTokens, *LastFinishReason);
			}
			if (Accumulated.IsEmpty() && !ToolCallAggregator.HasCalls())
			{
				UE_LOG(LogMALLM, Warning, TEXT("流结束但正文为空：finish=%s（length=输出预算被耗尽，思考型模型需更大 max_tokens）"),
					*LastFinishReason);
			}
			if (Callbacks.OnComplete)
			{
				Callbacks.OnComplete(Accumulated, ToolCallAggregator.GetCalls());
			}
			return;
		}

		const FMAOpenAIChunk Chunk = MAOpenAISSE::ParseChunk(Event);
		if (!Chunk.bValid)
		{
			UE_LOG(LogMALLM, Verbose, TEXT("忽略无法解析的流式载荷：%s"), *Event.Left(200));
			continue;
		}
		for (const FMAToolCallDelta& ToolDelta : Chunk.ToolCallDeltas)
		{
			ToolCallAggregator.Consume(ToolDelta);
		}
		if (Chunk.PromptTokens >= 0)
		{
			UsagePromptTokens = Chunk.PromptTokens;
			UsageCompletionTokens = Chunk.CompletionTokens;
		}
		if (!Chunk.FinishReason.IsEmpty())
		{
			LastFinishReason = Chunk.FinishReason;
		}
		if (!Chunk.Content.IsEmpty())
		{
			Accumulated += Chunk.Content;
			if (Callbacks.OnDelta)
			{
				Callbacks.OnDelta(Chunk.Content);
			}
		}
		// finish_reason 到达后紧跟 [DONE]/连接关闭，收尾统一走那两条路径。
	}
}

void FMALLMStreamRequest::HandleRequestComplete(FHttpRequestPtr /*Req*/, FHttpResponsePtr Resp, bool bConnectedOk)
{
	check(IsInGameThread());
	bActive = false;

	if (bCanceled || bFinished)
	{
		return;
	}
	bFinished = true;

	const int32 Code = Resp.IsValid() ? Resp->GetResponseCode() : 0;

	// 服务端正常关流但没发 [DONE]（部分兼容端点如此）——按成功收尾。
	if (bConnectedOk && Code == 200)
	{
		if (Callbacks.OnComplete)
		{
			Callbacks.OnComplete(Accumulated, ToolCallAggregator.GetCalls());
		}
		return;
	}

	// 失败路径：优先从错误体提取服务端 message。
	FString UserFacing;
	if (RawHead.Num() > 0)
	{
		const FUTF8ToTCHAR Converted(reinterpret_cast<const ANSICHAR*>(RawHead.GetData()), RawHead.Num());
		const FString ApiMessage = MAOpenAISSE::ExtractErrorMessage(FString(Converted.Length(), Converted.Get()));
		if (!ApiMessage.IsEmpty())
		{
			UserFacing = FString::Printf(TEXT("对话服务返回错误：%s"), *ApiMessage);
		}
	}
	if (UserFacing.IsEmpty())
	{
		UserFacing = Code > 0
			? FString::Printf(TEXT("对话服务异常（HTTP %d）"), Code)
			: TEXT("网络请求失败或超时");
	}

	UE_LOG(LogMALLM, Warning, TEXT("流式请求失败：code=%d connectedOk=%d msg=%s"),
		Code, bConnectedOk ? 1 : 0, *UserFacing);

	if (Callbacks.OnError)
	{
		Callbacks.OnError(UserFacing);
	}
}
