#include "AI/MATargetDummy.h"
#include "AbilitySystem/MAAbilitySystemComponent.h"
#include "AbilitySystem/MAAttributeSet.h"
#include "AbilitySystem/MAGameplayTags.h"
#include "AbilitySystemComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "TimerManager.h"
#include "Engine/World.h"

AMATargetDummy::AMATargetDummy()
{
	PrimaryActorTick.bCanEverTick = false;

	bReplicates = true;
	SetReplicateMovement(true);

	// Stand still — no controller possesses it
	AutoPossessAI = EAutoPossessAI::Disabled;
	AIControllerClass = nullptr;

	AbilitySystemComponent = CreateDefaultSubobject<UMAAbilitySystemComponent>(TEXT("AbilitySystemComponent"));
	AbilitySystemComponent->SetIsReplicated(true);
	// Minimal: AI/NPC — only attribute & tag changes replicate, not full GE/cooldown bookkeeping
	AbilitySystemComponent->SetReplicationMode(EGameplayEffectReplicationMode::Minimal);

	AttributeSet = CreateDefaultSubobject<UMAAttributeSet>(TEXT("AttributeSet"));
}

void AMATargetDummy::PostInitializeComponents()
{
	Super::PostInitializeComponents();

	// No PlayerState — pawn is both Owner and Avatar
	if (AbilitySystemComponent)
	{
		AbilitySystemComponent->InitAbilityActorInfo(this, this);
	}
}

void AMATargetDummy::BeginPlay()
{
	Super::BeginPlay();

	// Ensure starting Health = MaxHealth on the server
	if (HasAuthority() && AttributeSet)
	{
		AttributeSet->InitHealth(AttributeSet->GetMaxHealth());
		AttributeSet->InitStamina(100.f);
	}
}

UAbilitySystemComponent* AMATargetDummy::GetAbilitySystemComponent() const
{
	return AbilitySystemComponent;
}

void AMATargetDummy::HandleDeath_Implementation()
{
	if (!HasAuthority())
	{
		return;
	}

	Multicast_PlayDeath();
	GetWorldTimerManager().SetTimer(RespawnTimerHandle, this, &AMATargetDummy::Respawn, RespawnDelay, false);
}

void AMATargetDummy::Multicast_PlayDeath_Implementation()
{
	if (USkeletalMeshComponent* SkelMesh = GetMesh())
	{
		SkelMesh->SetCollisionProfileName(TEXT("Ragdoll"));
		SkelMesh->SetSimulatePhysics(true);
		SkelMesh->WakeAllRigidBodies();
	}
	if (UCapsuleComponent* Capsule = GetCapsuleComponent())
	{
		Capsule->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}
	if (UCharacterMovementComponent* Move = GetCharacterMovement())
	{
		Move->DisableMovement();
	}
}

void AMATargetDummy::Respawn()
{
	if (!HasAuthority() || !AbilitySystemComponent || !AttributeSet)
	{
		return;
	}

	Multicast_ResetVisuals();

	// Reset attributes + clear State.Dead so next damage round starts fresh (replicates via ASC)
	AbilitySystemComponent->RemoveLooseGameplayTag(MAGameplayTags::State_Dead);
	AbilitySystemComponent->SetNumericAttributeBase(UMAAttributeSet::GetHealthAttribute(), AttributeSet->GetMaxHealth());
}

void AMATargetDummy::Multicast_ResetVisuals_Implementation()
{
	if (USkeletalMeshComponent* SkelMesh = GetMesh())
	{
		SkelMesh->SetSimulatePhysics(false);
		SkelMesh->SetCollisionProfileName(TEXT("CharacterMesh"));
		SkelMesh->AttachToComponent(GetCapsuleComponent(), FAttachmentTransformRules::SnapToTargetIncludingScale);
		SkelMesh->SetRelativeLocation(FVector(0.f, 0.f, -90.f));
		SkelMesh->SetRelativeRotation(FRotator(0.f, -90.f, 0.f));
	}
	if (UCapsuleComponent* Capsule = GetCapsuleComponent())
	{
		Capsule->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	}
	if (UCharacterMovementComponent* Move = GetCharacterMovement())
	{
		Move->SetMovementMode(MOVE_Walking);
	}
}
