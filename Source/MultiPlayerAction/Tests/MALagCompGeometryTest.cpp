#include "Misc/AutomationTest.h"
#include "Network/MALagCompGeometry.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMALagCompSegmentDistTest,
	"MultiPlayerAction.LagComp.SegmentSegmentDistSq",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMALagCompSegmentDistTest::RunTest(const FString& Parameters)
{
	using namespace MALagCompGeometry;

	// Crossing segments through the origin -> distance 0.
	TestEqual(TEXT("crossing"), SegmentSegmentDistSq(
		FVector(-50, 0, 0), FVector(50, 0, 0),
		FVector(0, -50, 0), FVector(0, 50, 0)), 0.f);

	// Parallel segments 50 apart on Y -> dist^2 == 2500.
	TestEqual(TEXT("parallel"), SegmentSegmentDistSq(
		FVector(0, 0, 0), FVector(100, 0, 0),
		FVector(0, 50, 0), FVector(100, 50, 0)), 2500.f);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMALagCompSweptSphereTest,
	"MultiPlayerAction.LagComp.SweptSphereVsCapsule",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMALagCompSweptSphereTest::RunTest(const FString& Parameters)
{
	using namespace MALagCompGeometry;

	// Sphere path runs straight through the capsule axis at (0,0) -> hit.
	TestTrue(TEXT("through axis"), SweptSphereVsCapsule(
		FVector(-50, 0, 0), FVector(50, 0, 0), 50.f,
		FVector(0, 0, 0), 88.f, 34.f));

	// Same sweep, capsule far away on Y (gap 300 > 34+50) -> miss.
	TestFalse(TEXT("far away"), SweptSphereVsCapsule(
		FVector(-50, 0, 0), FVector(50, 0, 0), 50.f,
		FVector(0, 300, 0), 88.f, 34.f));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMALagCompInterpTest,
	"MultiPlayerAction.LagComp.InterpolateSnapshot",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMALagCompInterpTest::RunTest(const FString& Parameters)
{
	using namespace MALagCompGeometry;

	FMACapsuleSnapshot A; A.Time = 0.f; A.Center = FVector(0, 0, 0);   A.HalfHeight = 88.f; A.Radius = 34.f;
	FMACapsuleSnapshot B; B.Time = 1.f; B.Center = FVector(0, 100, 0); B.HalfHeight = 88.f; B.Radius = 34.f;

	// Midpoint.
	const FMACapsuleSnapshot Mid = InterpolateSnapshot(A, B, 0.5f);
	TestTrue(TEXT("mid center"), Mid.Center.Equals(FVector(0, 50, 0), 0.01f));

	// Clamp below range -> A.
	const FMACapsuleSnapshot Lo = InterpolateSnapshot(A, B, -5.f);
	TestTrue(TEXT("clamp low"), Lo.Center.Equals(FVector(0, 0, 0), 0.01f));

	// Clamp above range -> B.
	const FMACapsuleSnapshot Hi = InterpolateSnapshot(A, B, 5.f);
	TestTrue(TEXT("clamp high"), Hi.Center.Equals(FVector(0, 100, 0), 0.01f));

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
