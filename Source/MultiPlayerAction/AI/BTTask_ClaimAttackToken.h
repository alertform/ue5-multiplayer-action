#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTTaskNode.h"
#include "BTTask_ClaimAttackToken.generated.h"

/**
 * Claims an attack token from UMACombatDirectorSubsystem for the AI's pawn.
 * Granted -> Succeeded (the attack sequence proceeds); denied -> Failed (the
 * selector falls through to the circle branch). Re-claim by a holder refreshes its TTL.
 */
UCLASS()
class MULTIPLAYERACTION_API UBTTask_ClaimAttackToken : public UBTTaskNode
{
	GENERATED_BODY()

public:
	UBTTask_ClaimAttackToken();

	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
};
