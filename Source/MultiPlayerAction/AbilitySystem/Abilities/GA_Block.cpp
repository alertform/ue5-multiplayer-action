#include "AbilitySystem/Abilities/GA_Block.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystem/MAAttributeSet.h"
#include "AbilitySystem/MAGameplayTags.h"

UGA_Block::UGA_Block()
{
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;

	FGameplayTagContainer Tags;
	Tags.AddTag(MAGameplayTags::Ability_Defense_Block);
	SetAssetTags(Tags);

	// Guard cannot start mid-action; release-to-swing handles the other direction
	// (melee lists State.Blocking in its blocked tags).
	ActivationBlockedTags.AddTag(MAGameplayTags::State_Dead);
	ActivationBlockedTags.AddTag(MAGameplayTags::State_Attacking);
	ActivationBlockedTags.AddTag(MAGameplayTags::State_Casting);
	ActivationBlockedTags.AddTag(MAGameplayTags::State_Dodging);

	ActivationOwnedTags.AddTag(MAGameplayTags::State_Blocking);
}

void UGA_Block::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	// Guard walk — same attribute pattern as GA_Sprint's boost; the Character's MoveSpeed
	// delegate pushes it into CharacterMovement on both ends.
	if (UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo())
	{
		OriginalWalkSpeed = ASC->GetNumericAttribute(UMAAttributeSet::GetMoveSpeedAttribute());
		ASC->SetNumericAttributeBase(UMAAttributeSet::GetMoveSpeedAttribute(), BlockWalkSpeed);
	}

	StartLoopMontage();

	// Blocked hits: the AttributeSet raises Event.Damage.Taken on every (mitigated) hit;
	// while we own State.Blocking the normal hit react is suppressed and we absorb instead.
	UAbilityTask_WaitGameplayEvent* HitTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(
		this, MAGameplayTags::Event_Damage_Taken);
	HitTask->EventReceived.AddDynamic(this, &UGA_Block::OnBlockedHit);
	HitTask->ReadyForActivation();
}

void UGA_Block::StartLoopMontage()
{
	if (!BlockLoopMontage)
	{
		return; // stance is cosmetic — the ability (and mitigation) works without it
	}
	// No delegates on purpose: the loop never "completes", and the Block_Hit overlay
	// interrupting it must not end the ability. Ability lifetime = the input hold.
	UAbilityTask_PlayMontageAndWait* LoopTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(
		this, NAME_None, BlockLoopMontage);
	LoopTask->ReadyForActivation();
}

void UGA_Block::OnBlockedHit(FGameplayEventData EventData)
{
	if (!BlockHitMontage)
	{
		return;
	}
	UAbilityTask_PlayMontageAndWait* HitMontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(
		this, NAME_None, BlockHitMontage);
	HitMontageTask->OnCompleted.AddDynamic(this, &UGA_Block::OnBlockHitFinished);
	HitMontageTask->OnInterrupted.AddDynamic(this, &UGA_Block::OnBlockHitFinished);
	HitMontageTask->ReadyForActivation();
}

void UGA_Block::OnBlockHitFinished()
{
	// Resume the guard stance if we're still holding.
	if (IsActive())
	{
		StartLoopMontage();
	}
}

void UGA_Block::EndAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility, bool bWasCancelled)
{
	if (UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo())
	{
		ASC->SetNumericAttributeBase(UMAAttributeSet::GetMoveSpeedAttribute(), OriginalWalkSpeed);
	}
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
