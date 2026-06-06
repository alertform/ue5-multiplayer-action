#pragma once

#include "CoreMinimal.h"
#include "MAGameplayAbilityBase.h"
#include "GA_MeleeAttack.generated.h"

class UAnimMontage;

/**
 * Melee attack ability: plays AnimMontage, does sphere trace on notify, applies damage GE.
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

	/** Called when montage hits the "Attack" notify window */
	UFUNCTION()
	void OnMontageEvent(FGameplayEventData EventData);

	/** Called when montage completes or is interrupted */
	UFUNCTION()
	void OnMontageEnded();

private:
	/** Perform sphere trace and apply damage to hit targets */
	void PerformHitTrace(const FGameplayAbilityActorInfo* ActorInfo);
};
