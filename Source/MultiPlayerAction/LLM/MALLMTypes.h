#pragma once

#include "CoreMinimal.h"

/** OpenAI 兼容消息角色。 */
enum class EMALLMRole : uint8
{
	System,
	User,
	Assistant,
};

/** 一条对话消息（server 端会话历史的最小单元）。 */
struct FMALLMMessage
{
	EMALLMRole Role = EMALLMRole::User;
	FString Content;

	FMALLMMessage() = default;
	FMALLMMessage(EMALLMRole InRole, FString InContent)
		: Role(InRole), Content(MoveTemp(InContent))
	{
	}
};

namespace MALLM
{
	inline const TCHAR* RoleToString(EMALLMRole Role)
	{
		switch (Role)
		{
		case EMALLMRole::System:    return TEXT("system");
		case EMALLMRole::Assistant: return TEXT("assistant");
		default:                    return TEXT("user");
		}
	}
}
