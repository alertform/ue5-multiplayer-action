#include "Misc/AutomationTest.h"
#include "AI/Combat/MAAIDefensePolicy.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMAAIDefensePolicyTest,
	"MultiPlayerAction.AICombat.DefensePolicy",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMAAIDefensePolicyTest::RunTest(const FString& Parameters)
{
	const FMADefenseParams P; // range 350, chance 0.6, cooldown 3

	// In range, off cooldown, roll under chance -> react.
	TestTrue(TEXT("reacts"), MAAICombat::ShouldReactToAttack(200.f, 100.f, 0.3f, P));

	// Out of range never reacts, even with a winning roll.
	TestFalse(TEXT("out of range"), MAAICombat::ShouldReactToAttack(351.f, 100.f, 0.0f, P));
	// Boundary: exactly at range still reacts.
	TestTrue(TEXT("at range"), MAAICombat::ShouldReactToAttack(350.f, 100.f, 0.3f, P));

	// Within cooldown never reacts.
	TestFalse(TEXT("on cooldown"), MAAICombat::ShouldReactToAttack(200.f, 2.9f, 0.0f, P));
	// Boundary: exactly at cooldown reacts.
	TestTrue(TEXT("cooldown elapsed"), MAAICombat::ShouldReactToAttack(200.f, 3.f, 0.3f, P));

	// Roll at/above chance fails; strictly below succeeds.
	TestFalse(TEXT("roll == chance fails"), MAAICombat::ShouldReactToAttack(200.f, 100.f, 0.6f, P));
	TestTrue(TEXT("roll below chance"), MAAICombat::ShouldReactToAttack(200.f, 100.f, 0.59f, P));

	// Chance extremes.
	FMADefenseParams Never = P;  Never.Chance = 0.f;
	FMADefenseParams Always = P; Always.Chance = 1.f;
	TestFalse(TEXT("chance 0 never"), MAAICombat::ShouldReactToAttack(200.f, 100.f, 0.0f, Never));
	TestTrue(TEXT("chance 1 always"), MAAICombat::ShouldReactToAttack(200.f, 100.f, 0.999f, Always));

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
