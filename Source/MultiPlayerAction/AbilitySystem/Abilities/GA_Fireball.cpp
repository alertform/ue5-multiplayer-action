#include "AbilitySystem/Abilities/GA_Fireball.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystem/MAGameplayTags.h"
#include "Combat/MAProjectile.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/SkeletalMeshComponent.h"

UGA_Fireball::UGA_Fireball()
{
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;

	// UE 5.5+ deprecates direct AbilityTags mutation — use SetAssetTags in constructor only (see GA_MeleeAttack).
	FGameplayTagContainer Tags;
	Tags.AddTag(MAGameplayTags::Ability_Ranged_Fireball);
	SetAssetTags(Tags);

	// Cannot cast while dead — checked at TryActivate time, no need for runtime guard
	ActivationBlockedTags.AddTag(MAGameplayTags::State_Dead);

	// Casting auto-cancels active sprint, same as melee
	CancelAbilitiesWithTag.AddTag(MAGameplayTags::Ability_Movement_Sprint);

	// The cast roots the character (MOVE_None) — own State.Casting so dodge/melee/sprint are
	// blocked for the duration: LaunchCharacter is silently swallowed under MOVE_None, and a
	// melee montage interrupt would waste the already-committed fireball cost + cooldown.
	ActivationOwnedTags.AddTag(MAGameplayTags::State_Casting);
	// Symmetric block: starting a fireball mid-melee-swing would interrupt the melee montage,
	// whose EndAbility restore (MOVE_None guard) would stomp the fireball's fresh root —
	// unrooted sliding cast. Blocked tags are checked before our own owned tag applies,
	// so this does NOT deadlock self-activation.
	ActivationBlockedTags.AddTag(MAGameplayTags::State_Casting);
}

void UGA_Fireball::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	if (!CastMontage || !ProjectileClass || !DamageEffect)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("UGA_Fireball: missing BP config (CastMontage=%s ProjectileClass=%s DamageEffect=%s)"),
			CastMontage ? TEXT("ok") : TEXT("NULL"),
			ProjectileClass ? TEXT("ok") : TEXT("NULL"),
			DamageEffect ? TEXT("ok") : TEXT("NULL"));
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	// Mobile cast: upper-body layering keeps the legs running; only snap to the camera yaw
	// so the cast visual starts where the player is looking.
	SnapToAimYaw(ActorInfo);

	UAbilityTask_PlayMontageAndWait* MontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(
		this, NAME_None, CastMontage, MontagePlayRate);

	// OnBlendOut + OnCompleted both fire on natural end — bind only OnCompleted to avoid double EndAbility
	MontageTask->OnCompleted.AddDynamic(this, &UGA_Fireball::OnMontageEnded);
	MontageTask->OnInterrupted.AddDynamic(this, &UGA_Fireball::OnMontageEnded);
	MontageTask->OnCancelled.AddDynamic(this, &UGA_Fireball::OnMontageEnded);
	MontageTask->ReadyForActivation();

	// Wait for the release-frame gameplay event (sent from AnimNotify in AM_FireballCast)
	UAbilityTask_WaitGameplayEvent* EventTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(
		this, MAGameplayTags::Event_Montage_SpawnProjectile);

	EventTask->EventReceived.AddDynamic(this, &UGA_Fireball::OnMontageEvent);
	EventTask->ReadyForActivation();
}

void UGA_Fireball::OnMontageEvent(FGameplayEventData EventData)
{
	SpawnProjectile(GetCurrentActorInfo());
}

void UGA_Fireball::OnMontageEnded()
{
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void UGA_Fireball::SpawnProjectile(const FGameplayAbilityActorInfo* ActorInfo)
{
	// AvatarActor (not a character cast) so AI pawns can reuse this GA later, same as melee.
	AActor* Avatar = ActorInfo ? ActorInfo->AvatarActor.Get() : nullptr;
	if (!Avatar)
	{
		return;
	}

	// Server-authoritative spawn — the owning client only plays the (predicted) cast animation;
	// the projectile reaches it via replication, masked by the cast windup.
	if (!Avatar->HasAuthority())
	{
		return;
	}

	APawn* AvatarPawn = Cast<APawn>(Avatar);

	// Muzzle: hand socket when available, actor location + forward offset otherwise.
	FVector SpawnLocation = Avatar->GetActorLocation() + Avatar->GetActorForwardVector() * 50.f;
	if (const ACharacter* AvatarCharacter = Cast<ACharacter>(Avatar))
	{
		if (AvatarCharacter->GetMesh() && AvatarCharacter->GetMesh()->DoesSocketExist(MuzzleSocketName))
		{
			SpawnLocation = AvatarCharacter->GetMesh()->GetSocketLocation(MuzzleSocketName);
		}
	}

	// Aim at what the camera is looking at: trace from the camera through screen center and fly
	// toward the impact point — handles height differences (target on a platform) naturally, and
	// aiming at the ground is now an intentional AoE ground-shot. Falls back to horizontal yaw
	// when there is no player controller (AI) or the trace hits nothing within range.
	FRotator SpawnRotation(0.f,
		AvatarPawn ? AvatarPawn->GetBaseAimRotation().Yaw : Avatar->GetActorRotation().Yaw, 0.f);
	if (const APlayerController* PC = AvatarPawn ? Cast<APlayerController>(AvatarPawn->GetController()) : nullptr)
	{
		FVector CamLoc;
		FRotator CamRot;
		PC->GetPlayerViewPoint(CamLoc, CamRot);

		FCollisionQueryParams AimParams;
		AimParams.AddIgnoredActor(Avatar);
		FHitResult AimHit;
		const FVector TraceEnd = CamLoc + CamRot.Vector() * 10000.f;
		const FVector AimPoint = Avatar->GetWorld()->LineTraceSingleByChannel(
			AimHit, CamLoc, TraceEnd, ECC_Visibility, AimParams)
			? AimHit.ImpactPoint : TraceEnd;
		SpawnRotation = (AimPoint - SpawnLocation).GetSafeNormal().Rotation();
	}

	// Snapshot the damage spec NOW — ExecCalc captures source AttackPower at spec-creation time,
	// so the fireball lands with cast-time stats even if the caster dies mid-flight.
	const FGameplayEffectSpecHandle DamageSpec = MakeOutgoingGameplayEffectSpec(DamageEffect, GetAbilityLevel());

	const FTransform SpawnTransform(SpawnRotation, SpawnLocation);
	AMAProjectile* Projectile = Avatar->GetWorld()->SpawnActorDeferred<AMAProjectile>(
		ProjectileClass, SpawnTransform, Avatar, AvatarPawn,
		ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (!Projectile)
	{
		UE_LOG(LogTemp, Warning, TEXT("UGA_Fireball: failed to spawn projectile class %s"),
			*GetNameSafe(ProjectileClass));
		return;
	}

	Projectile->InitProjectile(DamageSpec, ExplosionRadius);
	Projectile->FinishSpawning(SpawnTransform);
}

