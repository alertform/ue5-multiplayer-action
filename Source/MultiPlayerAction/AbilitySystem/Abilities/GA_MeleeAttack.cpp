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
	// No attacking out of a rooted cast — the melee montage would interrupt the fireball montage,
	// wasting the already-committed fireball cost + cooldown before its projectile spawns.
	ActivationBlockedTags.AddTag(MAGameplayTags::State_Casting);
	// Owned for the swing's duration: gates the AnimInstance upper-body aim twist
	// (spine chain toward camera yaw) — see UMAAnimInstance::NativeUpdateAnimation.
	ActivationOwnedTags.AddTag(MAGameplayTags::State_Attacking);
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

	// No actor rotation here: the upper body visually turns toward the camera via the
	// AnimInstance spine twist (gated on our owned State.Attacking), the legs keep
	// following orient-to-movement, and PerformHitTrace aims with the camera yaw directly.

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

	// Trace along the camera yaw, not the body facing — the actor keeps orient-to-movement
	// during the (mobile) swing, so the body may point elsewhere while the player aims.
	// AI pawns: GetBaseAimRotation falls back to the actor rotation — same as before.
	const APawn* AvatarPawn = Cast<APawn>(Avatar);
	const FRotator AimYaw(0.f,
		AvatarPawn ? AvatarPawn->GetBaseAimRotation().Yaw : Avatar->GetActorRotation().Yaw, 0.f);
	const FVector AimDir = AimYaw.Vector();

	const FVector Start = Avatar->GetActorLocation() + AimDir * 50.f;
	const FVector End = Start + AimDir * TraceDistance;

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
