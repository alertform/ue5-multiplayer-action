#include "Combat/MAFallDamage.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMAFallDamageComputeTest,
	"MultiPlayerAction.Combat.FallDamage",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMAFallDamageComputeTest::RunTest(const FString& Parameters)
{
	// 安全速度以内无伤
	TestEqual(TEXT("below safe speed"), MAFallDamage::Compute(900.f, 1400.f, 0.05f), 0.f);
	TestEqual(TEXT("exactly safe speed"), MAFallDamage::Compute(1400.f, 1400.f, 0.05f), 0.f);
	TestEqual(TEXT("zero speed"), MAFallDamage::Compute(0.f, 1400.f, 0.05f), 0.f);

	// 超出部分线性
	TestEqual(TEXT("linear above safe"), MAFallDamage::Compute(2400.f, 1400.f, 0.05f), 50.f);
	TestEqual(TEXT("lethal-scale landing"), MAFallDamage::Compute(3400.f, 1400.f, 0.05f), 100.f);

	// 非法参数兜底
	TestEqual(TEXT("zero rate"), MAFallDamage::Compute(3000.f, 1400.f, 0.f), 0.f);
	TestEqual(TEXT("negative rate"), MAFallDamage::Compute(3000.f, 1400.f, -1.f), 0.f);

	return true;
}

#endif
