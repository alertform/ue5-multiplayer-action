#include "Player/MAPlayerState.h"
#include "AbilitySystem/MAAbilitySystemComponent.h"
#include "AbilitySystem/MAAttributeSet.h"
#include "Net/UnrealNetwork.h"

AMAPlayerState::AMAPlayerState()
{
	// ASC must tick and replicate
	AbilitySystemComponent = CreateDefaultSubobject<UMAAbilitySystemComponent>(TEXT("AbilitySystemComponent"));
	AbilitySystemComponent->SetIsReplicated(true);
	AbilitySystemComponent->SetReplicationMode(EGameplayEffectReplicationMode::Mixed);

	AttributeSet = CreateDefaultSubobject<UMAAttributeSet>(TEXT("AttributeSet"));

	// PlayerState replication frequency — increase for responsive health bars
	SetNetUpdateFrequency(100.f);
}

UAbilitySystemComponent* AMAPlayerState::GetAbilitySystemComponent() const
{
	return AbilitySystemComponent;
}

void AMAPlayerState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AMAPlayerState, Kills);
	DOREPLIFETIME(AMAPlayerState, Deaths);
	DOREPLIFETIME(AMAPlayerState, ActiveQuest);
	DOREPLIFETIME(AMAPlayerState, NpcFavor);
}
