#include "Misc/AutomationTest.h"
#include "Combat/MAHitFeel.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMAHitFeelComputeTest,
	"MultiPlayerAction.HitFeel.ComputeHitFeel",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMAHitFeelComputeTest::RunTest(const FString& Parameters)
{
	// Fully-mitigated / invalid damage -> no feedback at all.
	{
		const FMAHitFeelParams P0 = MAHitFeel::ComputeHitFeel(0.f);
		TestEqual(TEXT("zero dmg hitstop"), P0.HitStopSeconds, 0.f);
		TestEqual(TEXT("zero dmg impulse"), P0.KnockbackImpulse, 0.f);
		TestEqual(TEXT("negative dmg impulse"), MAHitFeel::ComputeHitFeel(-5.f).KnockbackImpulse, 0.f);
	}

	// Default loadout lands mid-curve: 10 post-mitigation damage.
	{
		const FMAHitFeelParams P = MAHitFeel::ComputeHitFeel(10.f);
		TestEqual(TEXT("hitstop @10"), P.HitStopSeconds, 0.08f);
		TestEqual(TEXT("impulse @10"), P.KnockbackImpulse, 300.f);
	}

	// Monotonic growth with damage.
	{
		const FMAHitFeelParams A = MAHitFeel::ComputeHitFeel(5.f);
		const FMAHitFeelParams B = MAHitFeel::ComputeHitFeel(15.f);
		TestTrue(TEXT("impulse grows"), B.KnockbackImpulse > A.KnockbackImpulse);
		TestTrue(TEXT("hitstop grows"), B.HitStopSeconds > A.HitStopSeconds);
	}

	// Clamps at both ends.
	{
		TestEqual(TEXT("impulse floor"), MAHitFeel::ComputeHitFeel(1.f).KnockbackImpulse, 60.f);
		TestEqual(TEXT("impulse cap"), MAHitFeel::ComputeHitFeel(500.f).KnockbackImpulse, 600.f);
		TestEqual(TEXT("hitstop floor"), MAHitFeel::ComputeHitFeel(0.1f).HitStopSeconds, 0.0404f);
		TestEqual(TEXT("hitstop cap"), MAHitFeel::ComputeHitFeel(500.f).HitStopSeconds, 0.12f);
	}

	// Block ratio: BlockMitigation 0.7 lets 30% damage through -> exactly 30% impulse
	// in the unclamped region (10 -> 300 vs 3 -> 90).
	{
		const float Full = MAHitFeel::ComputeHitFeel(10.f).KnockbackImpulse;
		const float Blocked = MAHitFeel::ComputeHitFeel(3.f).KnockbackImpulse;
		TestEqual(TEXT("blocked impulse"), Blocked, 90.f);
		TestTrue(TEXT("blocked is 30%"), FMath::IsNearlyEqual(Blocked / Full, 0.3f, 0.001f));
	}

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
