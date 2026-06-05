#pragma once

#include "CoreMinimal.h"
#include "MAGameplayAbilityBase.h"
#include "GA_Fireball.generated.h"

class UAnimMontage;
class AMAProjectile;

/**
 * Fireball ability: predicted cast montage; on the montage's release notify the SERVER spawns
 * a replicated AMAProjectile carrying a damage spec snapshotted at cast time.
 * Net model: LocalPredicted activation (instant cast start on the owning client) +
 * server-authoritative projectile + AoE damage.
 */
UCLASS()
class MULTIPLAYERACTION_API UGA_Fireball : public UMAGameplayAbilityBase
{
	GENERATED_BODY()

public:
	UGA_Fireball();

	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;

	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		bool bReplicateEndAbility, bool bWasCancelled) override;

protected:
	/** Cast montage — its release frame carries AN_SendGameplayEvent(Event.Montage.SpawnProjectile) */
	UPROPERTY(EditDefaultsOnly, Category = "Fireball")
	TObjectPtr<UAnimMontage> CastMontage;

	/** Play rate multiplier for the cast montage */
	UPROPERTY(EditDefaultsOnly, Category = "Fireball", meta = (ClampMin = "0.1", UIMin = "0.1", UIMax = "5.0"))
	float MontagePlayRate = 1.0f;

	/** Replicated projectile class — BP child of AMAProjectile carrying the visuals */
	UPROPERTY(EditDefaultsOnly, Category = "Fireball")
	TSubclassOf<AMAProjectile> ProjectileClass;

	/** Damage GE applied to every ASC inside the explosion radius (reuse BP_GE_Damage / ExecCalc) */
	UPROPERTY(EditDefaultsOnly, Category = "Fireball")
	TSubclassOf<UGameplayEffect> DamageEffect;

	/** AoE radius around the impact point */
	UPROPERTY(EditDefaultsOnly, Category = "Fireball")
	float ExplosionRadius = 300.f;

	/** Mesh socket/bone the projectile spawns from; falls back to actor location + forward offset */
	UPROPERTY(EditDefaultsOnly, Category = "Fireball")
	FName MuzzleSocketName = TEXT("hand_r");

	/** Called when the montage reaches the release notify */
	UFUNCTION()
	void OnMontageEvent(FGameplayEventData EventData);

	/** Called when montage completes or is interrupted */
	UFUNCTION()
	void OnMontageEnded();

private:
	/** Server-only: snapshot the damage spec + deferred-spawn the projectile toward BaseAimRotation */
	void SpawnProjectile(const FGameplayAbilityActorInfo* ActorInfo);
};
