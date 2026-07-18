#include "LLM/MALLMRequestBody.h"

#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

FString MALLM::BuildChatRequestBody(const TArray<FMALLMMessage>& Messages,
	const FMALLMRequestParams& Params, const TArray<FMALLMToolSpec>& Tools)
{
	TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
	Root->SetStringField(TEXT("model"), Params.Model);
	Root->SetBoolField(TEXT("stream"), true);
	// temperature 默认省缺（用模型服务端默认值）：kimi 全系携带非 1 的值会拒绝整个请求。
	if (Params.bSendTemperature)
	{
		Root->SetNumberField(TEXT("temperature"), Params.Temperature);
	}
	Root->SetNumberField(TEXT("max_tokens"), Params.MaxTokens);

	TArray<TSharedPtr<FJsonValue>> MessageArray;
	MessageArray.Reserve(Messages.Num());
	for (const FMALLMMessage& Msg : Messages)
	{
		TSharedRef<FJsonObject> JsonMsg = MakeShared<FJsonObject>();
		JsonMsg->SetStringField(TEXT("role"), MALLM::RoleToString(Msg.Role));
		JsonMsg->SetStringField(TEXT("content"), Msg.Content);
		if (Msg.Role == EMALLMRole::Tool && !Msg.ToolCallId.IsEmpty())
		{
			JsonMsg->SetStringField(TEXT("tool_call_id"), Msg.ToolCallId);
		}
		if (Msg.Role == EMALLMRole::Assistant && Msg.ToolCalls.Num() > 0)
		{
			TArray<TSharedPtr<FJsonValue>> CallArray;
			for (const FMALLMToolCall& Call : Msg.ToolCalls)
			{
				TSharedRef<FJsonObject> CallObj = MakeShared<FJsonObject>();
				CallObj->SetStringField(TEXT("id"), Call.Id);
				CallObj->SetStringField(TEXT("type"), TEXT("function"));
				TSharedRef<FJsonObject> Fn = MakeShared<FJsonObject>();
				Fn->SetStringField(TEXT("name"), Call.Name);
				// arguments 按 OpenAI 协议是字符串字段，不展开成对象。
				Fn->SetStringField(TEXT("arguments"), Call.ArgumentsJson);
				CallObj->SetObjectField(TEXT("function"), Fn);
				CallArray.Add(MakeShared<FJsonValueObject>(CallObj));
			}
			JsonMsg->SetArrayField(TEXT("tool_calls"), CallArray);
		}
		MessageArray.Add(MakeShared<FJsonValueObject>(JsonMsg));
	}
	Root->SetArrayField(TEXT("messages"), MessageArray);

	if (Tools.Num() > 0)
	{
		TArray<TSharedPtr<FJsonValue>> ToolArray;
		for (const FMALLMToolSpec& Tool : Tools)
		{
			TSharedRef<FJsonObject> ToolObj = MakeShared<FJsonObject>();
			ToolObj->SetStringField(TEXT("type"), TEXT("function"));
			TSharedRef<FJsonObject> Fn = MakeShared<FJsonObject>();
			Fn->SetStringField(TEXT("name"), Tool.Name);
			Fn->SetStringField(TEXT("description"), Tool.Description);
			// schema 字符串解析为对象注入 —— API 要求 parameters 是 JSON 对象不是字符串。
			TSharedPtr<FJsonObject> SchemaObj;
			if (FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Tool.ParametersSchemaJson), SchemaObj)
				&& SchemaObj.IsValid())
			{
				Fn->SetObjectField(TEXT("parameters"), SchemaObj);
			}
			ToolObj->SetObjectField(TEXT("function"), Fn);
			ToolArray.Add(MakeShared<FJsonValueObject>(ToolObj));
		}
		Root->SetArrayField(TEXT("tools"), ToolArray);
	}

	FString Body;
	const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Body);
	FJsonSerializer::Serialize(Root, Writer);
	return Body;
}
