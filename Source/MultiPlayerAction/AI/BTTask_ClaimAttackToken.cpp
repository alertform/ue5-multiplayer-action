#include "AI/BTTask_ClaimAttackToken.h"
#include "AI/Combat/MACombatDirectorSubsystem.h"
#include "AIController.h"
#include "BehaviorTree/BehaviorTreeComponent.h"
#include "GameFramework/Pawn.h"

UBTTask_ClaimAttackToken::UBTTask_ClaimAttackToken()
{
	NodeName = TEXT("Claim Attack Token");
}

EBTNodeResult::Type UBTTask_ClaimAttackToken::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	AAIController* AIC = OwnerComp.GetAIOwner();
	APawn* Pawn = AIC ? AIC->GetPawn() : nullptr;
	UWorld* W = Pawn ? Pawn->GetWorld() : nullptr;
	UMACombatDirectorSubsystem* Director = W ? W->GetSubsystem<UMACombatDirectorSubsystem>() : nullptr;
	if (!Director)
	{
		return EBTNodeResult::Failed;
	}
	return Director->ClaimToken(Pawn) ? EBTNodeResult::Succeeded : EBTNodeResult::Failed;
}
