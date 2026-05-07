#include "AI/MATargetDummy.h"
#include "AbilitySystem/MAAbilitySystemComponent.h"
#include "AbilitySystem/MAAttributeSet.h"
#include "AbilitySystemComponent.h"

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
