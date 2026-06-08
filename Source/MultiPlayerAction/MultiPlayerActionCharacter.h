// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "AbilitySystemInterface.h"
#include "AbilitySystem/MACombatantInterface.h"
#include "Logging/LogMacros.h"
#include "MultiPlayerActionCharacter.generated.h"

class USpringArmComponent;
class UCameraComponent;
class UInputMappingContext;
class UInputAction;
class UMAAbilitySystemComponent;
class UMAAttributeSet;
class UGameplayAbility;
class UMALockOnComponent;
struct FInputActionValue;

DECLARE_LOG_CATEGORY_EXTERN(LogTemplateCharacter, Log, All);

UCLASS(config=Game)
class AMultiPlayerActionCharacter : public ACharacter, public IAbilitySystemInterface, public IMACombatantInterface
{
	GENERATED_BODY()

	/** Camera boom positioning the camera behind the character */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Camera, meta = (AllowPrivateAccess = "true"))
	USpringArmComponent* CameraBoom;

	/** Follow camera */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Camera, meta = (AllowPrivateAccess = "true"))
	UCameraComponent* FollowCamera;

	/** Held weapon visual, attached to the mesh's hand_r bone. The StaticMesh asset and the
	 *  grip offset are configured in BP defaults (no content references in C++). Purely
	 *  cosmetic — melee hit detection stays the GA's server-side sphere sweep. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Weapon, meta = (AllowPrivateAccess = "true"))
	TObjectPtr<class UStaticMeshComponent> WeaponMesh;

	/** Soft-lock targeting (local-player camera/UI only — does nothing on simulated proxies) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Targeting, meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UMALockOnComponent> LockOnComponent;

	/** MappingContext */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input, meta = (AllowPrivateAccess = "true"))
	UInputMappingContext* DefaultMappingContext;

	/** Jump Input Action */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input, meta = (AllowPrivateAccess = "true"))
	UInputAction* JumpAction;

	/** Move Input Action */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input, meta = (AllowPrivateAccess = "true"))
	UInputAction* MoveAction;

	/** Look Input Action */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input, meta = (AllowPrivateAccess = "true"))
	UInputAction* LookAction;

	/** Attack Input Action */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input, meta = (AllowPrivateAccess = "true"))
	UInputAction* AttackAction;

	/** Sprint Input Action — Hold trigger; press starts sprint, release ends */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input, meta = (AllowPrivateAccess = "true"))
	UInputAction* SprintAction;

	/** Block Input Action — Hold trigger; press raises the guard, release drops it */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input, meta = (AllowPrivateAccess = "true"))
	UInputAction* BlockAction;

	/** Dodge Input Action — Started fires once per press */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input, meta = (AllowPrivateAccess = "true"))
	UInputAction* DodgeAction;

	/** Fireball Input Action — Started fires once per press */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input, meta = (AllowPrivateAccess = "true"))
	UInputAction* FireballAction;

	/** Scoreboard Input Action — Hold trigger; press shows the board, release hides it */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input, meta = (AllowPrivateAccess = "true"))
	UInputAction* ScoreboardAction;

	/** Lock-on Input Action — Started toggles target lock (R3 / Middle Mouse) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input, meta = (AllowPrivateAccess = "true"))
	UInputAction* LockOnAction;

public:
	AMultiPlayerActionCharacter();

	// IAbilitySystemInterface
	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;

	UMAAttributeSet* GetAttributeSet() const { return AttributeSet; }

	// IMACombatantInterface
	virtual void HandleDeath_Implementation() override;

	/** All clients run their own ragdoll setup — local component state doesn't replicate */
	UFUNCTION(NetMulticast, Reliable)
	void Multicast_PlayDeath();

protected:
	/** Authored-death epilogue: as the clip ends, hand the corpse to physics — the final frame
	 *  is authored in place and can hover; ragdoll settles it onto the ground. Runs locally on
	 *  every machine (scheduled from Multicast_PlayDeath). */
	void StartDeathRagdoll();
	FTimerHandle DeathRagdollTimerHandle;

public:

protected:
	// Cached pointers — ASC actually lives on PlayerState
	UPROPERTY()
	TObjectPtr<UMAAbilitySystemComponent> AbilitySystemComponent;

	UPROPERTY()
	TObjectPtr<UMAAttributeSet> AttributeSet;

	/** Default abilities granted on possess */
	UPROPERTY(EditDefaultsOnly, Category = "GAS")
	TArray<TSubclassOf<UGameplayAbility>> DefaultAbilities;

	/** Periodic Infinite GE applied on possess to tick Stamina regen — set to BP_GE_StaminaRegen */
	UPROPERTY(EditDefaultsOnly, Category = "GAS")
	TSubclassOf<class UGameplayEffect> StaminaRegenEffect;

	/** Authored death animation (full body, played via single-node mode — bypasses the ABP, no
	 *  montage slot needed; non-looping so the final frame holds until the corpse despawns).
	 *  Unset = legacy ragdoll death. Set in BP defaults (e.g. Death_A_Manny). */
	UPROPERTY(EditDefaultsOnly, Category = "Death")
	TObjectPtr<class UAnimSequence> DeathAnimation;

	virtual void PossessedBy(AController* NewController) override;       // Server: init ASC
	virtual void OnRep_PlayerState() override;                           // Client: init ASC

	/** Grant default abilities to the ASC (server only) */
	void GiveDefaultAbilities();

	/** Bind ASC delegate so MoveSpeed attribute changes drive CharacterMovement.MaxWalkSpeed */
	void BindMoveSpeedDelegate();
	bool bMoveSpeedBound = false;
	void HandleMoveSpeedChange(const struct FOnAttributeChangeData& Data);


protected:

	/** Called for movement input */
	void Move(const FInputActionValue& Value);

	/** Called for looking input */
	void Look(const FInputActionValue& Value);

	/** Called for attack input — activates first available melee ability */
	void OnAttackInput();

	void OnSprintPressed();
	void OnSprintReleased();

	void OnBlockPressed();
	void OnBlockReleased();

	void OnDodgeInput();

	void OnFireballInput();

	void OnScoreboardPressed();
	void OnScoreboardReleased();

	/** Toggle soft-lock onto the best target in front; release if already locked. */
	void OnLockOnInput();

public:
	/** Console cheats mirroring the lock-on input — testable without a gamepad.
	 *  `LockOnToggle`, `LockOnSwitch 1` (right) / `LockOnSwitch -1` (left). */
	UFUNCTION(Exec)
	void LockOnToggle();

	UFUNCTION(Exec)
	void LockOnSwitch(float Direction = 1.f);

protected:

	virtual void NotifyControllerChanged() override;

	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

public:
	/** Returns CameraBoom subobject **/
	FORCEINLINE class USpringArmComponent* GetCameraBoom() const { return CameraBoom; }
	/** Returns FollowCamera subobject **/
	FORCEINLINE class UCameraComponent* GetFollowCamera() const { return FollowCamera; }
};

