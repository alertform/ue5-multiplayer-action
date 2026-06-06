#include "AbilitySystem/Abilities/MAGameplayAbilityBase.h"
#include "MultiPlayerActionCharacter.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"

UMAGameplayAbilityBase::UMAGameplayAbilityBase()
{
	// Default: instanced per actor so we can store state
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;

	// Default: LocalPredicted for responsive feel
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
}

AMultiPlayerActionCharacter* UMAGameplayAbilityBase::GetMACharacter(const FGameplayAbilityActorInfo* ActorInfo) const
{
	return ActorInfo ? Cast<AMultiPlayerActionCharacter>(ActorInfo->AvatarActor.Get()) : nullptr;
}

void UMAGameplayAbilityBase::SnapToAimYaw(const FGameplayAbilityActorInfo* ActorInfo) const
{
	// Snap the character to the camera's yaw so the action animation fires where the player
	// is looking (orient-to-movement would otherwise leave the body facing the last move
	// direction). Runs on both the predicting client and the server — rotation replicates.
	if (ACharacter* AvatarCharacter = Cast<ACharacter>(ActorInfo ? ActorInfo->AvatarActor.Get() : nullptr))
	{
		const FRotator AimYaw(0.f, AvatarCharacter->GetBaseAimRotation().Yaw, 0.f);
		AvatarCharacter->SetActorRotation(AimYaw);
	}
}

void UMAGameplayAbilityBase::BeginAimFacing(const FGameplayAbilityActorInfo* ActorInfo) const
{
	// Snap to the camera yaw, then stop the CMC from immediately rotating the body back
	// toward the acceleration direction (bOrientRotationToMovement wins one tick after a
	// bare SetActorRotation). Runs on predicting client + server; rotation replicates.
	SnapToAimYaw(ActorInfo);

	if (ACharacter* AvatarCharacter = Cast<ACharacter>(ActorInfo ? ActorInfo->AvatarActor.Get() : nullptr))
	{
		if (UCharacterMovementComponent* MoveComp = AvatarCharacter->GetCharacterMovement())
		{
			MoveComp->bOrientRotationToMovement = false;
		}
	}
}

void UMAGameplayAbilityBase::EndAimFacing(const FGameplayAbilityActorInfo* ActorInfo) const
{
	// Restore the character default (orient-to-movement is the template's locomotion mode).
	// Idempotent — safe on end paths where BeginAimFacing never ran.
	if (ActorInfo)
	{
		if (ACharacter* AvatarCharacter = Cast<ACharacter>(ActorInfo->AvatarActor.Get()))
		{
			if (UCharacterMovementComponent* MoveComp = AvatarCharacter->GetCharacterMovement())
			{
				MoveComp->bOrientRotationToMovement = true;
			}
		}
	}
}

void UMAGameplayAbilityBase::BeginRootedAction(const FGameplayAbilityActorInfo* ActorInfo) const
{
	// Aim-snap + root the avatar for the action's duration (movement-mode replicates).
	// Currently unused — both melee and fireball moved to upper-body layering + SnapToAimYaw;
	// kept for future committed/channelled actions.
	SnapToAimYaw(ActorInfo);

	if (ACharacter* AvatarCharacter = Cast<ACharacter>(ActorInfo ? ActorInfo->AvatarActor.Get() : nullptr))
	{
		if (UCharacterMovementComponent* MoveComp = AvatarCharacter->GetCharacterMovement())
		{
			MoveComp->DisableMovement();
		}
	}
}

void UMAGameplayAbilityBase::EndRootedAction(const FGameplayAbilityActorInfo* ActorInfo) const
{
	// Restore movement locked in BeginRootedAction — but only if nothing else changed the mode
	// (death ragdoll etc. must not be stomped back to walking).
	if (ActorInfo)
	{
		if (ACharacter* AvatarCharacter = Cast<ACharacter>(ActorInfo->AvatarActor.Get()))
		{
			if (UCharacterMovementComponent* MoveComp = AvatarCharacter->GetCharacterMovement())
			{
				if (MoveComp->MovementMode == MOVE_None)
				{
					MoveComp->SetMovementMode(MOVE_Walking);
				}
			}
		}
	}
}
