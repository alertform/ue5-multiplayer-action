#include "AI/MAEnemyController.h"
#include "AI/Combat/MAAIDefenseComponent.h"
#include "BehaviorTree/BehaviorTree.h"
#include "BehaviorTree/BehaviorTreeComponent.h"

AMAEnemyController::AMAEnemyController()
{
	DefenseComponent = CreateDefaultSubobject<UMAAIDefenseComponent>(TEXT("DefenseComponent"));
}

void AMAEnemyController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);

	if (BehaviorTreeAsset)
	{
		RunBehaviorTree(BehaviorTreeAsset);
	}

	// After RunBehaviorTree so the blackboard exists — deterministic, no BeginPlay race.
	if (DefenseComponent)
	{
		DefenseComponent->BindToBlackboard();
	}
}

void AMAEnemyController::OnUnPossess()
{
	if (DefenseComponent)
	{
		DefenseComponent->UnbindAll();
	}
	if (UBehaviorTreeComponent* BTComp = Cast<UBehaviorTreeComponent>(BrainComponent))
	{
		BTComp->StopTree(EBTStopMode::Safe);
	}
	Super::OnUnPossess();
}
