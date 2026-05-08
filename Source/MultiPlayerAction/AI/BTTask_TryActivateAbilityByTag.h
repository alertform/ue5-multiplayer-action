#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTTaskNode.h"
#include "GameplayTagContainer.h"
#include "BTTask_TryActivateAbilityByTag.generated.h"

/**
 * BehaviorTree task that calls ASC->TryActivateAbilitiesByTag(AbilityTag) on the AI's
 * own pawn. The headline GAS+BT integration: AI driven via BT graph activates the
 * SAME UGameplayAbility class the player uses through input, no duplicate logic.
 *
 * Returns Succeeded if any matching ability activated, Failed otherwise (cooldown,
 * blocked tag, no matching GA granted, etc.).
 */
UCLASS()
class MULTIPLAYERACTION_API UBTTask_TryActivateAbilityByTag : public UBTTaskNode
{
	GENERATED_BODY()

public:
	UBTTask_TryActivateAbilityByTag();

	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;

protected:
	UPROPERTY(EditAnywhere, Category = "AI")
	FGameplayTag AbilityTag;
};
