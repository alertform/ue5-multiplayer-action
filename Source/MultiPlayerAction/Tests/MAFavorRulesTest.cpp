#include "Misc/AutomationTest.h"
#include "Narrative/MAFavorRules.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMAFavorRulesTest,
	"MultiPlayerAction.Narrative.FavorRules",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMAFavorRulesTest::RunTest(const FString& Parameters)
{
	using namespace MAFavorRules;

	FMAAdjustFavorParams Out;
	FString Reject;

	// 合法参数与理由保留。
	TestTrue(TEXT("valid delta accepted"), ParseAndValidateAdjustFavor(
		TEXT("{\"delta\":1,\"reason\":\"言辞诚恳\"}"), Out, Reject));
	TestEqual(TEXT("delta kept"), Out.Delta, 1);
	TestEqual(TEXT("reason kept"), Out.Reason, TEXT("言辞诚恳"));

	// delta 钳制到 ±2；0 与缺失与坏 JSON 拒绝。
	ParseAndValidateAdjustFavor(TEXT("{\"delta\":9}"), Out, Reject);
	TestEqual(TEXT("delta clamped up"), Out.Delta, DeltaClampAbs);
	ParseAndValidateAdjustFavor(TEXT("{\"delta\":-7}"), Out, Reject);
	TestEqual(TEXT("delta clamped down"), Out.Delta, -DeltaClampAbs);
	TestFalse(TEXT("zero delta rejected"), ParseAndValidateAdjustFavor(TEXT("{\"delta\":0}"), Out, Reject));
	TestFalse(TEXT("missing delta rejected"), ParseAndValidateAdjustFavor(TEXT("{}"), Out, Reject));
	TestFalse(TEXT("bad json rejected"), ParseAndValidateAdjustFavor(TEXT("x"), Out, Reject));

	// 边界钳制。
	TestEqual(TEXT("apply within range"), ApplyDelta(0, 2), 2);
	TestEqual(TEXT("apply caps at max"), ApplyDelta(FavorMax - 1, 2), FavorMax);
	TestEqual(TEXT("apply caps at min"), ApplyDelta(FavorMin + 1, -2), FavorMin);

	// 门槛语义。
	TestTrue(TEXT("trusted at gate"), IsTrusted(TrustGate));
	TestFalse(TEXT("not trusted below gate"), IsTrusted(TrustGate - 1));
	TestTrue(TEXT("muted at gate"), IsMuted(MuteGate));
	TestFalse(TEXT("not muted above gate"), IsMuted(MuteGate + 1));

	// 态度描述覆盖四段且互不相同。
	const FString Muted = DescribeAttitude(MuteGate);
	const FString Cold = DescribeAttitude(-1);
	const FString Neutral = DescribeAttitude(0);
	const FString Trusted = DescribeAttitude(TrustGate);
	TestNotEqual(TEXT("muted != cold"), Muted, Cold);
	TestNotEqual(TEXT("cold != neutral"), Cold, Neutral);
	TestNotEqual(TEXT("neutral != trusted"), Neutral, Trusted);

	return true;
}

#endif
