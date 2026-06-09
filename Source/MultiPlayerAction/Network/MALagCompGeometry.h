#pragma once

#include "CoreMinimal.h"
#include "Network/MALagCompTypes.h"

/** Pure, world-free rewind math. Every function here is unit-tested in MALagCompGeometryTest.cpp. */
namespace MALagCompGeometry
{
	/** Squared closest distance between segment [A0,A1] and segment [B0,B1]. */
	float SegmentSegmentDistSq(const FVector& A0, const FVector& A1, const FVector& B0, const FVector& B1);

	/** True iff a sphere of radius Rs swept along [Start,End] intersects the vertical capsule
	 *  centred at Center with the given HalfHeight and capsule radius Rc. */
	bool SweptSphereVsCapsule(const FVector& Start, const FVector& End, float Rs,
		const FVector& Center, float HalfHeight, float Rc);

	/** Linear interpolation between two snapshots at absolute time T (T clamped to [Older.Time, Newer.Time]). */
	FMACapsuleSnapshot InterpolateSnapshot(const FMACapsuleSnapshot& Older, const FMACapsuleSnapshot& Newer, float T);
}
