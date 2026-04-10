#include "Player/MAPlayerState.h"
#include "AbilitySystem/MAAbilitySystemComponent.h"
#include "AbilitySystem/MAAttributeSet.h"

AMAPlayerState::AMAPlayerState()
{
	// ASC must tick and replicate
	AbilitySystemComponent = CreateDefaultSubobject<UMAAbilitySystemComponent>(TEXT("AbilitySystemComponent"));
	AbilitySystemComponent->SetIsReplicated(true);
	AbilitySystemComponent->SetReplicationMode(EGameplayEffectReplicationMode::Mixed);

	AttributeSet = CreateDefaultSubobject<UMAAttributeSet>(TEXT("AttributeSet"));

	// PlayerState replication frequency — increase for responsive health bars
	NetUpdateFrequency = 100.f;
}

UAbilitySystemComponent* AMAPlayerState::GetAbilitySystemComponent() const
{
	return AbilitySystemComponent;
}
