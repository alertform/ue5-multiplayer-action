#include "Misc/AutomationTest.h"
#include "Network/MACorrectionTracker.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMACorrectionTrackerTest,
	"MultiPlayerAction.Prediction.CorrectionTracker",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMACorrectionTrackerTest::RunTest(const FString& Parameters)
{
	FMACorrectionTracker T;
	T.Record(0.0f, 5.f);
	T.Record(0.1f, 10.f);
	T.Record(0.2f, 3.f);

	// All three within the 1s window ending at 0.2.
	const FMACorrectionStats S = T.Stats(0.2f);
	TestEqual(TEXT("total"), S.TotalCorrections, 3);
	TestEqual(TEXT("persec all"), S.CorrectionsPerSec, 3);
	TestEqual(TEXT("last"), S.LastErrorCm, 3.f);
	TestEqual(TEXT("max"), S.MaxErrorCm, 10.f);

	// Query well past the window: rate falls to 0, totals/max/last persist.
	const FMACorrectionStats S2 = T.Stats(2.0f);
	TestEqual(TEXT("persec stale"), S2.CorrectionsPerSec, 0);
	TestEqual(TEXT("total persists"), S2.TotalCorrections, 3);
	TestEqual(TEXT("max persists"), S2.MaxErrorCm, 10.f);

	// A fresh correction much later: only it is within the window now.
	T.Record(2.0f, 8.f);
	const FMACorrectionStats S3 = T.Stats(2.0f);
	TestEqual(TEXT("persec one recent"), S3.CorrectionsPerSec, 1);
	TestEqual(TEXT("total now 4"), S3.TotalCorrections, 4);
	TestEqual(TEXT("last now 8"), S3.LastErrorCm, 8.f);
	TestEqual(TEXT("max still 10"), S3.MaxErrorCm, 10.f);

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
