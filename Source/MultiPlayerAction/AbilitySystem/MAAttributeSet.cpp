#include "AbilitySystem/MAAttributeSet.h"
#include "Net/UnrealNetwork.h"
#include "GameplayEffectExtension.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystem/MACombatantInterface.h"
#include "AbilitySystem/MAGameplayTags.h"
#include "MultiPlayerActionGameMode.h"
#include "Engine/World.h"
#include "Combat/MAHitFeel.h"
#include "GameFramework/Character.h"
#include "GameFramework/Controller.h"

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

				// Knockback: damage-scaled shove away from the instigator. Server-only by
				// construction (damage GEs execute on the authority); the un-predicted launch
				// costs a player victim one movement correction — standard engine pattern,
				// measurable via ma.PredDebug. Dodge i-frames already reject the damage GE,
				// so the State.Dodging check is belt-and-braces against ordering changes.
				// A State.Casting victim is rooted under MOVE_None where LaunchCharacter
				// no-ops — reads as intentional super-armor. Covers every damage GE through
				// this chokepoint (melee AND fireball), which is intended.
				if (MAHitFeel::IsEnabled() && !TargetASC->HasMatchingGameplayTag(MAGameplayTags::State_Dodging))
				{
					if (ACharacter* VictimChar = Cast<ACharacter>(TargetASC->GetAvatarActor_Direct()))
					{
						// Spatial source: EffectCauser is the attacking AVATAR. GetInstigator() is the
						// OwnerActor — for players that's the PlayerState, an AInfo at the world origin,
						// correct for kill attribution but useless for direction math.
						const FGameplayEffectContextHandle Ctx = Data.EffectSpec.GetEffectContext();
						const AActor* AttackSource = Ctx.GetEffectCauser() ? Ctx.GetEffectCauser() : Ctx.GetInstigator();
						FVector Dir = VictimChar->GetActorLocation()
							- (AttackSource ? AttackSource->GetActorLocation() : VictimChar->GetActorLocation());
						Dir.Z = 0.f;
						if (!Dir.Normalize())
						{
							Dir = -VictimChar->GetActorForwardVector(); // degenerate overlap — shove backward
						}
						// AI victims square up to the attack first: yaw-snap toward the attacker so
						// the knockback reads as "hit from the front" (the react anims are front-hits)
						// and the shove goes straight backward. Players are never force-rotated:
						// camera/control stays theirs. The dummy keeps APawn's default
						// bUseControllerRotationYaw=true, so its yaw is re-stamped from the AI
						// controller every tick — align the ControlRotation too or the snap lasts
						// exactly one frame.
						if (AttackSource && !VictimChar->IsPlayerControlled())
						{
							FRotator FaceAttacker = (-Dir).Rotation();
							FaceAttacker.Pitch = 0.f;
							FaceAttacker.Roll = 0.f;
							VictimChar->SetActorRotation(FaceAttacker);
							if (AController* VictimController = VictimChar->GetController())
							{
								VictimController->SetControlRotation(FaceAttacker);
							}
						}
						const float Impulse = MAHitFeel::ComputeHitFeel(Incoming).KnockbackImpulse;
						VictimChar->LaunchCharacter(Dir * Impulse + FVector(0.f, 0.f, MAHitFeel::KnockbackZBoost), false, false);
					}
				}
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
