#include "AbilitySystem/Abilities/GA_DashSlash.h"

#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystem/MACombatantInterface.h"
#include "AbilitySystem/MAGameplayTags.h"
#include "Combat/MAAttackLunge.h"
#include "Combat/MAMeleeHitOps.h"
#include "EngineUtils.h"
#include "GameFramework/Character.h"
#include "MotionWarpingComponent.h"

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
	ACharacter* Avatar = Cast<ACharacter>(GetAvatarActorFromActorInfo());
	UMotionWarpingComponent* Warp = Avatar ? Avatar->FindComponentByClass<UMotionWarpingComponent>() : nullptr;
	if (!Avatar || !Warp)
	{
		return;
	}

	// Candidates: living combatants, not self; AI attackers never snap onto fellow AI
	// (same faction rule as the damage path).
	const APawn* AvatarPawn = Cast<APawn>(Avatar);
	const bool bAttackerIsAI = AvatarPawn && !AvatarPawn->IsPlayerControlled();
	TArray<FVector> Locations;
	for (TActorIterator<APawn> It(Avatar->GetWorld()); It; ++It)
	{
		APawn* Candidate = *It;
		if (Candidate == Avatar || !Candidate->Implements<UMACombatantInterface>())
		{
			continue;
		}
		if (bAttackerIsAI && !Candidate->IsPlayerControlled())
		{
			continue;
		}
		if (const UAbilitySystemComponent* ASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Candidate))
		{
			if (ASC->HasMatchingGameplayTag(MAGameplayTags::State_Dead))
			{
				continue;
			}
		}
		Locations.Add(Candidate->GetActorLocation());
	}

	FMALungeParams Params;
	Params.ConeHalfAngleDeg = ConeHalfAngleDeg;
	Params.MaxLungeDistanceCm = MaxLungeDistanceCm;
	Params.StopDistanceCm = StopDistanceCm;

	const float AimYaw = AvatarPawn ? AvatarPawn->GetBaseAimRotation().Yaw : Avatar->GetActorRotation().Yaw;
	const FVector MyPos = Avatar->GetActorLocation();
	const int32 Pick = MAAttackLunge::PickLungeTarget(MyPos, AimYaw, Locations, Params);

	FVector WarpPoint;
	FRotator FaceRot(0.f, AimYaw, 0.f);
	if (Pick != INDEX_NONE)
	{
		WarpPoint = MAAttackLunge::ComputeWarpPoint(MyPos, Locations[Pick], StopDistanceCm);
		FVector To = Locations[Pick] - MyPos;
		To.Z = 0.f;
		if (To.Normalize())
		{
			FaceRot = To.Rotation();
		}
	}
	else
	{
		const FVector Fwd(FMath::Cos(FMath::DegreesToRadians(AimYaw)), FMath::Sin(FMath::DegreesToRadians(AimYaw)), 0.f);
		WarpPoint = MyPos + Fwd * NoTargetDashCm; // a whiff is a short hop, not the full authored dash
	}
	Warp->AddOrUpdateWarpTargetFromLocationAndRotation(WarpTargetName, WarpPoint, FaceRot);
}

void UGA_DashSlash::ClearWarpTarget()
{
	if (ACharacter* Avatar = Cast<ACharacter>(GetAvatarActorFromActorInfo()))
	{
		if (UMotionWarpingComponent* Warp = Avatar->FindComponentByClass<UMotionWarpingComponent>())
		{
			Warp->RemoveWarpTarget(WarpTargetName);
		}
	}
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
