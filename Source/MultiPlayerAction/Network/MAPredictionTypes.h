#pragma once

#include "CoreMinimal.h"

/** One server-correction (reconciliation) event on the autonomous proxy. */
struct FMACorrectionEvent
{
	float Time = 0.f;      // client world time the correction was applied, seconds
	float ErrorCm = 0.f;   // distance between predicted and server-authoritative location
};

/** Aggregated correction stats for the on-screen readout. */
struct FMACorrectionStats
{
	int32 TotalCorrections = 0;
	int32 CorrectionsPerSec = 0;  // count within the last window (1s)
	float LastErrorCm = 0.f;
	float MaxErrorCm = 0.f;
};
