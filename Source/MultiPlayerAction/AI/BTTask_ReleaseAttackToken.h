#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTTaskNode.h"
#include "BTTask_ReleaseAttackToken.generated.h"

/**
 * Releases the pawn's attack token (always Succeeded — releasing nothing is fine).
 * Sits at the attack sequence's tail; abort paths that skip it are covered by the
 * director's TTL reaper (ma.AI.TokenTTL).
 */
UCLASS()
class MULTIPLAYERACTION_API UBTTask_ReleaseAttackToken : public UBTTaskNode
{
	GENERATED_BODY()

public:
	UBTTask_ReleaseAttackToken();

	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
};
