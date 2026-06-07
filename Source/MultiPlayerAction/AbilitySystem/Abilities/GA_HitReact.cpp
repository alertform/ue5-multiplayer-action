#include "AbilitySystem/Abilities/GA_HitReact.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "AbilitySystem/MAGameplayTags.h"

UGA_HitReact::UGA_HitReact()
{
	// Server decides flinches (the damage event only exists server-side); activation and the
	// chosen montage replicate to the owning client and simulated proxies automatically.
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerInitiated;

	FGameplayTagContainer Tags;
	Tags.AddTag(MAGameplayTags::Ability_Reaction_HitReact);
	SetAssetTags(Tags);

	// Getting hit INTERRUPTS your attack (standard action-game stagger): the react montage
	// shares the UpperBody slot, so it stomps the combo montage -> the melee task's
	// OnInterrupted ends the ability and the chain dies. Casting stays protected for now —
	// interrupting a committed fireball would eat its cost+cooldown without a refund path.
	ActivationBlockedTags.AddTag(MAGameplayTags::State_Dead);
	ActivationBlockedTags.AddTag(MAGameplayTags::State_Casting);
	ActivationBlockedTags.AddTag(MAGameplayTags::State_Dodging);

	// Auto-trigger from the AttributeSet's damage event.
	FAbilityTriggerData Trigger;
	Trigger.TriggerTag = MAGameplayTags::Event_Damage_Taken;
	Trigger.TriggerSource = EGameplayAbilityTriggerSource::GameplayEvent;
	AbilityTriggers.Add(Trigger);
}

void UGA_HitReact::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	if (!CommitAbility(Handle, ActorInfo, ActivationInfo) || HitReactMontages.Num() == 0)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	// Server-side random pick — the chosen montage asset replicates with the play call.
	UAnimMontage* Montage = HitReactMontages[FMath::RandRange(0, HitReactMontages.Num() - 1)];
	if (!Montage)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	UAbilityTask_PlayMontageAndWait* MontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(
		this, NAME_None, Montage, PlayRate);
	MontageTask->OnCompleted.AddDynamic(this, &UGA_HitReact::OnMontageFinished);
	MontageTask->OnInterrupted.AddDynamic(this, &UGA_HitReact::OnMontageFinished);
	MontageTask->OnCancelled.AddDynamic(this, &UGA_HitReact::OnMontageFinished);
	MontageTask->ReadyForActivation();
}

void UGA_HitReact::OnMontageFinished()
{
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}
