#include "AbilitySystem/Abilities/GA_MeleeAttack.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "AbilitySystemComponent.h"
#include "MultiPlayerActionCharacter.h"

UGA_MeleeAttack::UGA_MeleeAttack()
{
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
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

	// Play montage
	UAbilityTask_PlayMontageAndWait* MontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(
		this, NAME_None, AttackMontage, 1.0f);

	MontageTask->OnCompleted.AddDynamic(this, &UGA_MeleeAttack::OnMontageEnded);
	MontageTask->OnBlendOut.AddDynamic(this, &UGA_MeleeAttack::OnMontageEnded);
	MontageTask->OnInterrupted.AddDynamic(this, &UGA_MeleeAttack::OnMontageEnded);
	MontageTask->OnCancelled.AddDynamic(this, &UGA_MeleeAttack::OnMontageEnded);
	MontageTask->ReadyForActivation();

	// Wait for "Event.Attack" gameplay event (sent from AnimNotify in montage)
	FGameplayTag EventTag = FGameplayTag::RequestGameplayTag(FName("Event.Attack"));

	UAbilityTask_WaitGameplayEvent* EventTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(
		this, EventTag);

	EventTask->EventReceived.AddDynamic(this, &UGA_MeleeAttack::OnMontageEvent);
	EventTask->ReadyForActivation();
}

void UGA_MeleeAttack::OnMontageEvent(FGameplayTag EventTag, FGameplayEventData EventData)
{
	PerformHitTrace(GetCurrentActorInfo());
}

void UGA_MeleeAttack::OnMontageEnded(FGameplayTag EventTag, FGameplayEventData EventData)
{
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void UGA_MeleeAttack::PerformHitTrace(const FGameplayAbilityActorInfo* ActorInfo)
{
	AMultiPlayerActionCharacter* Character = GetMACharacter(ActorInfo);
	if (!Character)
	{
		return;
	}

	// Only do authoritative hit detection on server (or standalone)
	if (!Character->HasAuthority())
	{
		return;
	}

	const FVector Start = Character->GetActorLocation() + Character->GetActorForwardVector() * 50.f;
	const FVector End = Start + Character->GetActorForwardVector() * TraceDistance;

	FCollisionQueryParams QueryParams;
	QueryParams.AddIgnoredActor(Character);

	TArray<FHitResult> HitResults;
	const bool bHit = Character->GetWorld()->SweepMultiByChannel(
		HitResults, Start, End, FQuat::Identity,
		ECC_Pawn, FCollisionShape::MakeSphere(TraceRadius), QueryParams);

	if (!bHit || !DamageEffect)
	{
		return;
	}

	for (const FHitResult& Hit : HitResults)
	{
		AActor* HitActor = Hit.GetActor();
		if (!HitActor || HitActor == Character)
		{
			continue;
		}

		// Apply damage GE to the target's ASC
		if (UAbilitySystemComponent* TargetASC = UAbilitySystemComponent::GetAbilitySystemComponentFromActor(HitActor))
		{
			FGameplayEffectSpecHandle SpecHandle = MakeOutgoingGameplayEffectSpec(DamageEffect, GetAbilityLevel());
			ApplyGameplayEffectSpecToTarget(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo,
				SpecHandle, TargetASC);
		}
	}
}
