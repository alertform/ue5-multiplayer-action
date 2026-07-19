#include "Narrative/MAWorldEventRules.h"

#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

namespace
{
	// 冷却 + 末段锁定：两类世界事件共用的门。
	bool MAWorldEvent_ValidateCommon(const MAWorldEventRules::FMAWorldEventState& State, FString& OutReject)
	{
		if (State.NowServerTime - State.LastEventServerTime < MAWorldEventRules::EventCooldownSeconds)
		{
			OutReject = TEXT("世界事件冷却中，稍后再试。");
			return false;
		}
		if (State.MatchEndServerTime > 0.0 &&
			State.MatchEndServerTime - State.NowServerTime < MAWorldEventRules::MatchEndLockoutSeconds)
		{
			OutReject = TEXT("对局临近结束，不再发起世界事件。");
			return false;
		}
		return true;
	}

	TSharedPtr<FJsonObject> MAWorldEvent_ParseArgs(const FString& ArgsJson, FString& OutReject)
	{
		TSharedPtr<FJsonObject> Args;
		if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(ArgsJson), Args) || !Args.IsValid())
		{
			OutReject = TEXT("参数不是合法 JSON。");
		}
		return Args;
	}
}

bool MAWorldEventRules::ParseAndValidateRaid(const FString& ArgsJson,
	const FMAWorldEventState& State, FMARaidParams& Out, FString& OutReject)
{
	OutReject.Reset();
	if (State.RaidsUsed >= MaxRaidsPerMatch)
	{
		OutReject = TEXT("本场敌袭次数已用尽。");
		return false;
	}
	if (!MAWorldEvent_ValidateCommon(State, OutReject))
	{
		return false;
	}
	const TSharedPtr<FJsonObject> Args = MAWorldEvent_ParseArgs(ArgsJson, OutReject);
	if (!Args.IsValid())
	{
		return false;
	}
	int32 Count = 0;
	if (!Args->TryGetNumberField(TEXT("enemy_count"), Count))
	{
		OutReject = TEXT("缺少 enemy_count。");
		return false;
	}
	Out.EnemyCount = FMath::Clamp(Count, RaidCountMin, RaidCountMax);
	Args->TryGetStringField(TEXT("raid_line"), Out.RaidLine);
	return true;
}

bool MAWorldEventRules::ParseAndValidateBlessing(const FString& ArgsJson,
	const FMAWorldEventState& State, FMABlessingParams& Out, FString& OutReject)
{
	OutReject.Reset();
	if (State.BlessingsUsed >= MaxBlessingsPerMatch)
	{
		OutReject = TEXT("本场赐福次数已用尽。");
		return false;
	}
	if (!MAWorldEvent_ValidateCommon(State, OutReject))
	{
		return false;
	}
	const TSharedPtr<FJsonObject> Args = MAWorldEvent_ParseArgs(ArgsJson, OutReject);
	if (!Args.IsValid())
	{
		return false;
	}
	FString TypeStr;
	if (!Args->TryGetStringField(TEXT("blessing_type"), TypeStr))
	{
		OutReject = TEXT("缺少 blessing_type。");
		return false;
	}
	if (TypeStr == TEXT("attack"))      { Out.Type = EMABlessingType::Attack; }
	else if (TypeStr == TEXT("speed"))  { Out.Type = EMABlessingType::Speed; }
	else if (TypeStr == TEXT("regen"))  { Out.Type = EMABlessingType::Regen; }
	else
	{
		OutReject = FString::Printf(TEXT("未知 blessing_type：%s（可选 attack/speed/regen）。"), *TypeStr);
		return false;
	}
	int32 Duration = 60;
	Args->TryGetNumberField(TEXT("duration"), Duration);
	Out.DurationSeconds = FMath::Clamp(Duration, BlessingDurationMin, BlessingDurationMax);
	Args->TryGetStringField(TEXT("line"), Out.Line);
	return true;
}
