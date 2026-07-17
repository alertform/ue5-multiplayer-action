#include "Misc/AutomationTest.h"
#include "AI/Combat/MAAdvisorDecision.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMAAdvisorDecisionTest,
	"MultiPlayerAction.AICombat.AdvisorDecision",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMAAdvisorDecisionTest::RunTest(const FString& Parameters)
{
	// 干净 JSON：全字段。
	{
		const FMAAdvisorDecision D = MAAdvisorDecision::Parse(
			TEXT("{\"max_attackers\":2,\"focus\":\"Ziton\",\"taunt\":\"上！别让他喘气！\"}"));
		TestTrue(TEXT("clean json valid"), D.bValid);
		TestEqual(TEXT("max attackers"), D.MaxAttackers, 2);
		TestEqual(TEXT("focus name"), D.FocusPlayerName, TEXT("Ziton"));
		TestEqual(TEXT("taunt"), D.Taunt, TEXT("上！别让他喘气！"));
	}

	// 模型爱加代码围栏和废话 —— 提取首个 { 到末个 } 之间的对象。
	{
		const FMAAdvisorDecision D = MAAdvisorDecision::Parse(
			TEXT("好的，指令如下：\n```json\n{\"max_attackers\":1,\"focus\":\"\",\"taunt\":\"围住他\"}\n```\n请执行。"));
		TestTrue(TEXT("fenced json valid"), D.bValid);
		TestEqual(TEXT("fenced max"), D.MaxAttackers, 1);
		TestEqual(TEXT("fenced focus empty"), D.FocusPlayerName, TEXT(""));
	}

	// 字段缺失 → 有效但用"不建议"缺省值。
	{
		const FMAAdvisorDecision D = MAAdvisorDecision::Parse(TEXT("{\"taunt\":\"呵\"}"));
		TestTrue(TEXT("partial valid"), D.bValid);
		TestEqual(TEXT("missing max = -1"), D.MaxAttackers, -1);
		TestEqual(TEXT("missing focus = empty"), D.FocusPlayerName, TEXT(""));
	}

	// 越界钳制：max_attackers 限 [0,3]。
	{
		TestEqual(TEXT("clamp high"), MAAdvisorDecision::Parse(TEXT("{\"max_attackers\":7}")).MaxAttackers, 3);
		TestEqual(TEXT("clamp low"), MAAdvisorDecision::Parse(TEXT("{\"max_attackers\":-2}")).MaxAttackers, 0);
	}

	// 超长嘲讽截断（60 字符上限）。
	{
		FString Long;
		for (int32 i = 0; i < 50; ++i) { Long += TEXT("哈哈"); }
		const FMAAdvisorDecision D = MAAdvisorDecision::Parse(
			FString::Printf(TEXT("{\"taunt\":\"%s\"}"), *Long));
		TestTrue(TEXT("long taunt valid"), D.bValid);
		TestTrue(TEXT("taunt truncated"), D.Taunt.Len() <= 60);
	}

	// 垃圾输入 / 非对象 → 无效。
	TestFalse(TEXT("garbage invalid"), MAAdvisorDecision::Parse(TEXT("我不知道该怎么办")).bValid);
	TestFalse(TEXT("array invalid"), MAAdvisorDecision::Parse(TEXT("[1,2,3]")).bValid);
	TestFalse(TEXT("empty invalid"), MAAdvisorDecision::Parse(TEXT("")).bValid);

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
