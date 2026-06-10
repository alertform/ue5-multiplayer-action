#include "AI/BTTask_CircleTarget.h"
#include "AIController.h"
#include "BehaviorTree/BehaviorTreeComponent.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "GameFramework/Pawn.h"

UBTTask_CircleTarget::UBTTask_CircleTarget()
{
	NodeName = TEXT("Circle Target");
	bNotifyTick = true;

	TargetActorKey.AddObjectFilter(this, GET_MEMBER_NAME_CHECKED(UBTTask_CircleTarget, TargetActorKey), AActor::StaticClass());
}

uint16 UBTTask_CircleTarget::GetInstanceMemorySize() const
{
	return sizeof(FBTCircleTargetMemory);
}

EBTNodeResult::Type UBTTask_CircleTarget::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	FBTCircleTargetMemory* Mem = CastInstanceNodeMemory<FBTCircleTargetMemory>(NodeMemory);
	Mem->Elapsed = 0.f;
	return EBTNodeResult::InProgress;
}

void UBTTask_CircleTarget::TickTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds)
{
	FBTCircleTargetMemory* Mem = CastInstanceNodeMemory<FBTCircleTargetMemory>(NodeMemory);
	Mem->Elapsed += DeltaSeconds;

	AAIController* AIC = OwnerComp.GetAIOwner();
	APawn* Pawn = AIC ? AIC->GetPawn() : nullptr;
	UBlackboardComponent* BB = OwnerComp.GetBlackboardComponent();
	AActor* Target = BB ? Cast<AActor>(BB->GetValueAsObject(TargetActorKey.SelectedKeyName)) : nullptr;
	if (!Pawn || !Target)
	{
		FinishLatentTask(OwnerComp, EBTNodeResult::Failed);
		return;
	}
	if (Mem->Elapsed >= CircleSeconds)
	{
		FinishLatentTask(OwnerComp, EBTNodeResult::Succeeded);
		return;
	}

	FVector ToTarget = Target->GetActorLocation() - Pawn->GetActorLocation();
	ToTarget.Z = 0.f;
	const float Dist = ToTarget.Size();
	if (!ToTarget.Normalize())
	{
		return; // standing inside the target this frame — skip input, distance resolves next tick
	}

	// Stable per-pawn orbit direction; ~half the pawns circle each way.
	const float Sign = (Pawn->GetUniqueID() % 2 == 0) ? 1.f : -1.f;
	const FVector Tangent = FVector::CrossProduct(FVector::UpVector, ToTarget) * Sign;

	// Radial correction steers into the preferred band (positive = toward target).
	float Radial = 0.f;
	if (Dist > PreferredRadiusMax)
	{
		Radial = 1.f;
	}
	else if (Dist < PreferredRadiusMin)
	{
		Radial = -1.f;
	}

	const FVector MoveDir = (Tangent + ToTarget * Radial * 0.8f).GetSafeNormal();
	Pawn->AddMovementInput(MoveDir, SpeedScale);
}
