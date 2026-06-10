#include "AI/Combat/MAAIDefensePolicy.h"

bool MAAICombat::ShouldReactToAttack(float DistanceToAttackerCm, float TimeSinceLastReaction,
	float Rand01, const FMADefenseParams& Params)
{
	if (DistanceToAttackerCm > Params.ReactRangeCm)
	{
		return false;
	}
	if (TimeSinceLastReaction < Params.CooldownSeconds)
	{
		return false;
	}
	return Rand01 < Params.Chance;
}
