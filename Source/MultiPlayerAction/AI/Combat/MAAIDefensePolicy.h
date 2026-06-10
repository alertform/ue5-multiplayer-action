#pragma once

#include "CoreMinimal.h"

/** Tunables for the AI guard reaction (mirrored from UMAAIDefenseComponent's UPROPERTYs). */
struct FMADefenseParams
{
	float ReactRangeCm = 350.f;     // only react to swings that can plausibly reach
	float Chance = 0.6f;            // roll per observed swing start
	float CooldownSeconds = 3.f;    // min spacing between reactions
};

namespace MAAICombat
{
	/** Pure react-or-not decision (Rand01 injected for deterministic tests).
	 *  Boundary semantics: distance == range reacts; elapsed == cooldown reacts; Rand01 < Chance
	 *  strictly (Chance 0 never reacts, Chance 1 always — FRand() is in [0,1)). */
	bool ShouldReactToAttack(float DistanceToAttackerCm, float TimeSinceLastReaction,
		float Rand01, const FMADefenseParams& Params);
}
