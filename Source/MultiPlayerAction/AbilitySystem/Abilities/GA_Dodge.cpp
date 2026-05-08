#include "AbilitySystem/Abilities/GA_Dodge.h"
#include "AbilitySystem/MAGameplayTags.h"
#include "GameFramework/Character.h"
#include "TimerManager.h"
#include "Engine/World.h"

UGA_Dodge::UGA_Dodge()
{
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;

	FGameplayTagContainer AssetTagsValue;
	AssetTagsValue.AddTag(MAGameplayTags::Ability_Movement_Dodge);
	SetAssetTags(AssetTagsValue);

	ActivationBlockedTags.AddTag(MAGameplayTags::State_Dead);

	// During dodge, ASC has State.Dodging — BP_GE_Damage's ApplicationTagRequirements.IgnoreTags
	// must include State.Dodging for the i-frame to actually block damage.
	ActivationOwnedTags.AddTag(MAGameplayTags::State_Dodging);

	// Dodge interrupts active sprint
	CancelAbilitiesWithTag.AddTag(MAGameplayTags::Ability_Movement_Sprint);
}

void UGA_Dodge::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	// Launch in the direction of last input (so dodge follows WASD), fall back to forward
	if (ACharacter* Char = Cast<ACharacter>(ActorInfo ? ActorInfo->AvatarActor.Get() : nullptr))
	{
		FVector Dir = Char->GetLastMovementInputVector();
		if (Dir.IsNearlyZero())
		{
			Dir = Char->GetActorForwardVector();
		}
		Char->LaunchCharacter(Dir.GetSafeNormal() * DodgeImpulse, /*bXYOverride=*/true, /*bZOverride=*/false);
	}

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(EndDodgeTimer, this, &UGA_Dodge::EndDodge, DodgeDuration, false);
	}
}

void UGA_Dodge::EndDodge()
{
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void UGA_Dodge::EndAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility, bool bWasCancelled)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(EndDodgeTimer);
	}
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
