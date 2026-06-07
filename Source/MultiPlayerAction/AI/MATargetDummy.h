#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "AbilitySystemInterface.h"
#include "AbilitySystem/MACombatantInterface.h"
#include "MATargetDummy.generated.h"

class UMAAbilitySystemComponent;
class UMAAttributeSet;
class UWidgetComponent;

/**
 * Stationary practice dummy. Owns its own ASC + AttributeSet (no PlayerState),
 * so melee Damage GE can resolve a valid target ASC and we can verify the loop in PIE.
 * On death: ragdolls, hides for RespawnDelay seconds, then restores Health to MaxHealth.
 */
UCLASS()
class MULTIPLAYERACTION_API AMATargetDummy : public ACharacter, public IAbilitySystemInterface, public IMACombatantInterface
{
	GENERATED_BODY()

public:
	AMATargetDummy();

	virtual void PostInitializeComponents() override;
	virtual void BeginPlay() override;

	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;

	// IMACombatantInterface
	virtual void HandleDeath_Implementation() override;

	/** All clients run their own ragdoll setup — local component state doesn't replicate */
	UFUNCTION(NetMulticast, Reliable)
	void Multicast_PlayDeath();

	/** All clients clear their own ragdoll on respawn */
	UFUNCTION(NetMulticast, Reliable)
	void Multicast_ResetVisuals();

	/** Server-only, match ended: stop the brain mid-swing and cancel in-flight abilities.
	 *  Pose/corpse stays as-is — the post-match map restart cleans the slate. */
	void FreezeForPostMatch();

protected:
	UPROPERTY(VisibleAnywhere, Category = "GAS")
	TObjectPtr<UMAAbilitySystemComponent> AbilitySystemComponent;

	UPROPERTY()
	TObjectPtr<UMAAttributeSet> AttributeSet;

	/** Floating health bar above the dummy. BP defaults set Widget Class = WBP_EnemyHealthBar */
	UPROPERTY(VisibleAnywhere, Category = "UI")
	TObjectPtr<UWidgetComponent> HealthBarWidget;

	UPROPERTY(EditDefaultsOnly, Category = "Combat")
	float RespawnDelay = 5.f;

	/** Abilities granted on possess so AI can activate them via tag (set BP_GA_MeleeAttack here) */
	UPROPERTY(EditDefaultsOnly, Category = "GAS")
	TArray<TSubclassOf<class UGameplayAbility>> DefaultAbilities;

	/** Periodic Infinite GE applied on possess to tick Stamina regen — set to BP_GE_StaminaRegen */
	UPROPERTY(EditDefaultsOnly, Category = "GAS")
	TSubclassOf<class UGameplayEffect> StaminaRegenEffect;

	/** Authored death animation (single-node playback, final frame holds until respawn).
	 *  Unset = legacy ragdoll. Respawn restores the AnimBlueprint mode. */
	UPROPERTY(EditDefaultsOnly, Category = "Combat")
	TObjectPtr<class UAnimSequence> DeathAnimation;

	FTimerHandle RespawnTimerHandle;

	void Respawn();

	/** Shared corpse/match-end AI shutdown: stop logic, drop focus, cancel moves, wipe blackboard. */
	void StopBrainAndWipeBlackboard();

	/** Authored-death epilogue: hand the corpse to physics as the clip ends (the in-place final
	 *  frame can hover); runs locally on every machine, cleared by Multicast_ResetVisuals. */
	void StartDeathRagdoll();
	FTimerHandle DeathRagdollTimerHandle;
};
