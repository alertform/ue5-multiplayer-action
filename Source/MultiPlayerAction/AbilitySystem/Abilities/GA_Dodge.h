#pragma once

#include "CoreMinimal.h"
#include "MAGameplayAbilityBase.h"
#include "GA_Dodge.generated.h"

/**
 * Brief invincible dash. While active:
 *  - Avatar gets a forward (or input-direction) launch impulse via LaunchCharacter
 *  - ActivationOwnedTags grants State.Dodging to the ASC
 *  - BP_GE_Damage's ApplicationTagRequirements.IgnoreTags = State.Dodging → damage GE
 *    rejected at apply time, granting i-frame for the dodge duration
 *  - Auto-ends after DodgeDuration (timer-driven)
 *
 * Cancels active sprint via CancelAbilitiesWithTag = Ability.Movement.Sprint.
 * Blocked while dead (State.Dead).
 */
UCLASS()
class MULTIPLAYERACTION_API UGA_Dodge : public UMAGameplayAbilityBase
{
	GENERATED_BODY()

public:
	UGA_Dodge();

	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;

	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		bool bReplicateEndAbility, bool bWasCancelled) override;

protected:
	UPROPERTY(EditDefaultsOnly, Category = "Dodge")
	float DodgeImpulse = 800.f;

	UPROPERTY(EditDefaultsOnly, Category = "Dodge", meta = (ClampMin = "0.1"))
	float DodgeDuration = 0.4f;

private:
	FTimerHandle EndDodgeTimer;
	void EndDodge();
};
