#include "Misc/AutomationTest.h"
#include "Config/MAConfigTypes.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMAConfigLookupTest,
	"MultiPlayerAction.LuaConfig.Lookup",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMAConfigLookupTest::RunTest(const FString& Parameters)
{
	TMap<FName, FMAConfigTable> Cache;
	FMAConfigTable& Melee = Cache.Add(TEXT("MeleeAttack"));
	Melee.Numbers.Add(TEXT("TraceRadius"), 120.0);
	Melee.Strings.Add(TEXT("MuzzleSocketName"), TEXT("hand_l"));

	// 命中 number
	TestEqual(TEXT("hit float"),
		MAConfigLookup::GetFloat(Cache, TEXT("MeleeAttack"), TEXT("TraceRadius"), 50.f), 120.f);
	// 缺键回退
	TestEqual(TEXT("missing param -> default"),
		MAConfigLookup::GetFloat(Cache, TEXT("MeleeAttack"), TEXT("Nope"), 50.f), 50.f);
	TestEqual(TEXT("missing config -> default"),
		MAConfigLookup::GetFloat(Cache, TEXT("Ghost"), TEXT("TraceRadius"), 7.f), 7.f);
	// 命中 name
	TestEqual(TEXT("hit name"),
		MAConfigLookup::GetName(Cache, TEXT("MeleeAttack"), TEXT("MuzzleSocketName"), FName(TEXT("hand_r"))),
		FName(TEXT("hand_l")));
	// name 缺键回退
	TestEqual(TEXT("missing name -> default"),
		MAConfigLookup::GetName(Cache, TEXT("MeleeAttack"), TEXT("Nope"), FName(TEXT("hand_r"))),
		FName(TEXT("hand_r")));

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
