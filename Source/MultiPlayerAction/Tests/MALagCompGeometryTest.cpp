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

// Build a synthetic strafe: target moves +Y at 600 cm/s, snapshots every 0.1s over [0,1]s.
static TArray<FMACapsuleSnapshot> MakeStrafeHistory()
{
	TArray<FMACapsuleSnapshot> H;
	for (int32 i = 0; i <= 10; ++i)
	{
		FMACapsuleSnapshot S;
		S.Time = i * 0.1f;
		S.Center = FVector(0, 600.f * S.Time, 0);
		S.HalfHeight = 88.f;
		S.Radius = 34.f;
		H.Add(S);
	}
	return H;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMALagCompSampleHistoryTest,
	"MultiPlayerAction.LagComp.SampleHistory",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMALagCompSampleHistoryTest::RunTest(const FString& Parameters)
{
	using namespace MALagCompGeometry;
	const TArray<FMACapsuleSnapshot> H = MakeStrafeHistory();

	FMACapsuleSnapshot Out;
	TestTrue(TEXT("sampled"), SampleHistory(H, 0.5f, Out));
	TestTrue(TEXT("mid pos"), Out.Center.Equals(FVector(0, 300, 0), 0.5f));

	// Clamp before oldest.
	TestTrue(TEXT("clamp lo ok"), SampleHistory(H, -1.f, Out));
	TestTrue(TEXT("clamp lo pos"), Out.Center.Equals(FVector(0, 0, 0), 0.5f));

	// Clamp after newest.
	TestTrue(TEXT("clamp hi ok"), SampleHistory(H, 99.f, Out));
	TestTrue(TEXT("clamp hi pos"), Out.Center.Equals(FVector(0, 600, 0), 0.5f));

	// Empty -> false.
	TArray<FMACapsuleSnapshot> Empty;
	TestFalse(TEXT("empty"), SampleHistory(Empty, 0.5f, Out));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMALagCompResolveTest,
	"MultiPlayerAction.LagComp.ResolveRewoundHit",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMALagCompResolveTest::RunTest(const FString& Parameters)
{
	using namespace MALagCompGeometry;
	const TArray<FMACapsuleSnapshot> H = MakeStrafeHistory();

	// Attacker swings a sphere through Y=300 (where the target was at t=0.5).
	const FVector Start(-50, 300, 0);
	const FVector End(150, 300, 0);
	const float R = 50.f;

	// Rewound to t=0.5 -> the capsule was at Y=300 -> HIT.
	FVector Center;
	TestTrue(TEXT("hit rewound"), ResolveRewoundHit(H, 0.5f, Start, End, R, Center));
	TestTrue(TEXT("rewound center"), Center.Equals(FVector(0, 300, 0), 0.5f));

	// The SAME swing tested against the CURRENT position (Y=600) must MISS.
	TestFalse(TEXT("miss current"),
		SweptSphereVsCapsule(Start, End, R, FVector(0, 600, 0), 88.f, 34.f));

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
