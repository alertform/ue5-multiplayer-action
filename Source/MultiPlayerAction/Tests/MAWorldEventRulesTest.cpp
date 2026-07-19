#include "Misc/AutomationTest.h"
#include "Narrative/MAWorldEventRules.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMAWorldEventRulesTest,
	"MultiPlayerAction.Narrative.WorldEventRules",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMAWorldEventRulesTest::RunTest(const FString& Parameters)
{
	using namespace MAWorldEventRules;

	FMAWorldEventState Fresh;
	Fresh.NowServerTime = 100.0;
	Fresh.MatchEndServerTime = 100.0 + 300.0;

	FMARaidParams Raid;
	FMABlessingParams Blessing;
	FString Reject;

	// 敌袭：合法参数 + 钳制。
	TestTrue(TEXT("valid raid accepted"), ParseAndValidateRaid(
		TEXT("{\"enemy_count\":4,\"raid_line\":\"来吧\"}"), Fresh, Raid, Reject));
	TestEqual(TEXT("count kept"), Raid.EnemyCount, 4);
	TestEqual(TEXT("line kept"), Raid.RaidLine, TEXT("来吧"));
	ParseAndValidateRaid(TEXT("{\"enemy_count\":99,\"raid_line\":\"x\"}"), Fresh, Raid, Reject);
	TestEqual(TEXT("count clamped"), Raid.EnemyCount, RaidCountMax);

	// 敌袭：预算耗尽 / JSON 坏 / 缺字段。
	FMAWorldEventState UsedUp = Fresh;
	UsedUp.RaidsUsed = MaxRaidsPerMatch;
	TestFalse(TEXT("raid budget exhausted"), ParseAndValidateRaid(
		TEXT("{\"enemy_count\":2,\"raid_line\":\"x\"}"), UsedUp, Raid, Reject));
	TestFalse(TEXT("raid bad json"), ParseAndValidateRaid(TEXT("nope"), Fresh, Raid, Reject));
	TestFalse(TEXT("raid missing count"), ParseAndValidateRaid(TEXT("{}"), Fresh, Raid, Reject));

	// 冷却：距上一事件不足 60s 拒绝；足够则放行。
	FMAWorldEventState Cooling = Fresh;
	Cooling.LastEventServerTime = Fresh.NowServerTime - 30.0;
	TestFalse(TEXT("cooldown blocks"), ParseAndValidateRaid(
		TEXT("{\"enemy_count\":2,\"raid_line\":\"x\"}"), Cooling, Raid, Reject));
	Cooling.LastEventServerTime = Fresh.NowServerTime - 61.0;
	TestTrue(TEXT("cooldown elapsed"), ParseAndValidateRaid(
		TEXT("{\"enemy_count\":2,\"raid_line\":\"x\"}"), Cooling, Raid, Reject));

	// 末段锁定：距对局结束 <60s 拒绝；未开局（MatchEnd<=0）不锁。
	FMAWorldEventState LateGame = Fresh;
	LateGame.MatchEndServerTime = Fresh.NowServerTime + 45.0;
	TestFalse(TEXT("late game locks out"), ParseAndValidateRaid(
		TEXT("{\"enemy_count\":2,\"raid_line\":\"x\"}"), LateGame, Raid, Reject));
	FMAWorldEventState NoMatch = Fresh;
	NoMatch.MatchEndServerTime = -1.0;
	TestTrue(TEXT("no match clock = no lockout"), ParseAndValidateRaid(
		TEXT("{\"enemy_count\":2,\"raid_line\":\"x\"}"), NoMatch, Raid, Reject));

	// 赐福：三种类型解析、未知类型拒绝、时长钳制、预算独立于敌袭。
	TestTrue(TEXT("blessing attack"), ParseAndValidateBlessing(
		TEXT("{\"blessing_type\":\"attack\",\"duration\":60,\"line\":\"受我一礼\"}"), Fresh, Blessing, Reject));
	TestEqual(TEXT("type attack"), Blessing.Type, EMABlessingType::Attack);
	TestTrue(TEXT("blessing speed"), ParseAndValidateBlessing(
		TEXT("{\"blessing_type\":\"speed\",\"duration\":10}"), Fresh, Blessing, Reject));
	TestEqual(TEXT("type speed"), Blessing.Type, EMABlessingType::Speed);
	TestEqual(TEXT("duration raised to min"), Blessing.DurationSeconds, BlessingDurationMin);
	TestTrue(TEXT("blessing regen"), ParseAndValidateBlessing(
		TEXT("{\"blessing_type\":\"regen\",\"duration\":999}"), Fresh, Blessing, Reject));
	TestEqual(TEXT("duration capped"), Blessing.DurationSeconds, BlessingDurationMax);
	TestFalse(TEXT("unknown type rejected"), ParseAndValidateBlessing(
		TEXT("{\"blessing_type\":\"luck\",\"duration\":60}"), Fresh, Blessing, Reject));
	FMAWorldEventState BlessUsedUp = Fresh;
	BlessUsedUp.BlessingsUsed = MaxBlessingsPerMatch;
	TestFalse(TEXT("blessing budget exhausted"), ParseAndValidateBlessing(
		TEXT("{\"blessing_type\":\"attack\",\"duration\":60}"), BlessUsedUp, Blessing, Reject));
	BlessUsedUp.RaidsUsed = MaxRaidsPerMatch; // 敌袭预算满不影响赐福（独立账本）
	FMAWorldEventState OnlyRaidUsed = Fresh;
	OnlyRaidUsed.RaidsUsed = MaxRaidsPerMatch;
	TestTrue(TEXT("raid budget does not affect blessing"), ParseAndValidateBlessing(
		TEXT("{\"blessing_type\":\"attack\",\"duration\":60}"), OnlyRaidUsed, Blessing, Reject));

	return true;
}

#endif
