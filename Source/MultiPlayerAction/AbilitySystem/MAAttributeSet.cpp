#include "AbilitySystem/MAAttributeSet.h"
#include "Net/UnrealNetwork.h"
#include "GameplayEffectExtension.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystem/MACombatantInterface.h"
#include "AbilitySystem/MAGameplayTags.h"

UMAAttributeSet::UMAAttributeSet()
{
	InitHealth(100.f);
	InitMaxHealth(100.f);
	InitStamina(100.f);
	InitAttackPower(20.f);
}

void UMAAttributeSet::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME_CONDITION_NOTIFY(UMAAttributeSet, Health, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UMAAttributeSet, MaxHealth, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UMAAttributeSet, Stamina, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UMAAttributeSet, AttackPower, COND_None, REPNOTIFY_Always);
}

void UMAAttributeSet::PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data)
{
	Super::PostGameplayEffectExecute(Data);

	if (Data.EvaluatedData.Attribute == GetHealthAttribute())
	{
		SetHealth(FMath::Clamp(GetHealth(), 0.f, GetMaxHealth()));
		CheckDeath(Data.Target.AbilityActorInfo->AbilitySystemComponent.Get());
	}
	else if (Data.EvaluatedData.Attribute == GetStaminaAttribute())
	{
		// Hardcoded upper bound 100 — promote to a MaxStamina attribute later if needed
		SetStamina(FMath::Clamp(GetStamina(), 0.f, 100.f));
	}
}

void UMAAttributeSet::CheckDeath(UAbilitySystemComponent* ASC)
{
	if (!ASC) return;
	if (ASC->HasMatchingGameplayTag(MAGameplayTags::State_Dead)) return;
	if (ASC->GetNumericAttribute(GetHealthAttribute()) > 0.f) return;

	AActor* AvatarActor = ASC->GetAvatarActor_Direct();
	if (!AvatarActor) return;

	ASC->AddLooseGameplayTag(MAGameplayTags::State_Dead);
	ASC->CancelAbilities();

	if (AvatarActor->Implements<UMACombatantInterface>())
	{
		IMACombatantInterface::Execute_HandleDeath(AvatarActor);
	}
}

void UMAAttributeSet::OnRep_Health(const FGameplayAttributeData& OldHealth)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UMAAttributeSet, Health, OldHealth);
}

void UMAAttributeSet::OnRep_MaxHealth(const FGameplayAttributeData& OldMaxHealth)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UMAAttributeSet, MaxHealth, OldMaxHealth);
}

void UMAAttributeSet::OnRep_Stamina(const FGameplayAttributeData& OldStamina)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UMAAttributeSet, Stamina, OldStamina);
}

void UMAAttributeSet::OnRep_AttackPower(const FGameplayAttributeData& OldAttackPower)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UMAAttributeSet, AttackPower, OldAttackPower);
}
