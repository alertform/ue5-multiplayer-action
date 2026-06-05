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

	// Demo feedback fixes: snap the character to the aim direction so the cast animation,
	// the projectile, and the camera agree; and root the caster for the cast duration.
	// Runs on both the predicting client and the server — rotation/movement-mode replicate.
	if (ACharacter* AvatarCharacter = Cast<ACharacter>(ActorInfo->AvatarActor.Get()))
	{
		const FRotator AimYaw(0.f, AvatarCharacter->GetBaseAimRotation().Yaw, 0.f);
		AvatarCharacter->SetActorRotation(AimYaw);

		if (UCharacterMovementComponent* MoveComp = AvatarCharacter->GetCharacterMovement())
		{
			MoveComp->DisableMovement();
		}
	}

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

	// BaseAimRotation: ControlRotation for players, focal rotation for AI — one code path serves both.
	// Yaw only: the third-person camera carries a slight downward pitch, which would slam the bolt
	// into the ground a few meters out. No crosshair in this game — horizontal flight reads best.
	const FRotator SpawnRotation(0.f,
		AvatarPawn ? AvatarPawn->GetBaseAimRotation().Yaw : Avatar->GetActorRotation().Yaw, 0.f);

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

void UGA_Fireball::EndAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility, bool bWasCancelled)
{
	// Restore movement locked in ActivateAbility — but only if nothing else changed the mode
	// (death ragdoll etc. must not be stomped back to walking).
	if (ActorInfo)
	{
		if (ACharacter* AvatarCharacter = Cast<ACharacter>(ActorInfo->AvatarActor.Get()))
		{
			if (UCharacterMovementComponent* MoveComp = AvatarCharacter->GetCharacterMovement())
			{
				if (MoveComp->MovementMode == MOVE_None)
				{
					MoveComp->SetMovementMode(MOVE_Walking);
				}
			}
		}
	}

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
