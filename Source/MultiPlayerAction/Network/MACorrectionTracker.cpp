#include "Network/MACorrectionTracker.h"

void FMACorrectionTracker::Record(float Time, float ErrorCm)
{
	RecentEvents.Add(FMACorrectionEvent{ Time, ErrorCm });
	++Total;
	LastError = ErrorCm;
	MaxError = FMath::Max(MaxError, ErrorCm);

	// Trim events older than the window relative to the latest event (bounds memory).
	const float Cutoff = Time - WindowSeconds;
	int32 Trim = 0;
	while (Trim < RecentEvents.Num() && RecentEvents[Trim].Time < Cutoff)
	{
		++Trim;
	}
	if (Trim > 0)
	{
		RecentEvents.RemoveAt(0, Trim, EAllowShrinking::No);
	}
}

FMACorrectionStats FMACorrectionTracker::Stats(float Now) const
{
	// Re-filter by Now so the rate decays even when no new events arrive.
	const float Cutoff = Now - WindowSeconds;
	int32 PerSec = 0;
	for (const FMACorrectionEvent& E : RecentEvents)
	{
		if (E.Time >= Cutoff)
		{
			++PerSec;
		}
	}
	return FMACorrectionStats{ Total, PerSec, LastError, MaxError };
}
