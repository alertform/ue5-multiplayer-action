#pragma once

#include "CoreMinimal.h"
#include "MAGameplayAbilityBase.h"
#include "GA_MeleeAttack.generated.h"

class UAnimMontage;

/**
 * Melee attack ability: plays AnimMontage, does sphere trace on notify, applies damage GE.
 *
 * Combo chain (one ability instance + one multi-section montage):
 * pressing attack while a swing plays buffers the input via the GAS generic replicated
 * InputPressed event (WaitInputPress — fires on owning client AND server). At each section's
 * Event.Montage.ComboWindow notify the ability consumes the buffer and MontageJumpToSection's
 * to the next entry of ComboSections; without buffered input the montage runs out and the
 * ability ends. Section state replicates through FGameplayAbilityRepAnimMontage — a
 * misprediction corrects as a within-montage section snap, never an ability rollback.
 * A montage without matching sections (or an empty ComboSections) degrades to the legacy
 * single-swing behavior.
 */
UCLASS()
class MULTIPLAYERACTION_API UGA_MeleeAttack : public UMAGameplayAbilityBase
{
	GENERATED_BODY()

public:
	UGA_MeleeAttack();

	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;

	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		bool bReplicateEndAbility, bool bWasCancelled) override;

protected:
	/** The montage to play for this attack */
	UPROPERTY(EditDefaultsOnly, Category = "Attack")
	TObjectPtr<UAnimMontage> AttackMontage;

	/** Montage section per combo step, in chain order. The montage must contain sections with
	 *  these names; missing sections (or an empty array) mean no chaining — single swing. */
	UPROPERTY(EditDefaultsOnly, Category = "Attack|Combo")
	TArray<FName> ComboSections;

	/** Play rate multiplier — increase to shorten attack duration (e.g. 2.0 = twice as fast) */
	UPROPERTY(EditDefaultsOnly, Category = "Attack", meta = (ClampMin = "0.1", UIMin = "0.1", UIMax = "5.0"))
	float MontagePlayRate = 2.0f;

	/** Trace radius for hit detection */
	UPROPERTY(EditDefaultsOnly, Category = "Attack")
	float TraceRadius = 50.f;

	/** Trace distance forward from character */
	UPROPERTY(EditDefaultsOnly, Category = "Attack")
	float TraceDistance = 150.f;

	/** Damage GameplayEffect class to apply on hit */
	UPROPERTY(EditDefaultsOnly, Category = "Attack")
	TSubclassOf<UGameplayEffect> DamageEffect;

	// --- Per-swing Motion Warping (shared setup with GA_DashSlash via MAWarpOps) ---
	// Each swing re-resolves a lunge target and warps the authored root motion onto it, so the
	// combo closes distance instead of whiffing in place. Smaller reach than the dash: a combo
	// swing is a step-in, not a charge. Mirrors AM_KatanaCombo's per-section AttackTarget windows.

	/** Acceptance cone (half-angle, deg) around the aim yaw for picking a swing target. */
	UPROPERTY(EditDefaultsOnly, Category = "Attack|Warp")
	float ConeHalfAngleDeg = 35.f;

	/** Beyond this, a combo swing does not snap (a step-in, not a dash). */
	UPROPERTY(EditDefaultsOnly, Category = "Attack|Warp")
	float MaxLungeDistanceCm = 350.f;

	/** Land this far short of the target — keeps the swing at striking reach, not inside it. */
	UPROPERTY(EditDefaultsOnly, Category = "Attack|Warp")
	float StopDistanceCm = 120.f;

	/** Forward warp when no target is in the cone. 0 (default) = let the authored 1.4–3.4 m
	 *  swing step-in play unwarped (an acceptable whiff lunge); >0 caps it like the dash. */
	UPROPERTY(EditDefaultsOnly, Category = "Attack|Warp")
	float NoTargetDashCm = 0.f;

	/** Called when montage hits the "Attack" notify window */
	UFUNCTION()
	void OnMontageEvent(FGameplayEventData EventData);

	/** Called when montage completes or is interrupted */
	UFUNCTION()
	void OnMontageEnded();

	/** Combo decision point — Event.Montage.ComboWindow notify near each section's end. */
	UFUNCTION()
	void OnComboWindow(FGameplayEventData EventData);

	/** Attack re-press while the ability is active (WaitInputPress, replicated to server). */
	UFUNCTION()
	void OnComboInputPressed(float TimeWaited);

private:
	/** Perform sphere trace and apply damage to hit targets */
	void PerformHitTrace(const FGameplayAbilityActorInfo* ActorInfo);

	/** Resolve and push this swing's Motion-Warping target ("AttackTarget"). Called on activate
	 *  (first swing) and again on every chained swing so each one re-aims at the current foe. */
	void SetupWarpTarget();

	/** One-shot WaitInputPress task; re-armed from its own callback (continuous listening). */
	void ArmComboInputTask();

	/** Consumes one queued press and jumps to the next section when the window is open. */
	void TryAdvanceCombo();

	/** Current swing index into ComboSections — instance state, reset each activation. */
	int32 ComboIndex = 0;

	/** Press QUEUE, not a boolean: mashing N times mid-swing must yield N chained swings.
	 *  Each chained swing consumes one press; capped so spam can't bank more than the chain holds. */
	int32 BufferedComboPresses = 0;

	/** Window-PERIOD semantics: the ComboWindow notify OPENS the window (placed just after the
	 *  hit frame); from then until the section's blend-out a press chains INSTANTLY — cancelling
	 *  the swing's recovery. A reactive player who clicks late in the swing must not be punished
	 *  by an instant-checkpoint window. Presses before the window queue up and chain at open. */
	bool bComboWindowOpen = false;

	/** Motion-Warping target name — must match AM_KatanaCombo's per-section warp windows. */
	static const FName WarpTargetName;
};
