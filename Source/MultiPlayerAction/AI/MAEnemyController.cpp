#include "AI/MAEnemyController.h"
#include "AbilitySystem/MAGameplayTags.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"

void AMAEnemyController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);

	GetWorldTimerManager().SetTimer(
		EvalTimer, this, &AMAEnemyController::EvaluateAndAttack,
		EvalInterval, /*bLoop=*/true);
}

void AMAEnemyController::OnUnPossess()
{
	GetWorldTimerManager().ClearTimer(EvalTimer);
	Super::OnUnPossess();
}

void AMAEnemyController::EvaluateAndAttack()
{
	APawn* MyPawn = GetPawn();
	if (!MyPawn)
	{
		return;
	}

	UAbilitySystemComponent* MyASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(MyPawn);
	if (!MyASC)
	{
		return;
	}

	// Skip if dead — State.Dead tag is added by AS::CheckDeath when Health hits 0
	if (MyASC->HasMatchingGameplayTag(MAGameplayTags::State_Dead))
	{
		return;
	}

	// Find player 0 (single-player or first listen-server player). Iterate for proper PvE.
	APawn* TargetPawn = UGameplayStatics::GetPlayerPawn(this, 0);
	if (!TargetPawn)
	{
		return;
	}

	const FVector ToTarget = TargetPawn->GetActorLocation() - MyPawn->GetActorLocation();
	if (ToTarget.SizeSquared() > AttackRange * AttackRange)
	{
		return;
	}

	// Snap-face the target on yaw axis only (no pitch, dummy doesn't tilt)
	FRotator FaceRot = ToTarget.Rotation();
	FaceRot.Pitch = 0.f;
	FaceRot.Roll = 0.f;
	MyPawn->SetActorRotation(FaceRot);

	// ★ Same GA class the player uses. TryActivateAbilitiesByTag matches by AssetTags.
	FGameplayTagContainer ActivationTags;
	ActivationTags.AddTag(MAGameplayTags::Ability_Melee_Attack);
	MyASC->TryActivateAbilitiesByTag(ActivationTags);
}
