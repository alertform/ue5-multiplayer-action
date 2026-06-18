#pragma once

#include "CoreMinimal.h"

class UGameplayAbility;
struct FMALungeParams;

/**
 * Impure Motion-Warping setup shared by the lunge/dash attacks. Sits beside the pure,
 * unit-tested target math in MAAttackLunge (and mirrors MAMeleeHitOps, the impure sweep
 * sibling): this layer touches the world (actor iteration, ASC tags) and mutates the
 * avatar's UMotionWarpingComponent, so it stays out of the unit-tested geometry file.
 */
namespace MAWarpOps
{
	/** Resolve a lunge target for the ability's avatar and push it onto the avatar's
	 *  UMotionWarpingComponent under WarpTargetName. Candidates are living combatants
	 *  (UMACombatantInterface), never self; an AI attacker never snaps onto fellow AI (same
	 *  faction rule as the damage path). Aim is the avatar pawn's BaseAimRotation yaw.
	 *   - target in cone: warp StopDistanceCm short of it and face it.
	 *   - no target, NoTargetDashCm > 0: a forced forward warp that far (caps an over-long
	 *     authored dash, e.g. GA_DashSlash).
	 *   - no target, NoTargetDashCm <= 0: REMOVE the target so the authored root motion plays
	 *     unwarped (a combo whiff's authored step-in is an acceptable lunge).
	 *  No-op without a Character avatar + MotionWarpingComponent. Runs identically on client and
	 *  server (small, prediction-measurable corrections). */
	void SetupWarpTargetForAbility(const UGameplayAbility* Ability, FName WarpTargetName,
		const FMALungeParams& Params, float NoTargetDashCm);

	/** Remove a previously-set warp target from the ability's avatar. Safe if already absent. */
	void ClearWarpTarget(const UGameplayAbility* Ability, FName WarpTargetName);
}
