#pragma once

#include "CoreMinimal.h"
#include "MAGameplayAbilityBase.h"
#include "GA_Block.generated.h"

class UAnimMontage;

/**
 * Hold-to-block guard. While active the ASC owns State.Blocking, which:
 *   - gates damage mitigation in UMADamageExecutionCalculation,
 *   - suppresses the normal hit react (this ability plays Block_Hit instead),
 *   - blocks starting a melee attack (release to swing).
 *
 * Stance visuals: a LOOPING UpperBody montage (sections linked to themselves) holds the
 * guard pose for as long as the ability lives; blocked hits overlay a short Block_Hit
 * montage and then resume the loop. Movement slows via the MoveSpeed attribute
 * (same pattern as GA_Sprint's boost). Input release cancels by asset tag.
 */
UCLASS()
class MULTIPLAYERACTION_API UGA_Block : public UMAGameplayAbilityBase
{
	GENERATED_BODY()

public:
	UGA_Block();

	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;

	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		bool bReplicateEndAbility, bool bWasCancelled) override;

protected:
	/** Looping guard-stance montage (UpperBody slot, sections self-linked). */
	UPROPERTY(EditDefaultsOnly, Category = "Block")
	TObjectPtr<UAnimMontage> BlockLoopMontage;

	/** Short impact-absorb montage played per blocked hit, then the loop resumes. */
	UPROPERTY(EditDefaultsOnly, Category = "Block")
	TObjectPtr<UAnimMontage> BlockHitMontage;

	/** MoveSpeed attribute value while guarding (restored on end). */
	UPROPERTY(EditDefaultsOnly, Category = "Block", meta = (ClampMin = "0.0"))
	float BlockWalkSpeed = 200.f;

	/** Blocked hit landed (Event.Damage.Taken while this ability owns State.Blocking). */
	UFUNCTION()
	void OnBlockedHit(FGameplayEventData EventData);

	/** Block_Hit overlay finished — resume the guard stance loop. */
	UFUNCTION()
	void OnBlockHitFinished();

private:
	void StartLoopMontage();

	float OriginalWalkSpeed = 0.f;
};
