#include "Misc/AutomationTest.h"
#include "Narrative/MAQuestRules.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMAQuestRulesTest,
	"MultiPlayerAction.Narrative.QuestRules",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMAQuestRulesTest::RunTest(const FString& Parameters)
{
	using EKill = MAQuestRules::EMAQuestKillResult;

	const FMAQuestState NoQuest;
	FMAGiveQuestParams Out;
	FString Reject;

	// 合法参数通过并保留台词。
	TestTrue(TEXT("valid args accepted"), MAQuestRules::ParseAndValidateGiveQuest(
		TEXT("{\"kill_count\":5,\"time_limit_seconds\":300,\"quest_line\":\"去吧\"}"),
		NoQuest, 0, Out, Reject));
	TestEqual(TEXT("kill count kept"), Out.KillCount, 5);
	TestEqual(TEXT("time kept"), Out.TimeLimitSeconds, 300);
	TestEqual(TEXT("line kept"), Out.QuestLine, TEXT("去吧"));

	// 钳制：kill_count 越界收进 1..10；time (0,60) 抬 60、>600 压 600、0 保持 0。
	TestTrue(TEXT("clamp parses"), MAQuestRules::ParseAndValidateGiveQuest(
		TEXT("{\"kill_count\":99,\"time_limit_seconds\":30,\"quest_line\":\"x\"}"), NoQuest, 0, Out, Reject));
	TestEqual(TEXT("kill clamped to 10"), Out.KillCount, 10);
	TestEqual(TEXT("time raised to 60"), Out.TimeLimitSeconds, 60);
	MAQuestRules::ParseAndValidateGiveQuest(
		TEXT("{\"kill_count\":1,\"time_limit_seconds\":9999,\"quest_line\":\"x\"}"), NoQuest, 0, Out, Reject);
	TestEqual(TEXT("time capped to 600"), Out.TimeLimitSeconds, 600);
	MAQuestRules::ParseAndValidateGiveQuest(
		TEXT("{\"kill_count\":1,\"time_limit_seconds\":0,\"quest_line\":\"x\"}"), NoQuest, 0, Out, Reject);
	TestEqual(TEXT("zero time = untimed"), Out.TimeLimitSeconds, 0);

	// 拒绝：JSON 坏、缺必填、已有进行中任务、预算耗尽。
	TestFalse(TEXT("bad json rejected"), MAQuestRules::ParseAndValidateGiveQuest(
		TEXT("not json"), NoQuest, 0, Out, Reject));
	TestFalse(TEXT("missing field rejected"), MAQuestRules::ParseAndValidateGiveQuest(
		TEXT("{\"quest_line\":\"x\"}"), NoQuest, 0, Out, Reject));
	MAQuestRules::ParseAndValidateGiveQuest(
		TEXT("{\"kill_count\":3,\"time_limit_seconds\":0,\"quest_line\":\"x\"}"), NoQuest, 0, Out, Reject);
	const FMAQuestState Active = MAQuestRules::MakeActiveQuest(Out, 100.0);
	TestFalse(TEXT("active quest blocks new one"), MAQuestRules::ParseAndValidateGiveQuest(
		TEXT("{\"kill_count\":3,\"time_limit_seconds\":0,\"quest_line\":\"x\"}"), Active, 1, Out, Reject));
	TestTrue(TEXT("reject reason set"), !Reject.IsEmpty());
	TestFalse(TEXT("budget exhausted rejected"), MAQuestRules::ParseAndValidateGiveQuest(
		TEXT("{\"kill_count\":3,\"time_limit_seconds\":0,\"quest_line\":\"x\"}"),
		NoQuest, MAQuestRules::MaxQuestsPerMatch, Out, Reject));

	// 状态机：进度 → 完成；限时任务的期限换算；过期一次性置 Failed。
	MAQuestRules::ParseAndValidateGiveQuest(
		TEXT("{\"kill_count\":2,\"time_limit_seconds\":60,\"quest_line\":\"x\"}"), NoQuest, 0, Out, Reject);
	FMAQuestState Quest = MAQuestRules::MakeActiveQuest(Out, 100.0);
	TestEqual(TEXT("timed type"), Quest.Type, EMAQuestType::TimedKill);
	TestEqual(TEXT("deadline = now + limit"), Quest.DeadlineServerTime, 160.f);
	TestEqual(TEXT("first kill progresses"), MAQuestRules::ApplyKill(Quest, 110.0), EKill::Progressed);
	TestEqual(TEXT("second kill completes"), MAQuestRules::ApplyKill(Quest, 120.0), EKill::JustCompleted);
	TestEqual(TEXT("phase completed"), Quest.Phase, EMAQuestPhase::Completed);
	TestEqual(TEXT("kills after completion ignored"), MAQuestRules::ApplyKill(Quest, 130.0), EKill::NotTracking);

	FMAQuestState Expiring = MAQuestRules::MakeActiveQuest(Out, 100.0);
	TestFalse(TEXT("not expired before deadline"), MAQuestRules::CheckExpired(Expiring, 159.0));
	TestTrue(TEXT("expired past deadline"), MAQuestRules::CheckExpired(Expiring, 161.0));
	TestEqual(TEXT("phase failed"), Expiring.Phase, EMAQuestPhase::Failed);
	TestFalse(TEXT("expiry fires once"), MAQuestRules::CheckExpired(Expiring, 162.0));
	TestEqual(TEXT("kill on expired quest ignored"), MAQuestRules::ApplyKill(Expiring, 163.0), EKill::NotTracking);

	// 期限已过但 Tick 还没扫到时的击杀也不算进度（双保险）。
	FMAQuestState PastDeadline = MAQuestRules::MakeActiveQuest(Out, 100.0);
	TestEqual(TEXT("kill past deadline not counted"), MAQuestRules::ApplyKill(PastDeadline, 200.0), EKill::NotTracking);

	// 不限时任务永不过期。
	MAQuestRules::ParseAndValidateGiveQuest(
		TEXT("{\"kill_count\":2,\"time_limit_seconds\":0,\"quest_line\":\"x\"}"), NoQuest, 0, Out, Reject);
	FMAQuestState Untimed = MAQuestRules::MakeActiveQuest(Out, 100.0);
	TestEqual(TEXT("untimed type"), Untimed.Type, EMAQuestType::KillCount);
	TestFalse(TEXT("untimed never expires"), MAQuestRules::CheckExpired(Untimed, 99999.0));

	// 终态（完成/失败）不阻挡新任务发布。
	TestTrue(TEXT("completed quest allows new one"), MAQuestRules::ParseAndValidateGiveQuest(
		TEXT("{\"kill_count\":3,\"time_limit_seconds\":0,\"quest_line\":\"x\"}"), Quest, 1, Out, Reject));

	// 重开对话的状态感知开场白：无任务=默认词；其余各态给不同的词且都非默认。
	{
		const FString DefaultGreeting = TEXT("站住，旅人。");
		TestEqual(TEXT("no quest -> default greeting"),
			MAQuestRules::MakeReturnGreeting(NoQuest, DefaultGreeting), DefaultGreeting);

		const FString ActiveGreeting = MAQuestRules::MakeReturnGreeting(Active, DefaultGreeting);
		TestNotEqual(TEXT("active quest greeting differs"), ActiveGreeting, DefaultGreeting);
		TestTrue(TEXT("active greeting mentions progress"), ActiveGreeting.Contains(TEXT("3")));

		TestNotEqual(TEXT("completed greeting differs"),
			MAQuestRules::MakeReturnGreeting(Quest, DefaultGreeting), DefaultGreeting);
		TestNotEqual(TEXT("failed greeting differs"),
			MAQuestRules::MakeReturnGreeting(Expiring, DefaultGreeting), DefaultGreeting);
	}

	return true;
}

#endif
