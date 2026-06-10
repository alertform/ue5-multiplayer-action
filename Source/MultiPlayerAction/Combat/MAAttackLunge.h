#pragma once

#include "CoreMinimal.h"

/** Tunables for dash/lunge target selection (mirrored from GA_DashSlash's UPROPERTYs). */
struct FMALungeParams
{
	float ConeHalfAngleDeg = 35.f;     // acceptance cone around the aim yaw
	float MaxLungeDistanceCm = 520.f;  // beyond this, no snap
	float StopDistanceCm = 120.f;      // land this far short of the target
};

namespace MAAttackLunge
{
	/** Index into CandidateLocations of the nearest candidate inside the aim cone and range,
	 *  or INDEX_NONE. XY only. A candidate standing on top of the attacker counts as in-cone
	 *  (direction undefined) and, being nearest, wins. Pure — unit-tested. */
	int32 PickLungeTarget(const FVector& AttackerPos, float AimYawDeg,
		const TArray<FVector>& CandidateLocations, const FMALungeParams& Params);

	/** Point StopDistanceCm short of the target on the attacker->target XY line, at the
	 *  attacker's Z. Already inside stop distance -> AttackerPos (dash compresses to zero). */
	FVector ComputeWarpPoint(const FVector& AttackerPos, const FVector& TargetPos, float StopDistanceCm);
}
