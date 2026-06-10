#include "AI/BTTask_ReleaseAttackToken.h"
#include "AI/Combat/MACombatDirectorSubsystem.h"
#include "AIController.h"
#include "BehaviorTree/BehaviorTreeComponent.h"
#include "GameFramework/Pawn.h"

UBTTask_ReleaseAttackToken::UBTTask_ReleaseAttackToken()
{
	NodeName = TEXT("Release Attack Token");
}

EBTNodeResult::Type UBTTask_ReleaseAttackToken::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	AAIController* AIC = OwnerComp.GetAIOwner();
	APawn* Pawn = AIC ? AIC->GetPawn() : nullptr;
	UWorld* W = Pawn ? Pawn->GetWorld() : nullptr;
	if (UMACombatDirectorSubsystem* Director = W ? W->GetSubsystem<UMACombatDirectorSubsystem>() : nullptr)
	{
		Director->ReleaseToken(Pawn);
	}
	return EBTNodeResult::Succeeded;
}
