#include "AbilitySystem/MAAttributeSet.h"
#include "Net/UnrealNetwork.h"
#include "GameplayEffectExtension.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystem/MACombatantInterface.h"
#include "AbilitySystem/MAGameplayTags.h"
#include "MultiPlayerActionGameMode.h"
#include "Engine/World.h"

UMAAttributeSet::UMAAttributeSet()
{
	InitHealth(100.f);
	InitMaxHealth(100.f);
	InitStamina(100.f);
	InitMaxStamina(100.f);
	InitAttackPower(20.f);
	InitArmor(10.f);
	InitMoveSpeed(600.f);
	// Damage is a meta attribute — never init, never replicate
}

void UMAAttributeSet::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME_CONDITION_NOTIFY(UMAAttributeSet, Health, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UMAAttributeSet, MaxHealth, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UMAAttributeSet, Stamina, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UMAAttributeSet, AttackPower, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UMAAttributeSet, Armor, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UMAAttributeSet, MaxStamina, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UMAAttributeSet, MoveSpeed, COND_None, REPNOTIFY_Always);
	// Damage is meta — not replicated
}

void UMAAttributeSet::PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data)
{
	Super::PostGameplayEffectExecute(Data);

	if (Data.EvaluatedData.Attribute == GetDamageAttribute())
	{
		// Meta attribute: route incoming Damage to Health, then zero Damage so it doesn't accumulate.
		// Lyra ULyraHealthSet::PostGameplayEffectExecute follows the same pattern.
		const float Incoming = GetDamage();
		SetDamage(0.f);
		if (Incoming > 0.f)
		{
			UAbilitySystemComponent* TargetASC = Data.Target.AbilityActorInfo->AbilitySystemComponent.Get();
			SetHealth(FMath::Clamp(GetHealth() - Incoming, 0.f, GetMaxHealth()));
			CheckDeath(TargetASC, Data.EffectSpec.GetEffectContext().GetInstigator());

			// Survivors flinch: raise the hit-react trigger (server-side; UGA_HitReact is
			// ServerInitiated, its montage replicates to every client). The dead play
			// the death animation instead — no flinch stacking.
			if (TargetASC && !TargetASC->HasMatchingGameplayTag(MAGameplayTags::State_Dead))
			{
				FGameplayEventData Payload;
				Payload.EventTag = MAGameplayTags::Event_Damage_Taken;
				Payload.EventMagnitude = Incoming;
				Payload.Instigator = Data.EffectSpec.GetEffectContext().GetInstigator();
				Payload.Target = TargetASC->GetAvatarActor_Direct();
				TargetASC->HandleGameplayEvent(MAGameplayTags::Event_Damage_Taken, &Payload);
			}
		}
	}
	else if (Data.EvaluatedData.Attribute == GetHealthAttribute())
	{
		// Direct Health writes (DamageSelf cheat / heal GE) — clamp + death check
		SetHealth(FMath::Clamp(GetHealth(), 0.f, GetMaxHealth()));
		CheckDeath(Data.Target.AbilityActorInfo->AbilitySystemComponent.Get());
	}
	else if (Data.EvaluatedData.Attribute == GetStaminaAttribute())
	{
		SetStamina(FMath::Clamp(GetStamina(), 0.f, GetMaxStamina()));
	}
}

void UMAAttributeSet::CheckDeath(UAbilitySystemComponent* ASC, AActor* KillerActor)
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

	// Scoreboard bookkeeping — only the authority has a GameMode; clients learn the
	// result through PlayerState replication.
	if (AMultiPlayerActionGameMode* GM = AvatarActor->GetWorld()->GetAuthGameMode<AMultiPlayerActionGameMode>())
	{
		GM->NotifyKill(KillerActor, AvatarActor);
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

void UMAAttributeSet::OnRep_Armor(const FGameplayAttributeData& OldArmor)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UMAAttributeSet, Armor, OldArmor);
}

void UMAAttributeSet::OnRep_MaxStamina(const FGameplayAttributeData& OldMaxStamina)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UMAAttributeSet, MaxStamina, OldMaxStamina);
}

void UMAAttributeSet::OnRep_MoveSpeed(const FGameplayAttributeData& OldMoveSpeed)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UMAAttributeSet, MoveSpeed, OldMoveSpeed);
}
