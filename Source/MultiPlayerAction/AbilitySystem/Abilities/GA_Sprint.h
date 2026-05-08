#pragma once

#include "CoreMinimal.h"
#include "MAGameplayAbilityBase.h"
#include "GA_Sprint.generated.h"

/**
 * Hold-to-activate sprint ability. While active:
 *  - Boosts CharacterMovement.MaxWalkSpeed to SprintSpeed
 *  - Drains Stamina by StaminaDrainPerTick every DrainInterval seconds
 *  - Auto-ends when Stamina hits 0 (via tick check)
 *
 * Cancelled by GA_MeleeAttack on attack input (CancelAbilitiesWithTag = Ability.Movement.Sprint).
 * Released by Character::OnSprintReleased via ASC.CancelAbilities.
 */
UCLASS()
class MULTIPLAYERACTION_API UGA_Sprint : public UMAGameplayAbilityBase
{
	GENERATED_BODY()

public:
	UGA_Sprint();

	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;

	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		bool bReplicateEndAbility, bool bWasCancelled) override;

protected:
	UPROPERTY(EditDefaultsOnly, Category = "Sprint")
	float SprintSpeed = 800.f;

	UPROPERTY(EditDefaultsOnly, Category = "Sprint")
	float StaminaDrainPerTick = 5.f;

	UPROPERTY(EditDefaultsOnly, Category = "Sprint", meta = (ClampMin = "0.05"))
	float DrainInterval = 0.5f;

private:
	float OriginalWalkSpeed = 600.f;
	FTimerHandle DrainTimer;

	void TickStaminaDrain();
};
