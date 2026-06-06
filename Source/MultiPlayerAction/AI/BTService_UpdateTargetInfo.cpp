#include "AI/BTService_UpdateTargetInfo.h"
#include "AIController.h"
#include "BehaviorTree/BehaviorTreeComponent.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
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

	// Target = NEAREST LIVING player pawn. GetPlayerPawn(0) only ever tracked the host's
	// pawn (local player 0 on the server) — clients were invisible to the AI in multiplayer.
	// Dead pawns are skipped: the ragdoll persists through the death→respawn gap and the AI
	// would otherwise chase a corpse.
	APawn* PlayerPawn = nullptr;
	float BestDistSq = TNumericLimits<float>::Max();
	for (FConstPlayerControllerIterator It = MyPawn->GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		const APlayerController* PC = It->Get();
		APawn* Candidate = PC ? PC->GetPawn() : nullptr;
		if (!Candidate)
		{
			continue;
		}

		if (const UAbilitySystemComponent* ASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Candidate))
		{
			if (ASC->HasMatchingGameplayTag(MAGameplayTags::State_Dead))
			{
				continue;
			}
		}

		const float DistSq = FVector::DistSquared(MyPawn->GetActorLocation(), Candidate->GetActorLocation());
		if (DistSq < BestDistSq)
		{
			BestDistSq = DistSq;
			PlayerPawn = Candidate;
		}
	}

	BB->SetValueAsObject(TargetActorKey.SelectedKeyName, PlayerPawn);

	const bool bInRange = PlayerPawn && BestDistSq <= AttackRange * AttackRange;
	BB->SetValueAsBool(InRangeKey.SelectedKeyName, bInRange);
}
