#include "AI/BTService_UpdateTargetInfo.h"
#include "AIController.h"
#include "BehaviorTree/BehaviorTreeComponent.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/Pawn.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystem/MAGameplayTags.h"

UBTService_UpdateTargetInfo::UBTService_UpdateTargetInfo()
{
	NodeName = TEXT("Update Target Info");
	Interval = 0.5f;
	RandomDeviation = 0.f;

	TargetActorKey.AddObjectFilter(this, GET_MEMBER_NAME_CHECKED(UBTService_UpdateTargetInfo, TargetActorKey), AActor::StaticClass());
	InRangeKey.AddBoolFilter(this, GET_MEMBER_NAME_CHECKED(UBTService_UpdateTargetInfo, InRangeKey));
}

void UBTService_UpdateTargetInfo::TickNode(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds)
{
	Super::TickNode(OwnerComp, NodeMemory, DeltaSeconds);

	UBlackboardComponent* BB = OwnerComp.GetBlackboardComponent();
	AAIController* AIC = OwnerComp.GetAIOwner();
	if (!BB || !AIC) return;

	APawn* MyPawn = AIC->GetPawn();
	if (!MyPawn) return;

	APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(this, 0);

	// Treat dead pawns as no target — the pawn persists briefly as a ragdoll during the
	// death→respawn gap; without this check the AI chases a corpse.
	if (PlayerPawn)
	{
		if (UAbilitySystemComponent* ASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(PlayerPawn))
		{
			if (ASC->HasMatchingGameplayTag(MAGameplayTags::State_Dead))
			{
				PlayerPawn = nullptr;
			}
		}
	}

	BB->SetValueAsObject(TargetActorKey.SelectedKeyName, PlayerPawn);

	bool bInRange = false;
	if (PlayerPawn)
	{
		const float DistSq = FVector::DistSquared(MyPawn->GetActorLocation(), PlayerPawn->GetActorLocation());
		bInRange = DistSq <= AttackRange * AttackRange;
	}
	BB->SetValueAsBool(InRangeKey.SelectedKeyName, bInRange);
}
