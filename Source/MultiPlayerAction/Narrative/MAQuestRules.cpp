#include "Narrative/MAQuestRules.h"

#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

bool MAQuestRules::ParseAndValidateGiveQuest(const FString& ArgsJson,
	const FMAQuestState& Current, int32 IssuedThisMatch,
	FMAGiveQuestParams& Out, FString& OutReject)
{
	OutReject.Reset();

	if (Current.Phase == EMAQuestPhase::Active)
	{
		OutReject = TEXT("玩家已有进行中的任务，先完成或等它超时。");
		return false;
	}
	if (IssuedThisMatch >= MaxQuestsPerMatch)
	{
		OutReject = TEXT("本场对局任务次数已用尽，不能再发。");
		return false;
	}

	TSharedPtr<FJsonObject> Args;
	if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(ArgsJson), Args) || !Args.IsValid())
	{
		OutReject = TEXT("任务参数不是合法 JSON。");
		return false;
	}

	int32 KillCount = 0;
	int32 TimeLimit = 0;
	if (!Args->TryGetNumberField(TEXT("kill_count"), KillCount) ||
		!Args->TryGetNumberField(TEXT("time_limit_seconds"), TimeLimit))
	{
		OutReject = TEXT("缺少 kill_count 或 time_limit_seconds。");
		return false;
	}

	Out.KillCount = FMath::Clamp(KillCount, KillCountMin, KillCountMax);
	Out.TimeLimitSeconds = TimeLimit <= 0 ? 0 : FMath::Clamp(TimeLimit, TimeLimitMin, TimeLimitMax);
	Args->TryGetStringField(TEXT("quest_line"), Out.QuestLine);
	return true;
}

FMAQuestState MAQuestRules::MakeActiveQuest(const FMAGiveQuestParams& Params, double NowServerTime)
{
	FMAQuestState Quest;
	Quest.Type = Params.TimeLimitSeconds > 0 ? EMAQuestType::TimedKill : EMAQuestType::KillCount;
	Quest.Phase = EMAQuestPhase::Active;
	Quest.TargetKills = Params.KillCount;
	Quest.Progress = 0;
	Quest.DeadlineServerTime = Params.TimeLimitSeconds > 0
		? static_cast<float>(NowServerTime + Params.TimeLimitSeconds) : 0.f;
	return Quest;
}

MAQuestRules::EMAQuestKillResult MAQuestRules::ApplyKill(FMAQuestState& Quest, double NowServerTime)
{
	if (Quest.Phase != EMAQuestPhase::Active)
	{
		return EMAQuestKillResult::NotTracking;
	}
	// 过期后、Tick 扫描标记前到达的击杀不算进度 —— 状态标记统一交给 CheckExpired。
	if (Quest.DeadlineServerTime > 0.f && NowServerTime > Quest.DeadlineServerTime)
	{
		return EMAQuestKillResult::NotTracking;
	}
	++Quest.Progress;
	if (Quest.Progress >= Quest.TargetKills)
	{
		Quest.Phase = EMAQuestPhase::Completed;
		return EMAQuestKillResult::JustCompleted;
	}
	return EMAQuestKillResult::Progressed;
}

bool MAQuestRules::CheckExpired(FMAQuestState& Quest, double NowServerTime)
{
	if (Quest.Phase != EMAQuestPhase::Active || Quest.DeadlineServerTime <= 0.f)
	{
		return false;
	}
	if (NowServerTime <= Quest.DeadlineServerTime)
	{
		return false;
	}
	Quest.Phase = EMAQuestPhase::Failed;
	return true;
}
