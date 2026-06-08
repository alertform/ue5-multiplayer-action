#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "MALockOnComponent.generated.h"

class ACharacter;
class UMALockOnReticleWidget;

/**
 * Soft-lock targeting for the locally-controlled player. Lives on the character as a
 * default subobject but acts only on the autonomous proxy (IsLocallyControlled) — pure
 * local camera/UI concern, nothing replicated.
 *
 * Toggle acquires the most centered combatant inside an acquisition cone; while locked the
 * character strafes facing the target (controller-yaw orientation) and the camera tracks it.
 * A right-stick / mouse flick steps to the next target on that side. Lock auto-drops when the
 * target dies (State.Dead) or leaves range.
 *
 * "Lockable" = any actor implementing IMACombatantInterface (players + dummies) whose ASC
 * does not own State.Dead.
 */
UCLASS(ClassGroup = (MultiPlayerAction), meta = (BlueprintSpawnableComponent))
class MULTIPLAYERACTION_API UMALockOnComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UMALockOnComponent();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	/** Acquire the best target if free, release if already locked. Local player only. */
	void ToggleLock();

	/** A lock is held on a valid (still-alive) target. */
	bool IsLocked() const { return CurrentTarget.IsValid(); }

	AActor* GetCurrentTarget() const { return CurrentTarget.Get(); }

	/** Immediately step to the adjacent target on the given side (DirSign > 0 = camera-right). */
	void SwitchTarget(float DirSign);

	/** Routed from the character's Look input. While locked, a horizontal flick switches target
	 *  (debounced) and the look input is consumed so the camera stays on the target. Returns
	 *  true when the input was consumed (caller should skip free-look). */
	bool HandleLookInput(const FVector2D& AxisValue);

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	// ---- tuning ----
	UPROPERTY(EditAnywhere, Category = "LockOn")
	float MaxLockDistance = 1800.f;

	/** Half-angle of the cone (from camera forward) used when first acquiring a target. */
	UPROPERTY(EditAnywhere, Category = "LockOn")
	float AcquireHalfAngleDeg = 55.f;

	/** Extra distance the held target may drift before the lock breaks (hysteresis). */
	UPROPERTY(EditAnywhere, Category = "LockOn")
	float BreakDistancePadding = 300.f;

	UPROPERTY(EditAnywhere, Category = "LockOn")
	float CameraInterpSpeed = 9.f;

	/** Downward framing bias added to the look-at pitch. */
	UPROPERTY(EditAnywhere, Category = "LockOn")
	float LockedPitchBiasDeg = -8.f;

	UPROPERTY(EditAnywhere, Category = "LockOn")
	float MinPitchDeg = -45.f;

	UPROPERTY(EditAnywhere, Category = "LockOn")
	float MaxPitchDeg = 18.f;

	/** Stick magnitude that arms a target switch / that must release before re-arming. */
	UPROPERTY(EditAnywhere, Category = "LockOn")
	float SwitchInputThreshold = 0.65f;

	UPROPERTY(EditAnywhere, Category = "LockOn")
	float SwitchReleaseThreshold = 0.25f;

	UPROPERTY(EditAnywhere, Category = "LockOn")
	float SwitchCooldown = 0.3f;

	/** Vertical lift from actor origin toward the upper torso, for aim + reticle. */
	UPROPERTY(EditAnywhere, Category = "LockOn")
	float TargetVerticalOffset = 40.f;

	/** Reticle widget class — defaults to the code-built UMALockOnReticleWidget (no BP needed). */
	UPROPERTY(EditDefaultsOnly, Category = "LockOn")
	TSubclassOf<UMALockOnReticleWidget> ReticleWidgetClass;

	// ---- state ----
	TWeakObjectPtr<AActor> CurrentTarget;

	UPROPERTY()
	TObjectPtr<UMALockOnReticleWidget> Reticle;

	bool bSwitchArmed = true;
	float LastSwitchTime = -100.f;

	// Character orientation flags saved on lock, restored on release.
	bool bSavedOrientToMovement = true;
	bool bSavedUseControllerYaw = false;

	// ---- helpers ----
	ACharacter* GetOwnerCharacter() const;
	bool OwnerIsLocallyControlled() const;

	void SetLock(AActor* NewTarget);   // enters strafe mode, then adopts target
	void AdoptTarget(AActor* NewTarget); // swaps the held target without touching flags
	void ClearLock();
	bool IsTargetStillValid() const;

	AActor* FindBestTarget() const;
	AActor* FindAdjacentTarget(float DirSign) const;
	void GatherCandidates(TArray<AActor*>& Out) const;
	bool IsActorLockable(const AActor* Actor) const;
	static bool IsActorAlive(const AActor* Actor);
	FVector GetTargetFocusLocation(const AActor* Actor) const;

	void UpdateCameraToTarget(float DeltaTime);
	void EnsureReticle();
};
