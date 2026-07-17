#include "AI/Combat/MAAdvisorDecision.h"

#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

namespace MAAdvisorDecision
{

FMAAdvisorDecision Parse(const FString& LLMText)
{
	FMAAdvisorDecision Out;

	// 模型常在 JSON 外包围栏或说明文字 —— 只取首个 '{' 到末个 '}'。
	const int32 Open = LLMText.Find(TEXT("{"));
	const int32 Close = LLMText.Find(TEXT("}"), ESearchCase::IgnoreCase, ESearchDir::FromEnd);
	if (Open == INDEX_NONE || Close == INDEX_NONE || Close <= Open)
	{
		return Out;
	}
	const FString JsonSlice = LLMText.Mid(Open, Close - Open + 1);

	TSharedPtr<FJsonObject> Root;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonSlice);
	if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
	{
		return Out;
	}

	Out.bValid = true;

	double MaxAttackers = 0.0;
	if (Root->TryGetNumberField(TEXT("max_attackers"), MaxAttackers))
	{
		Out.MaxAttackers = FMath::Clamp(FMath::RoundToInt32(MaxAttackers), 0, 3);
	}

	Root->TryGetStringField(TEXT("focus"), Out.FocusPlayerName);
	Out.FocusPlayerName.TrimStartAndEndInline();

	if (Root->TryGetStringField(TEXT("taunt"), Out.Taunt))
	{
		Out.Taunt.TrimStartAndEndInline();
		Out.Taunt.LeftInline(60);
	}

	return Out;
}

} // namespace MAAdvisorDecision
