#include "AI/BTService_UpdateTargetInfo.h"
#include "AIController.h"
#include "BehaviorTree/BehaviorTreeComponent.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "NavigationSystem.h"
#include "NavigationPath.h"
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

	// Target = nearest LIVING player pawn, preferring REACHABLE ones.
	// - GetPlayerPawn(0) only ever tracked the host's pawn — clients were invisible in multiplayer.
	// - Dead pawns are skipped: the ragdoll persists through the death→respawn gap.
	// - Unreachable pawns (no nav path, e.g. standing where the navmesh is disconnected) lose to
	//   reachable ones regardless of distance — otherwise the AI locks onto a target it can never
	//   reach instead of switching. If NO ONE is reachable we still take the nearest living pawn,
	//   so close-range rotate/attack keeps working without a path.
	UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(MyPawn->GetWorld());
	const FVector MyLocation = MyPawn->GetActorLocation();

	APawn* PlayerPawn = nullptr;
	float BestDistSq = TNumericLimits<float>::Max();
	bool bBestReachable = false;
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

		// Synchronous path probe — cheap at this service's 0.5s interval with ≤ a handful of players.
		bool bReachable = false;
		if (NavSys)
		{
			const UNavigationPath* Path = NavSys->FindPathToLocationSynchronously(
				MyPawn->GetWorld(), MyLocation, Candidate->GetActorLocation(), MyPawn);
			bReachable = Path && Path->IsValid() && !Path->IsPartial();
		}

		const float DistSq = FVector::DistSquared(MyLocation, Candidate->GetActorLocation());
		// Reachability outranks distance; distance breaks ties within the same tier.
		const bool bBetter = (bReachable && !bBestReachable)
			|| (bReachable == bBestReachable && DistSq < BestDistSq);
		if (bBetter)
		{
			BestDistSq = DistSq;
			PlayerPawn = Candidate;
			bBestReachable = bReachable;
		}
	}

	BB->SetValueAsObject(TargetActorKey.SelectedKeyName, PlayerPawn);

	const bool bInRange = PlayerPawn && BestDistSq <= AttackRange * AttackRange;
	BB->SetValueAsBool(InRangeKey.SelectedKeyName, bInRange);
}
