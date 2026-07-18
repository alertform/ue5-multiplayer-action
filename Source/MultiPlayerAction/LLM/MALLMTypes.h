#pragma once

#include "CoreMinimal.h"

/** OpenAI 兼容消息角色。 */
enum class EMALLMRole : uint8
{
	System,
	User,
	Assistant,
	/** 工具执行结果回填（必须紧跟带 tool_calls 的 assistant 消息，携带 ToolCallId）。 */
	Tool,
};

/** 一次工具调用（模型输出侧；ArgumentsJson 为模型生成的原始 JSON 字符串，未经校验）。 */
struct FMALLMToolCall
{
	FString Id;
	FString Name;
	FString ArgumentsJson;
};

/** 随请求声明的一个工具。ParametersSchemaJson 为 JSON Schema 对象的字符串形式。 */
struct FMALLMToolSpec
{
	FString Name;
	FString Description;
	FString ParametersSchemaJson;
};

/** 一条对话消息（server 端会话历史的最小单元）。 */
struct FMALLMMessage
{
	EMALLMRole Role = EMALLMRole::User;
	FString Content;

	/** Role==Tool 时必填：对应 assistant 消息里的 tool call id。 */
	FString ToolCallId;

	/** Role==Assistant 且该轮发生了工具调用时必填（OpenAI 协议要求原样回填）。 */
	TArray<FMALLMToolCall> ToolCalls;

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
		case EMALLMRole::Tool:      return TEXT("tool");
		default:                    return TEXT("user");
		}
	}
}
