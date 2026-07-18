#pragma once

#include "CoreMinimal.h"
#include "LLM/MALLMTypes.h"

/** 请求采样/端点参数（与 UMALLMSettings 解耦，纯逻辑可单测）。 */
struct FMALLMRequestParams
{
	FString Model;
	int32 MaxTokens = 512;
	/** kimi 全系只接受服务端默认 temperature，默认省缺该字段。 */
	bool bSendTemperature = false;
	float Temperature = 1.f;
};

namespace MALLM
{
	/** 组 chat/completions 流式请求体。Tools 为空则不写 tools 字段。
	 *  测试：MultiPlayerAction.LLM.RequestBody */
	MULTIPLAYERACTION_API FString BuildChatRequestBody(
		const TArray<FMALLMMessage>& Messages,
		const FMALLMRequestParams& Params,
		const TArray<FMALLMToolSpec>& Tools);
}
