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

private:
	/** Smoothed full-chain yaw delta */
	float CurrentAimYaw = 0.f;
};
