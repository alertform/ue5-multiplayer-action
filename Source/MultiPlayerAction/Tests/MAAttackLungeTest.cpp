#include "Misc/AutomationTest.h"
#include "Combat/MAAttackLunge.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMAAttackLungeTest,
	"MultiPlayerAction.Lunge.TargetSelection",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMAAttackLungeTest::RunTest(const FString& Parameters)
{
	const FMALungeParams P; // 35deg half-angle, 520cm max, 120cm stop
	const FVector Origin(0, 0, 90);

	// Nearest in-cone candidate wins (aim along +X).
	{
		TArray<FVector> C = { FVector(400, 0, 90), FVector(200, 50, 90), FVector(300, -40, 90) };
		TestEqual(TEXT("nearest in cone"), MAAttackLunge::PickLungeTarget(Origin, 0.f, C, P), 1);
	}

	// Outside the cone is excluded even when nearest (90 degrees off-axis).
	{
		TArray<FVector> C = { FVector(0, 150, 90), FVector(400, 0, 90) };
		TestEqual(TEXT("off-axis excluded"), MAAttackLunge::PickLungeTarget(Origin, 0.f, C, P), 1);
	}

	// Beyond max distance excluded; nothing valid -> INDEX_NONE.
	{
		TArray<FVector> C = { FVector(600, 0, 90) };
		TestEqual(TEXT("too far"), MAAttackLunge::PickLungeTarget(Origin, 0.f, C, P), (int32)INDEX_NONE);
		TestEqual(TEXT("empty"), MAAttackLunge::PickLungeTarget(Origin, 0.f, TArray<FVector>(), P), (int32)INDEX_NONE);
	}

	// Yaw wraparound: aiming 170deg, target at 175deg (in cone across the 180 seam).
	{
		const FVector T(FMath::Cos(FMath::DegreesToRadians(175.f)) * 300.f,
			FMath::Sin(FMath::DegreesToRadians(175.f)) * 300.f, 90.f);
		TArray<FVector> C = { T };
		TestEqual(TEXT("yaw wraparound"), MAAttackLunge::PickLungeTarget(Origin, 170.f, C, P), 0);
	}

	// Overlapping candidate (zero distance) counts as in-cone and wins.
	{
		TArray<FVector> C = { FVector(300, 0, 90), Origin };
		TestEqual(TEXT("overlap wins"), MAAttackLunge::PickLungeTarget(Origin, 0.f, C, P), 1);
	}

	// Warp point: lands exactly StopDistance short, at attacker Z.
	{
		const FVector W = MAAttackLunge::ComputeWarpPoint(Origin, FVector(400, 0, 150), 120.f);
		TestEqual(TEXT("warp x"), W.X, 280.0);
		TestEqual(TEXT("warp y"), W.Y, 0.0);
		TestEqual(TEXT("warp z = attacker z"), W.Z, 90.0);
	}

	// Inside stop distance -> attacker position (no dash).
	{
		const FVector W = MAAttackLunge::ComputeWarpPoint(Origin, FVector(80, 0, 90), 120.f);
		TestEqual(TEXT("inside stop"), W, Origin);
		TestEqual(TEXT("degenerate same pos"), MAAttackLunge::ComputeWarpPoint(Origin, Origin, 120.f), Origin);
	}

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
