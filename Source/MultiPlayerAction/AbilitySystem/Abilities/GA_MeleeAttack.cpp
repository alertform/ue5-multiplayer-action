#include "AbilitySystem/Abilities/GA_MeleeAttack.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystem/MAGameplayTags.h"
#include "MultiPlayerActionCharacter.h"

UGA_MeleeAttack::UGA_MeleeAttack()
{
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;

	// Identify this ability by tag so TryActivateAbilitiesByTag can find it; BP children inherit this.
	// UE 5.5+ deprecates direct AbilityTags mutation — use SetAssetTags in constructor only.
	FGameplayTagContainer Tags;
	Tags.AddTag(MAGameplayTags::Ability_Melee_Attack);
	SetAssetTags(Tags);

	// Cannot attack while dead — checked at TryActivate time, no need for runtime guard
	ActivationBlockedTags.AddTag(MAGameplayTags::State_Dead);
}

void UGA_MeleeAttack::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	if (!AttackMontage)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	// Play montage at configurable rate (default 2.0x — see MontagePlayRate UPROPERTY)
	UAbilityTask_PlayMontageAndWait* MontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(
		this, NAME_None, AttackMontage, MontagePlayRate);

	// OnBlendOut + OnCompleted both fire on natural end — bind only OnCompleted to avoid double EndAbility
	MontageTask->OnCompleted.AddDynamic(this, &UGA_MeleeAttack::OnMontageEnded);
	MontageTask->OnInterrupted.AddDynamic(this, &UGA_MeleeAttack::OnMontageEnded);
	MontageTask->OnCancelled.AddDynamic(this, &UGA_MeleeAttack::OnMontageEnded);
	MontageTask->ReadyForActivation();

	// Wait for Event.Montage.Hit gameplay event (sent from AnimNotify in montage)
	UAbilityTask_WaitGameplayEvent* EventTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(
		this, MAGameplayTags::Event_Montage_Hit);

	EventTask->EventReceived.AddDynamic(this, &UGA_MeleeAttack::OnMontageEvent);
	EventTask->ReadyForActivation();
}

void UGA_MeleeAttack::OnMontageEvent(FGameplayEventData EventData)
{
	PerformHitTrace(GetCurrentActorInfo());
}

void UGA_MeleeAttack::OnMontageEnded()
{
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void UGA_MeleeAttack::PerformHitTrace(const FGameplayAbilityActorInfo* ActorInfo)
{
	// Use AvatarActor (any AActor) instead of casting to AMultiPlayerActionCharacter,
	// so AI-controlled pawns (TargetDummy) can use the same GA class as players.
	AActor* Avatar = ActorInfo ? ActorInfo->AvatarActor.Get() : nullptr;
	if (!Avatar)
	{
		return;
	}

	// Only do authoritative hit detection on server (or standalone)
	if (!Avatar->HasAuthority())
	{
		return;
	}

	const FVector Start = Avatar->GetActorLocation() + Avatar->GetActorForwardVector() * 50.f;
	const FVector End = Start + Avatar->GetActorForwardVector() * TraceDistance;

	FCollisionQueryParams QueryParams;
	QueryParams.AddIgnoredActor(Avatar);

	TArray<FHitResult> HitResults;
	const bool bHit = Avatar->GetWorld()->SweepMultiByChannel(
		HitResults, Start, End, FQuat::Identity,
		ECC_Pawn, FCollisionShape::MakeSphere(TraceRadius), QueryParams);

	if (!bHit || !DamageEffect)
	{
		return;
	}

	for (const FHitResult& Hit : HitResults)
	{
		AActor* HitActor = Hit.GetActor();
		if (!HitActor || HitActor == Avatar)
		{
			continue;
		}

		// Apply damage GE through source ASC so prediction key + instigator/context route correctly
		UAbilitySystemComponent* TargetASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(HitActor);
		UAbilitySystemComponent* SourceASC = GetAbilitySystemComponentFromActorInfo();
		if (!TargetASC || !SourceASC)
		{
			continue;
		}

		FGameplayEffectSpecHandle SpecHandle = MakeOutgoingGameplayEffectSpec(DamageEffect, GetAbilityLevel());
		if (SpecHandle.IsValid())
		{
			SourceASC->ApplyGameplayEffectSpecToTarget(*SpecHandle.Data.Get(), TargetASC);
		}

		// Fire hit-impact cue with hit location + normal so FX spawn at exact contact point.
		// Static cue notify (BP_GCN_MeleeHit) routes by tag and replicates to all relevant clients.
		FGameplayCueParameters CueParams;
		CueParams.Location = Hit.ImpactPoint;
		CueParams.Normal = Hit.ImpactNormal;
		CueParams.PhysicalMaterial = Hit.PhysMaterial;
		CueParams.Instigator = Avatar;
		CueParams.EffectCauser = Avatar;
		SourceASC->ExecuteGameplayCue(MAGameplayTags::GameplayCue_Melee_Hit, CueParams);
	}
}
