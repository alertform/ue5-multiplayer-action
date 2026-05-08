#include "AbilitySystem/Abilities/GA_Sprint.h"
#include "AbilitySystem/MAGameplayTags.h"
#include "AbilitySystem/MAAttributeSet.h"
#include "AbilitySystemComponent.h"
#include "TimerManager.h"
#include "Engine/World.h"

UGA_Sprint::UGA_Sprint()
{
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;

	FGameplayTagContainer AssetTagsValue;
	AssetTagsValue.AddTag(MAGameplayTags::Ability_Movement_Sprint);
	SetAssetTags(AssetTagsValue);

	ActivationBlockedTags.AddTag(MAGameplayTags::State_Dead);

	// Tag挂到 ASC 在整个 sprint 激活期间 — BP_GE_StaminaRegen 配 IgnoreTags 即停止 regen tick
	ActivationOwnedTags.AddTag(MAGameplayTags::Ability_Movement_Sprint);
}

void UGA_Sprint::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	// Boost via attribute — Character listens to MoveSpeed delegate and updates MaxWalkSpeed
	if (UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo())
	{
		OriginalWalkSpeed = ASC->GetNumericAttribute(UMAAttributeSet::GetMoveSpeedAttribute());
		ASC->SetNumericAttributeBase(UMAAttributeSet::GetMoveSpeedAttribute(), SprintSpeed);
	}

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(DrainTimer, this,
			&UGA_Sprint::TickStaminaDrain, DrainInterval, /*bLoop=*/true, /*FirstDelay=*/DrainInterval);
	}
}

void UGA_Sprint::TickStaminaDrain()
{
	UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();
	if (!ASC)
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
		return;
	}

	const float Stamina = ASC->GetNumericAttribute(UMAAttributeSet::GetStaminaAttribute());
	if (Stamina < StaminaDrainPerTick)
	{
		// Stamina exhausted — set to 0 and end
		ASC->SetNumericAttributeBase(UMAAttributeSet::GetStaminaAttribute(), 0.f);
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
		return;
	}

	ASC->SetNumericAttributeBase(UMAAttributeSet::GetStaminaAttribute(), Stamina - StaminaDrainPerTick);
}

void UGA_Sprint::EndAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility, bool bWasCancelled)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(DrainTimer);
	}

	if (UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo())
	{
		ASC->SetNumericAttributeBase(UMAAttributeSet::GetMoveSpeedAttribute(), OriginalWalkSpeed);
	}

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
