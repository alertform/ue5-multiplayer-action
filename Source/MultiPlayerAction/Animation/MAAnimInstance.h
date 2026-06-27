#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "MAAnimInstance.generated.h"

/**
 * AnimInstance base for ABP_Manny: drives the upper-body aim twist.
 *
 * While an aim-facing ability is active (State.Attacking / State.Casting on the owner's ASC),
 * SpineAimRotation interpolates toward the camera-vs-body yaw delta, divided across the spine
 * chain. The AnimGraph applies it as additive mesh-space rotation on spine_01..03 AFTER the
 * layered blend — so the upper body turns toward the camera while the legs keep following
 * orient-to-movement (the actor itself is never snapped).
 *
 * AI pawns share this class safely: GetBaseAimRotation falls back to the actor rotation,
 * making the delta zero.
 */
UCLASS()
class MULTIPLAYERACTION_API UMAAnimInstance : public UAnimInstance
{
	GENERATED_BODY()

public:
	virtual void NativeUpdateAnimation(float DeltaSeconds) override;

	/** Per-bone additive rotation for the spine chain (yaw delta already divided by NumSpineBones).
	 *  Read by the AnimGraph's Transform (Modify) Bone nodes. */
	UPROPERTY(BlueprintReadOnly, Category = "Aim")
	FRotator SpineAimRotation = FRotator::ZeroRotator;

	/** Foot-IK trace gate read by the AnimGraph Control Rig node. False while airborne OR while a
	 *  full-body (DefaultSlot) montage owns the pose — so attack swings keep their authored foot
	 *  lifts instead of being ground-locked, which would slide the planted feet as root motion moves
	 *  the body. True only when grounded locomotion should adapt feet to the floor. */
	UPROPERTY(BlueprintReadOnly, Category = "IK")
	bool bShouldDoFootIKTrace = false;

	/** True while the active montage plays on DefaultSlot (full-body — the katana combo / dash). */
	UPROPERTY(BlueprintReadOnly, Category = "IK")
	bool bFullBodyMontageActive = false;

	/** Signed angle (−180..180) of velocity relative to facing: 0 ahead, ±90 strafing, ±180
	 *  backpedalling. Drives the katana 8-way strafe blendspace. Derived from replicated velocity +
	 *  rotation, so simulated proxies get it for free. */
	UPROPERTY(BlueprintReadOnly, Category = "Locomotion")
	float Direction = 0.f;

	/** True when the owner is in lock-on strafe mode AND moving — gates the AnimGraph's
	 *  BlendPosesByBool between the orient-to-movement free-run blendspace (false) and the 8-way
	 *  strafe blendspace (true). Driven by the replicated AMultiPlayerActionCharacter::IsStrafeMode
	 *  flag plus a speed gate, so it stays correct on simulated proxies. */
	UPROPERTY(BlueprintReadOnly, Category = "Locomotion")
	bool bStrafing = false;

protected:
	/** Max torso twist in degrees (camera further off-axis than this gets clamped) */
	UPROPERTY(EditDefaultsOnly, Category = "Aim", meta = (ClampMin = "0.0", UIMax = "120.0"))
	float MaxAimYaw = 90.f;

	/** Interp speed toward/away from the aim delta — keeps the twist from popping */
	UPROPERTY(EditDefaultsOnly, Category = "Aim", meta = (ClampMin = "0.1", UIMax = "30.0"))
	float AimInterpSpeed = 12.f;

	/** Number of spine bones the twist is spread across (must match the AnimGraph node count) */
	UPROPERTY(EditDefaultsOnly, Category = "Aim", meta = (ClampMin = "1", UIMax = "5"))
	int32 NumSpineBones = 3;

	/** Min planar speed (cm/s) before strafe can engage — keeps a locked-on but near-stationary
	 *  character on the idle/free-run pose instead of a zero-speed strafe. */
	UPROPERTY(EditDefaultsOnly, Category = "Locomotion", meta = (ClampMin = "0.0"))
	float StrafeSpeedThreshold = 10.f;

private:
	/** Smoothed full-chain yaw delta */
	float CurrentAimYaw = 0.f;
};
