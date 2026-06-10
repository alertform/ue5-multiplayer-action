#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTTaskNode.h"
#include "BehaviorTree/BehaviorTreeTypes.h"
#include "BTTask_CircleTarget.generated.h"

/**
 * Token-less positioning: strafes tangentially around the blackboard target for
 * CircleSeconds, holding a preferred radius band, then Succeeds so the selector
 * re-evaluates (= the pawn re-bids for the attack token). Orbit direction is stable
 * per pawn (seeded from GetUniqueID) so two circlers don't orbit in lockstep.
 * Drives AddMovementInput directly — flat arena navmesh, no pathfinding needed.
 */
UCLASS()
class MULTIPLAYERACTION_API UBTTask_CircleTarget : public UBTTaskNode
{
	GENERATED_BODY()

public:
	UBTTask_CircleTarget();

	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
	virtual void TickTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds) override;
	virtual uint16 GetInstanceMemorySize() const override;

protected:
	UPROPERTY(EditAnywhere, Category = "AI")
	FBlackboardKeySelector TargetActorKey;

	/** How long to circle before succeeding (and re-bidding for the token). */
	UPROPERTY(EditAnywhere, Category = "AI", meta = (ClampMin = "0.1"))
	float CircleSeconds = 1.5f;

	UPROPERTY(EditAnywhere, Category = "AI")
	float PreferredRadiusMin = 200.f;

	UPROPERTY(EditAnywhere, Category = "AI")
	float PreferredRadiusMax = 300.f;

	/** Movement input scale while circling (1.0 = full MoveSpeed). */
	UPROPERTY(EditAnywhere, Category = "AI", meta = (ClampMin = "0.1", ClampMax = "1.0"))
	float SpeedScale = 0.6f;
};

struct FBTCircleTargetMemory
{
	float Elapsed = 0.f;
};
