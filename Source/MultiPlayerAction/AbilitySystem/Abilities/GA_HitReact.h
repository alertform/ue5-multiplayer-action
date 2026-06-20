#pragma once

#include "CoreMinimal.h"
#include "MAGameplayAbilityBase.h"
#include "GA_HitReact.generated.h"

class UAnimMontage;

/**
 * Flinch on taking damage. ServerInitiated + GameplayEvent-triggered: the AttributeSet raises
 * Event.Damage.Taken when damage lands on a living target (server-side), this ability plays a
 * brief upper-body react montage which replicates to every client via the ASC montage path.
 *
 * Deliberately NOT allowed to interrupt the target's own committed actions: blocked while
 * attacking / casting / dodging / dead — light reacts are feedback, not a stun.
 */
UCLASS()
class MULTIPLAYERACTION_API UGA_HitReact : public UMAGameplayAbilityBase
{
	GENERATED_BODY()

public:
	UGA_HitReact();

	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;

	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		bool bReplicateEndAbility, bool bWasCancelled) override;

protected:
	/** React montage pool — the server picks one at random per hit (selection replicates
	 *  implicitly through the ASC montage path). Configure in BP defaults (Hit_A/Hit_B). */
	UPROPERTY(EditDefaultsOnly, Category = "HitReact")
	TArray<TObjectPtr<UAnimMontage>> HitReactMontages;

	/** Play-rate multiplier for the react montage. */
	UPROPERTY(EditDefaultsOnly, Category = "HitReact", meta = (ClampMin = "0.1", UIMax = "5.0"))
	float PlayRate = 1.0f;

	UFUNCTION()
	void OnMontageFinished();
};
