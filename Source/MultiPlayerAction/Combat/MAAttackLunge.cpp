#include "Combat/MAAttackLunge.h"

int32 MAAttackLunge::PickLungeTarget(const FVector& AttackerPos, float AimYawDeg,
	const TArray<FVector>& CandidateLocations, const FMALungeParams& Params)
{
	const FVector AimDir(FMath::Cos(FMath::DegreesToRadians(AimYawDeg)),
		FMath::Sin(FMath::DegreesToRadians(AimYawDeg)), 0.f);
	const float MinCos = FMath::Cos(FMath::DegreesToRadians(Params.ConeHalfAngleDeg));

	int32 Best = INDEX_NONE;
	float BestDistSq = TNumericLimits<float>::Max();
	for (int32 i = 0; i < CandidateLocations.Num(); ++i)
	{
		FVector To = CandidateLocations[i] - AttackerPos;
		To.Z = 0.f;
		const float DistSq = To.SizeSquared();
		if (DistSq > Params.MaxLungeDistanceCm * Params.MaxLungeDistanceCm)
		{
			continue;
		}
		// Zero distance: direction undefined -> treat as in-cone.
		if (To.Normalize() && FVector::DotProduct(To, AimDir) < MinCos)
		{
			continue;
		}
		if (DistSq < BestDistSq)
		{
			BestDistSq = DistSq;
			Best = i;
		}
	}
	return Best;
}

FVector MAAttackLunge::ComputeWarpPoint(const FVector& AttackerPos, const FVector& TargetPos, float StopDistanceCm)
{
	FVector To = TargetPos - AttackerPos;
	To.Z = 0.f;
	const float Dist = To.Size();
	if (Dist <= StopDistanceCm || !To.Normalize())
	{
		return AttackerPos;
	}
	FVector Result = AttackerPos + To * (Dist - StopDistanceCm);
	Result.Z = AttackerPos.Z;
	return Result;
}
