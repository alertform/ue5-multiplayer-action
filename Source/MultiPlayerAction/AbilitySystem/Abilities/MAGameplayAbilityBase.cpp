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

void UMAGameplayAbilityBase::BeginRootedAction(const FGameplayAbilityActorInfo* ActorInfo) const
{
	// Demo feedback fixes: snap the character to the aim direction so the action animation,
	// the projectile, and the camera agree; and root the avatar for the action's duration.
	// Runs on both the predicting client and the server — rotation/movement-mode replicate.
	if (ACharacter* AvatarCharacter = Cast<ACharacter>(ActorInfo ? ActorInfo->AvatarActor.Get() : nullptr))
	{
		const FRotator AimYaw(0.f, AvatarCharacter->GetBaseAimRotation().Yaw, 0.f);
		AvatarCharacter->SetActorRotation(AimYaw);

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
