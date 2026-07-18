#include "Misc/AutomationTest.h"
#include "Dom/JsonObject.h"
#include "LLM/MALLMRequestBody.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMALLMRequestBodyTest,
	"MultiPlayerAction.LLM.RequestBody",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

namespace
{
	TSharedPtr<FJsonObject> MARequestBodyTest_Parse(const FString& Body)
	{
		TSharedPtr<FJsonObject> Obj;
		FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Body), Obj);
		return Obj;
	}
}

bool FMALLMRequestBodyTest::RunTest(const FString& Parameters)
{
	FMALLMRequestParams Params;
	Params.Model = TEXT("kimi-k2.6");
	Params.MaxTokens = 256;

	// 1) temperature 默认省缺；开 bSendTemperature 才携带（kimi 全系只收 1 的教训）。
	{
		TArray<FMALLMMessage> Msgs = { FMALLMMessage(EMALLMRole::User, TEXT("hi")) };
		TSharedPtr<FJsonObject> Body = MARequestBodyTest_Parse(MALLM::BuildChatRequestBody(Msgs, Params, {}));
		if (!TestTrue(TEXT("body parses"), Body.IsValid()))
		{
			return true;
		}
		TestFalse(TEXT("no temperature by default"), Body->HasField(TEXT("temperature")));
		TestFalse(TEXT("no tools field when empty"), Body->HasField(TEXT("tools")));
		TestEqual(TEXT("model kept"), Body->GetStringField(TEXT("model")), TEXT("kimi-k2.6"));
		TestTrue(TEXT("stream on"), Body->GetBoolField(TEXT("stream")));

		FMALLMRequestParams WithTemp = Params;
		WithTemp.bSendTemperature = true;
		WithTemp.Temperature = 1.f;
		TSharedPtr<FJsonObject> Body2 = MARequestBodyTest_Parse(MALLM::BuildChatRequestBody(Msgs, WithTemp, {}));
		TestTrue(TEXT("temperature present when opted in"), Body2->HasField(TEXT("temperature")));
	}

	// 2) tools 数组：schema 字符串必须注入为 JSON 对象而非字符串。
	{
		FMALLMToolSpec Spec;
		Spec.Name = TEXT("give_quest");
		Spec.Description = TEXT("发布击杀任务");
		Spec.ParametersSchemaJson = TEXT("{\"type\":\"object\",\"properties\":{\"kill_count\":{\"type\":\"integer\"}},\"required\":[\"kill_count\"]}");
		TArray<FMALLMMessage> Msgs = { FMALLMMessage(EMALLMRole::User, TEXT("hi")) };
		TSharedPtr<FJsonObject> Body = MARequestBodyTest_Parse(MALLM::BuildChatRequestBody(Msgs, Params, { Spec }));
		const TArray<TSharedPtr<FJsonValue>>* Tools = nullptr;
		TestTrue(TEXT("tools array present"), Body->TryGetArrayField(TEXT("tools"), Tools) && Tools->Num() == 1);
		if (Tools && Tools->Num() == 1)
		{
			const TSharedPtr<FJsonObject> Tool = (*Tools)[0]->AsObject();
			TestEqual(TEXT("tool type"), Tool->GetStringField(TEXT("type")), TEXT("function"));
			const TSharedPtr<FJsonObject> Fn = Tool->GetObjectField(TEXT("function"));
			TestEqual(TEXT("tool name"), Fn->GetStringField(TEXT("name")), TEXT("give_quest"));
			TestTrue(TEXT("schema is object not string"),
				Fn->GetObjectField(TEXT("parameters"))->HasField(TEXT("properties")));
		}
	}

	// 3) assistant 带 tool_calls 与 tool 结果消息的序列化（OpenAI 协议回填格式）。
	{
		FMALLMMessage Assistant(EMALLMRole::Assistant, TEXT(""));
		FMALLMToolCall Call;
		Call.Id = TEXT("give_quest_0");
		Call.Name = TEXT("give_quest");
		Call.ArgumentsJson = TEXT("{\"kill_count\":5}");
		Assistant.ToolCalls.Add(Call);

		FMALLMMessage ToolResult(EMALLMRole::Tool, TEXT("已发布任务"));
		ToolResult.ToolCallId = TEXT("give_quest_0");

		TArray<FMALLMMessage> Msgs = { Assistant, ToolResult };
		TSharedPtr<FJsonObject> Body = MARequestBodyTest_Parse(MALLM::BuildChatRequestBody(Msgs, Params, {}));
		const TArray<TSharedPtr<FJsonValue>>* Arr = nullptr;
		Body->TryGetArrayField(TEXT("messages"), Arr);
		if (TestTrue(TEXT("two messages"), Arr && Arr->Num() == 2))
		{
			const TSharedPtr<FJsonObject> A = (*Arr)[0]->AsObject();
			TestEqual(TEXT("assistant role"), A->GetStringField(TEXT("role")), TEXT("assistant"));
			const TArray<TSharedPtr<FJsonValue>>* Calls = nullptr;
			if (TestTrue(TEXT("assistant carries tool_calls"), A->TryGetArrayField(TEXT("tool_calls"), Calls) && Calls->Num() == 1))
			{
				const TSharedPtr<FJsonObject> CallObj = (*Calls)[0]->AsObject();
				TestEqual(TEXT("call id"), CallObj->GetStringField(TEXT("id")), TEXT("give_quest_0"));
				TestEqual(TEXT("call args are raw string"),
					CallObj->GetObjectField(TEXT("function"))->GetStringField(TEXT("arguments")),
					TEXT("{\"kill_count\":5}"));
			}
			const TSharedPtr<FJsonObject> T = (*Arr)[1]->AsObject();
			TestEqual(TEXT("tool role"), T->GetStringField(TEXT("role")), TEXT("tool"));
			TestEqual(TEXT("tool_call_id"), T->GetStringField(TEXT("tool_call_id")), TEXT("give_quest_0"));
		}
	}

	return true;
}

#endif
