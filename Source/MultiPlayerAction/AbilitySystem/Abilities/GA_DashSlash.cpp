#include "AbilitySystem/Abilities/GA_DashSlash.h"

#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystem/MAGameplayTags.h"
#include "Combat/MAAttackLunge.h"
#include "Combat/MAMeleeHitOps.h"
#include "Combat/MAWarpOps.h"

static TAutoConsoleVariable<int32> CVarDashSlash(
	TEXT("ma.DashSlash"), 1,
	TEXT("1 = the Motion-Warped dash slash ability can activate. 0 = disabled (demo A/B)."));

const FName UGA_DashSlash::WarpTargetName(TEXT("AttackTarget"));

UGA_DashSlash::UGA_DashSlash()
{
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;

	FGameplayTagContainer Tags;
	Tags.AddTag(MAGameplayTags::Ability_Melee_DashSlash);
	SetAssetTags(Tags);

	ActivationBlockedTags.AddTag(MAGameplayTags::State_Dead);
	ActivationBlockedTags.AddTag(MAGameplayTags::State_Casting);
	ActivationBlockedTags.AddTag(MAGameplayTags::State_Blocking);
	// No dashing out of the melee combo (and vice versa — GA_MeleeAttack blocks on its own swing
	// via the montage; the dash's State.Attacking blocks new combo activations symmetrically).
	ActivationBlockedTags.AddTag(MAGameplayTags::State_Attacking);
	ActivationOwnedTags.AddTag(MAGameplayTags::State_Attacking); // AI guards react to the dash

	CancelAbilitiesWithTag.AddTag(MAGameplayTags::Ability_Movement_Sprint);
}

void UGA_DashSlash::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	if (CVarDashSlash.GetValueOnGameThread() == 0
		|| !DashMontage
		|| !CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	SetupWarpTarget();

	UAbilityTask_PlayMontageAndWait* MontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(
		this, NAME_None, DashMontage, 1.0f);
	MontageTask->OnCompleted.AddDynamic(this, &UGA_DashSlash::OnMontageFinished);
	MontageTask->OnInterrupted.AddDynamic(this, &UGA_DashSlash::OnMontageFinished);
	MontageTask->OnCancelled.AddDynamic(this, &UGA_DashSlash::OnMontageFinished);
	MontageTask->ReadyForActivation();

	UAbilityTask_WaitGameplayEvent* EventTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(
		this, MAGameplayTags::Event_Montage_Hit);
	EventTask->EventReceived.AddDynamic(this, &UGA_DashSlash::OnHitEvent);
	EventTask->ReadyForActivation();
}

void UGA_DashSlash::SetupWarpTarget()
{
	FMALungeParams Params;
	Params.ConeHalfAngleDeg = ConeHalfAngleDeg;
	Params.MaxLungeDistanceCm = MaxLungeDistanceCm;
	Params.StopDistanceCm = StopDistanceCm;
	MAWarpOps::SetupWarpTargetForAbility(this, WarpTargetName, Params, NoTargetDashCm);
}

void UGA_DashSlash::ClearWarpTarget()
{
	MAWarpOps::ClearWarpTarget(this, WarpTargetName);
}

void UGA_DashSlash::OnHitEvent(FGameplayEventData EventData)
{
	AActor* Avatar = GetAvatarActorFromActorInfo();
	if (!Avatar || !Avatar->HasAuthority() || !DamageEffect)
	{
		return;
	}
	UAbilitySystemComponent* SourceASC = GetAbilitySystemComponentFromActorInfo();
	if (!SourceASC)
	{
		return;
	}
	const FGameplayEffectSpecHandle SpecHandle = MakeOutgoingGameplayEffectSpec(DamageEffect, GetAbilityLevel());
	MAMeleeHitOps::SweepAndApplyMeleeHit(Avatar, SourceASC, SpecHandle, TraceDistance, TraceRadius);
}

void UGA_DashSlash::OnMontageFinished()
{
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void UGA_DashSlash::EndAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility, bool bWasCancelled)
{
	ClearWarpTarget();
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
