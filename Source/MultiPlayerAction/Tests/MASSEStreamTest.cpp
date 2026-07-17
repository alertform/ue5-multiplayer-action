#include "Misc/AutomationTest.h"
#include "LLM/MASSEStream.h"

#if WITH_DEV_AUTOMATION_TESTS

// ---------------------------------------------------------------------------
// FMASSEStreamParser — byte-level SSE framing
// ---------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMASSEParserTest,
	"MultiPlayerAction.LLM.SSEParser",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

namespace
{
	// Feed a whole TCHAR string to the parser as UTF-8 bytes.
	void FeedString(FMASSEStreamParser& Parser, const FString& Text, TArray<FString>& OutEvents)
	{
		FTCHARToUTF8 Utf8(*Text);
		Parser.Append(reinterpret_cast<const uint8*>(Utf8.Get()), Utf8.Length(), OutEvents);
	}
}

bool FMASSEParserTest::RunTest(const FString& Parameters)
{
	// Single complete frame in one chunk.
	{
		FMASSEStreamParser P;
		TArray<FString> Events;
		FeedString(P, TEXT("data: {\"a\":1}\n\n"), Events);
		TestEqual(TEXT("single frame -> one event"), Events.Num(), 1);
		if (Events.Num() == 1)
		{
			TestEqual(TEXT("payload stripped of data: prefix"), Events[0], TEXT("{\"a\":1}"));
		}
	}

	// Chunk boundary inside a multi-byte UTF-8 char must not corrupt the payload.
	{
		FMASSEStreamParser P;
		TArray<FString> Events;
		const FString Frame = TEXT("data: {\"c\":\"你好\"}\n\n");
		FTCHARToUTF8 Utf8(*Frame);
		const uint8* Bytes = reinterpret_cast<const uint8*>(Utf8.Get());
		const int32 Split = 13; // lands inside 你 (3-byte sequence starting at byte 12)
		P.Append(Bytes, Split, Events);
		TestEqual(TEXT("no event from partial frame"), Events.Num(), 0);
		P.Append(Bytes + Split, Utf8.Length() - Split, Events);
		TestEqual(TEXT("one event after completion"), Events.Num(), 1);
		if (Events.Num() == 1)
		{
			TestEqual(TEXT("UTF-8 survives split"), Events[0], TEXT("{\"c\":\"你好\"}"));
		}
	}

	// Several frames in one chunk arrive in order.
	{
		FMASSEStreamParser P;
		TArray<FString> Events;
		FeedString(P, TEXT("data: one\n\ndata: two\n\ndata: three\n\n"), Events);
		TestEqual(TEXT("three frames"), Events.Num(), 3);
		if (Events.Num() == 3)
		{
			TestEqual(TEXT("order 0"), Events[0], TEXT("one"));
			TestEqual(TEXT("order 1"), Events[1], TEXT("two"));
			TestEqual(TEXT("order 2"), Events[2], TEXT("three"));
		}
	}

	// CRLF line endings (Kimi/Moonshot serves \n but stay spec-tolerant).
	{
		FMASSEStreamParser P;
		TArray<FString> Events;
		FeedString(P, TEXT("data: X\r\n\r\n"), Events);
		TestEqual(TEXT("CRLF frame"), Events.Num(), 1);
		if (Events.Num() == 1)
		{
			TestEqual(TEXT("CRLF payload clean"), Events[0], TEXT("X"));
		}
	}

	// Comment / keep-alive lines and non-data fields are ignored.
	{
		FMASSEStreamParser P;
		TArray<FString> Events;
		FeedString(P, TEXT(": keep-alive\n\nevent: message\nid: 7\ndata: bar\n\n"), Events);
		TestEqual(TEXT("only data fields emit"), Events.Num(), 1);
		if (Events.Num() == 1)
		{
			TestEqual(TEXT("data field extracted"), Events[0], TEXT("bar"));
		}
	}

	// Multiple data lines in one frame join with \n (SSE spec).
	{
		FMASSEStreamParser P;
		TArray<FString> Events;
		FeedString(P, TEXT("data: a\ndata: b\n\n"), Events);
		TestEqual(TEXT("multi-line data one event"), Events.Num(), 1);
		if (Events.Num() == 1)
		{
			TestEqual(TEXT("joined with newline"), Events[0], TEXT("a\nb"));
		}
	}

	// "data:" with no space after the colon (spec allows both).
	{
		FMASSEStreamParser P;
		TArray<FString> Events;
		FeedString(P, TEXT("data:[DONE]\n\n"), Events);
		TestEqual(TEXT("no-space data"), Events.Num(), 1);
		if (Events.Num() == 1)
		{
			TestEqual(TEXT("[DONE] passthrough"), Events[0], TEXT("[DONE]"));
		}
	}

	// Partial line stays buffered until its frame completes.
	{
		FMASSEStreamParser P;
		TArray<FString> Events;
		FeedString(P, TEXT("data: x"), Events);
		TestEqual(TEXT("incomplete frame holds"), Events.Num(), 0);
		FeedString(P, TEXT("\n\n"), Events);
		TestEqual(TEXT("completes on delimiter"), Events.Num(), 1);
		if (Events.Num() == 1)
		{
			TestEqual(TEXT("buffered payload"), Events[0], TEXT("x"));
		}
	}

	return true;
}

// ---------------------------------------------------------------------------
// MAOpenAISSE — OpenAI 兼容流式 chunk 的 JSON 解析（Kimi 走同一格式）
// ---------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMAOpenAIChunkTest,
	"MultiPlayerAction.LLM.OpenAIChunk",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMAOpenAIChunkTest::RunTest(const FString& Parameters)
{
	// Normal content delta.
	{
		const FMAOpenAIChunk C = MAOpenAISSE::ParseChunk(
			TEXT("{\"choices\":[{\"delta\":{\"content\":\"你好\"},\"finish_reason\":null}]}"));
		TestTrue(TEXT("valid chunk"), C.bValid);
		TestEqual(TEXT("content delta"), C.Content, TEXT("你好"));
		TestEqual(TEXT("no finish yet"), C.FinishReason, TEXT(""));
	}

	// First chunk often carries only the role — valid, empty content.
	{
		const FMAOpenAIChunk C = MAOpenAISSE::ParseChunk(
			TEXT("{\"choices\":[{\"delta\":{\"role\":\"assistant\"}}]}"));
		TestTrue(TEXT("role-only chunk valid"), C.bValid);
		TestEqual(TEXT("role-only has no content"), C.Content, TEXT(""));
	}

	// Final chunk carries finish_reason.
	{
		const FMAOpenAIChunk C = MAOpenAISSE::ParseChunk(
			TEXT("{\"choices\":[{\"delta\":{},\"finish_reason\":\"stop\"}]}"));
		TestTrue(TEXT("finish chunk valid"), C.bValid);
		TestEqual(TEXT("finish reason"), C.FinishReason, TEXT("stop"));
	}

	// [DONE] sentinel is not JSON — detected separately.
	TestTrue(TEXT("done marker"), MAOpenAISSE::IsDoneMarker(TEXT("[DONE]")));
	TestTrue(TEXT("done marker tolerates spaces"), MAOpenAISSE::IsDoneMarker(TEXT(" [DONE] ")));
	TestFalse(TEXT("json not done"), MAOpenAISSE::IsDoneMarker(TEXT("{\"choices\":[]}")));

	// Garbage does not crash and reports invalid.
	{
		const FMAOpenAIChunk C = MAOpenAISSE::ParseChunk(TEXT("not json at all"));
		TestFalse(TEXT("garbage invalid"), C.bValid);
	}

	// API error body -> human-readable message.
	{
		const FString Msg = MAOpenAISSE::ExtractErrorMessage(
			TEXT("{\"error\":{\"message\":\"Invalid API key\",\"type\":\"auth_error\"}}"));
		TestEqual(TEXT("error message extracted"), Msg, TEXT("Invalid API key"));
		TestEqual(TEXT("no error in clean body"), MAOpenAISSE::ExtractErrorMessage(TEXT("{}")), TEXT(""));
	}

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
