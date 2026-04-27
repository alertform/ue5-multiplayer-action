#include "AbilitySystem/Abilities/MAGameplayAbilityBase.h"
#include "MultiPlayerActionCharacter.h"

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
