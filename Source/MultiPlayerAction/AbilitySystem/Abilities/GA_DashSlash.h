#pragma once

#include "CoreMinimal.h"
#include "MAGameplayAbilityBase.h"
#include "GA_DashSlash.generated.h"

class UAnimMontage;

/**
 * Motion-Warped root-motion dash attack. On activate (client-predicted AND server):
 * deterministic cone pick of the nearest aimed-at combatant -> warp target "AttackTarget"
 * clamped StopDistance short of the victim -> full-body root-motion montage whose
 * UAnimNotifyState_MotionWarping window (SkewWarp) bends the authored dash onto the target.
 * No target -> short fixed forward warp (a whiff never travels the full authored distance).
 * Damage on Event.Montage.Hit through MAMeleeHitOps (lag comp, faction filter, impact cue).
 */
UCLASS()
class MULTIPLAYERACTION_API UGA_DashSlash : public UMAGameplayAbilityBase
{
	GENERATED_BODY()

public:
	UGA_DashSlash();

	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;

	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		bool bReplicateEndAbility, bool bWasCancelled) override;

protected:
	/** Full-body root-motion montage (AM_DashSlash) with the MotionWarping window. */
	UPROPERTY(EditDefaultsOnly, Category = "DashSlash")
	TObjectPtr<UAnimMontage> DashMontage;

	UPROPERTY(EditDefaultsOnly, Category = "DashSlash")
	float ConeHalfAngleDeg = 35.f;

	UPROPERTY(EditDefaultsOnly, Category = "DashSlash")
	float MaxLungeDistanceCm = 520.f;

	UPROPERTY(EditDefaultsOnly, Category = "DashSlash")
	float StopDistanceCm = 120.f;

	/** Forward warp distance when no target is in the cone (caps the authored 3.8m travel). */
	UPROPERTY(EditDefaultsOnly, Category = "DashSlash")
	float NoTargetDashCm = 250.f;

	/** Strike sweep, same semantics as GA_MeleeAttack. */
	UPROPERTY(EditDefaultsOnly, Category = "DashSlash")
	float TraceDistance = 180.f;

	UPROPERTY(EditDefaultsOnly, Category = "DashSlash")
	float TraceRadius = 60.f;

	UPROPERTY(EditDefaultsOnly, Category = "DashSlash")
	TSubclassOf<UGameplayEffect> DamageEffect;

	UFUNCTION()
	void OnHitEvent(FGameplayEventData EventData);

	UFUNCTION()
	void OnMontageFinished();

private:
	void SetupWarpTarget();
	void ClearWarpTarget();

	static const FName WarpTargetName;
};
