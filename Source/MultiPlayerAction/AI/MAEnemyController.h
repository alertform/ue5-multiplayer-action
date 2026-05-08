#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "MAEnemyController.generated.h"

class UBehaviorTree;

/**
 * AI controller for melee enemies. On Possess, runs the configured BehaviorTree.
 *
 * The BT graph itself drives the per-tick logic via custom nodes (UBTService_UpdateTargetInfo
 * + UBTTask_TryActivateAbilityByTag) — designer can edit composition in BT editor without
 * recompiling C++. Headline GAS+BT integration: BTTask_TryActivateAbilityByTag drives the
 * SAME UGameplayAbility class the player uses through input.
 */
UCLASS()
class MULTIPLAYERACTION_API AMAEnemyController : public AAIController
{
	GENERATED_BODY()

public:
	virtual void OnPossess(APawn* InPawn) override;
	virtual void OnUnPossess() override;

protected:
	/** BehaviorTree to run on possess. Set in BP_EnemyController defaults to BT_Enemy. */
	UPROPERTY(EditDefaultsOnly, Category = "AI")
	TObjectPtr<UBehaviorTree> BehaviorTreeAsset;
};
