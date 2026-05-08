#include "AI/BTTask_TryActivateAbilityByTag.h"
#include "AIController.h"
#include "BehaviorTree/BehaviorTreeComponent.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "GameFramework/Pawn.h"

UBTTask_TryActivateAbilityByTag::UBTTask_TryActivateAbilityByTag()
{
	NodeName = TEXT("Try Activate Ability By Tag");
}

EBTNodeResult::Type UBTTask_TryActivateAbilityByTag::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	AAIController* AIC = OwnerComp.GetAIOwner();
	if (!AIC) return EBTNodeResult::Failed;

	APawn* Pawn = AIC->GetPawn();
	if (!Pawn) return EBTNodeResult::Failed;

	UAbilitySystemComponent* ASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Pawn);
	if (!ASC) return EBTNodeResult::Failed;

	if (!AbilityTag.IsValid()) return EBTNodeResult::Failed;

	FGameplayTagContainer Tags;
	Tags.AddTag(AbilityTag);
	const bool bActivated = ASC->TryActivateAbilitiesByTag(Tags);

	return bActivated ? EBTNodeResult::Succeeded : EBTNodeResult::Failed;
}
