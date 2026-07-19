#include "Player/MAPlayerState.h"
#include "AbilitySystem/MAAbilitySystemComponent.h"
#include "AbilitySystem/MAAttributeSet.h"
#include "GameplayEffect.h"
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

TArray<FString> AMAPlayerState::GetActiveBuffLines() const
{
	TArray<FString> Lines;
	if (!AbilitySystemComponent)
	{
		return Lines;
	}
	const float WorldTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;
	for (const FActiveGameplayEffectHandle& Handle :
		AbilitySystemComponent->GetActiveEffects(FGameplayEffectQuery()))
	{
		const FActiveGameplayEffect* Active = AbilitySystemComponent->GetActiveGameplayEffect(Handle);
		if (!Active || !Active->Spec.Def || Active->Spec.GetDuration() <= 0.f)
		{
			continue;
		}
		// 白名单：只报叙事增益（GE_Buff_*），冷却/常驻 regen 不进清单。
		const FString GEName = Active->Spec.Def->GetName();
		FString Label;
		if (GEName.Contains(TEXT("Buff_Speed")))       { Label = TEXT("移速提升"); }
		else if (GEName.Contains(TEXT("Buff_Attack"))) { Label = TEXT("攻击提升"); }
		else
		{
			continue;
		}
		const int32 Remain = FMath::Max(0, FMath::CeilToInt(Active->GetTimeRemaining(WorldTime)));
		Lines.Add(FString::Printf(TEXT("%s  剩 %d 秒"), *Label, Remain));
	}
	return Lines;
}
