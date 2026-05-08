#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTService.h"
#include "BehaviorTree/BehaviorTreeTypes.h"
#include "BTService_UpdateTargetInfo.generated.h"

/**
 * BT service that periodically updates blackboard with the closest player + range check.
 * Runs at the BT root or branch level. Configures two BB keys:
 *   TargetActorKey (Object) — set to player pawn (or cleared if none)
 *   InRangeKey (Bool)       — true if player is within AttackRange
 */
UCLASS()
class MULTIPLAYERACTION_API UBTService_UpdateTargetInfo : public UBTService
{
	GENERATED_BODY()

public:
	UBTService_UpdateTargetInfo();

	virtual void TickNode(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds) override;

protected:
	UPROPERTY(EditAnywhere, Category = "AI")
	FBlackboardKeySelector TargetActorKey;

	UPROPERTY(EditAnywhere, Category = "AI")
	FBlackboardKeySelector InRangeKey;

	UPROPERTY(EditAnywhere, Category = "AI")
	float AttackRange = 250.f;
};
