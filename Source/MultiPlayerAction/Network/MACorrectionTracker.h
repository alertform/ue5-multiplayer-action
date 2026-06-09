#pragma once

#include "CoreMinimal.h"
#include "Network/MAPredictionTypes.h"

/** Pure, world-free aggregator of CMC server-correction events. Unit-tested in MACorrectionTrackerTest.cpp. */
class FMACorrectionTracker
{
public:
	/** Record a correction: appends the event, bumps total, tracks last/max, trims old events for memory. */
	void Record(float Time, float ErrorCm);

	/** Snapshot at time Now: total, per-second rate over the last window, last and max error. */
	FMACorrectionStats Stats(float Now) const;

private:
	static constexpr float WindowSeconds = 1.0f;
	TArray<FMACorrectionEvent> RecentEvents; // time-ascending, kept within ~WindowSeconds of the latest Record
	int32 Total = 0;
	float LastError = 0.f;
	float MaxError = 0.f;
};
