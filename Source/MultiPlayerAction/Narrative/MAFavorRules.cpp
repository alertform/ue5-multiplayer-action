#include "Narrative/MAFavorRules.h"

#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

bool MAFavorRules::ParseAndValidateAdjustFavor(const FString& ArgsJson,
	FMAAdjustFavorParams& Out, FString& OutReject)
{
	OutReject.Reset();

	TSharedPtr<FJsonObject> Args;
	if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(ArgsJson), Args) || !Args.IsValid())
	{
		OutReject = TEXT("参数不是合法 JSON。");
		return false;
	}
	int32 Delta = 0;
	if (!Args->TryGetNumberField(TEXT("delta"), Delta))
	{
		OutReject = TEXT("缺少 delta。");
		return false;
	}
	if (Delta == 0)
	{
		OutReject = TEXT("delta 不能为 0。");
		return false;
	}
	Out.Delta = FMath::Clamp(Delta, -DeltaClampAbs, DeltaClampAbs);
	Args->TryGetStringField(TEXT("reason"), Out.Reason);
	return true;
}

int32 MAFavorRules::ApplyDelta(int32 CurrentFavor, int32 Delta)
{
	return FMath::Clamp(CurrentFavor + Delta, FavorMin, FavorMax);
}

FString MAFavorRules::DescribeAttitude(int32 Favor)
{
	if (IsMuted(Favor))
	{
		return TEXT("你已被此人惹恼，冷淡拒绝一切请求（任务、敌袭、赐福都不给），除非对方诚恳道歉挽回好感");
	}
	if (IsTrusted(Favor))
	{
		return TEXT("你信任此人，愿意透露对局情报并可为其赐福");
	}
	if (Favor < 0)
	{
		return TEXT("你对此人略有不满，说话更冷");
	}
	return TEXT("你对此人态度中立");
}
