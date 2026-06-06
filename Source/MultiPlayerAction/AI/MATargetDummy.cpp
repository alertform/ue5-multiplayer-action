#include "AI/MATargetDummy.h"
#include "AI/MAEnemyController.h"
#include "AbilitySystem/MAAbilitySystemComponent.h"
#include "AbilitySystem/MAAttributeSet.h"
#include "AbilitySystem/MAGameplayTags.h"
#include "UI/MAHealthBarWidget.h"
#include "UI/MAUserWidget.h"
#include "AbilitySystemComponent.h"
#include "Abilities/GameplayAbility.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/WidgetComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "TimerManager.h"
#include "Engine/World.h"

AMATargetDummy::AMATargetDummy()
{
	PrimaryActorTick.bCanEverTick = false;

	bReplicates = true;
	SetReplicateMovement(true);

	// AI possess on spawn — drives the melee attack loop via MAEnemyController
	AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;
	AIControllerClass = AMAEnemyController::StaticClass();

	AbilitySystemComponent = CreateDefaultSubobject<UMAAbilitySystemComponent>(TEXT("AbilitySystemComponent"));
	AbilitySystemComponent->SetIsReplicated(true);
	// Minimal: AI/NPC — only attribute & tag changes replicate, not full GE/cooldown bookkeeping
	AbilitySystemComponent->SetReplicationMode(EGameplayEffectReplicationMode::Minimal);

	AttributeSet = CreateDefaultSubobject<UMAAttributeSet>(TEXT("AttributeSet"));

	// Floating health bar — Widget Class assigned in BP_TargetDummy defaults
	HealthBarWidget = CreateDefaultSubobject<UWidgetComponent>(TEXT("HealthBarWidget"));
	HealthBarWidget->SetupAttachment(GetCapsuleComponent());
	HealthBarWidget->SetRelativeLocation(FVector(0.f, 0.f, 110.f));
	HealthBarWidget->SetWidgetSpace(EWidgetSpace::Screen);
	HealthBarWidget->SetDrawSize(FVector2D(150.f, 20.f));
	HealthBarWidget->SetCollisionEnabled(ECollisionEnabled::NoCollision);
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

	// Server-only: init attributes, grant abilities, apply regen GE
	if (HasAuthority() && AttributeSet && AbilitySystemComponent)
	{
		AttributeSet->InitHealth(AttributeSet->GetMaxHealth());
		AttributeSet->InitStamina(100.f);

		for (const TSubclassOf<UGameplayAbility>& AbilityClass : DefaultAbilities)
		{
			if (AbilityClass)
			{
				AbilitySystemComponent->GiveAbility(FGameplayAbilitySpec(AbilityClass, 1, INDEX_NONE, this));
			}
		}

		if (StaminaRegenEffect)
		{
			FGameplayEffectContextHandle Ctx = AbilitySystemComponent->MakeEffectContext();
			Ctx.AddSourceObject(this);
			FGameplayEffectSpecHandle Spec = AbilitySystemComponent->MakeOutgoingSpec(StaminaRegenEffect, 1.f, Ctx);
			if (Spec.IsValid())
			{
				AbilitySystemComponent->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());
			}
		}
	}

	// Bind the floating health bar widget to this dummy's ASC. Each client runs locally
	// — widget instance lives in viewport, ASC replicates, attribute change delegate fires.
	if (HealthBarWidget)
	{
		if (UMAHealthBarWidget* ChipBar = Cast<UMAHealthBarWidget>(HealthBarWidget->GetUserWidgetObject()))
		{
			// Overhead chip bar (same widget as the HUD): invisible until first damage.
			ChipBar->SetHideUntilDamaged(true);
			ChipBar->InitHealthBar(AbilitySystemComponent);
		}
		else if (UMAUserWidget* Widget = Cast<UMAUserWidget>(HealthBarWidget->GetUserWidgetObject()))
		{
			Widget->InitFromASC(AbilitySystemComponent);
		}
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
