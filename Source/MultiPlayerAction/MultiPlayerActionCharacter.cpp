// Copyright Epic Games, Inc. All Rights Reserved.

#include "MultiPlayerActionCharacter.h"
#include "Engine/LocalPlayer.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "GameFramework/Controller.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputActionValue.h"
#include "Player/MAPlayerState.h"
#include "Player/MAPlayerController.h"
#include "Targeting/MALockOnComponent.h"
#include "MotionWarpingComponent.h"
#include "AbilitySystem/MAAbilityInputID.h"
#include "AbilitySystem/MAAbilitySystemComponent.h"
#include "AbilitySystem/MAAttributeSet.h"
#include "AbilitySystem/MAGameplayTags.h"
#include "AbilitySystemComponent.h"
#include "GameplayTagContainer.h"
#include "TimerManager.h"
#include "Network/MALagCompSubsystem.h"
#include "Network/MAPredictionMovementComponent.h"

DEFINE_LOG_CATEGORY(LogTemplateCharacter);

//////////////////////////////////////////////////////////////////////////
// GAS Init

UAbilitySystemComponent* AMultiPlayerActionCharacter::GetAbilitySystemComponent() const
{
	return AbilitySystemComponent;
}

void AMultiPlayerActionCharacter::BeginPlay()
{
	Super::BeginPlay();

	if (HasAuthority())
	{
		if (UMALagCompSubsystem* LagComp = GetWorld()->GetSubsystem<UMALagCompSubsystem>())
		{
			LagComp->RegisterTarget(this);
		}
	}
}

void AMultiPlayerActionCharacter::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (HasAuthority())
	{
		if (UWorld* W = GetWorld())
		{
			if (UMALagCompSubsystem* LagComp = W->GetSubsystem<UMALagCompSubsystem>())
			{
				LagComp->UnregisterTarget(this);
			}
		}
	}
	Super::EndPlay(EndPlayReason);
}

void AMultiPlayerActionCharacter::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);

	// Server: grab ASC from PlayerState
	if (AMAPlayerState* PS = GetPlayerState<AMAPlayerState>())
	{
		AbilitySystemComponent = Cast<UMAAbilitySystemComponent>(PS->GetAbilitySystemComponent());
		AttributeSet = PS->GetAttributeSet();
		PS->GetAbilitySystemComponent()->InitAbilityActorInfo(PS, this);
		GiveDefaultAbilities();
		BindMoveSpeedDelegate();
	}
}

void AMultiPlayerActionCharacter::GiveDefaultAbilities()
{
	if (!HasAuthority() || !AbilitySystemComponent)
	{
		return;
	}

	// Guard: default abilities + regen GE must only be granted once per ASC lifetime.
	// The ASC lives on AMAPlayerState and survives pawn death/respawn; without this guard
	// every RestartPlayer→PossessedBy re-grants every ability and stacks another
	// Infinite Regen GE, doubling the regen rate on each death.
	// Note: TargetDummy (BP_TargetDummy) is a separate class and does not call this
	// function — it grants abilities directly in its own Blueprint — so there is no
	// dummy-path concern here.
	AMAPlayerState* PS = GetPlayerState<AMAPlayerState>();
	if (PS)
	{
		if (PS->HasGrantedStartupAbilities())
		{
			return;
		}
	}

	for (const TSubclassOf<UGameplayAbility>& AbilityClass : DefaultAbilities)
	{
		if (AbilityClass)
		{
			FGameplayAbilitySpec Spec(AbilityClass, 1, INDEX_NONE, this);

			// Melee gets a GAS InputID: AbilityLocalInputPressed(Attack) activates the idle spec
			// and, while the combo ability is active, raises the replicated InputPressed event
			// its WaitInputPress task consumes as the combo buffer (client AND server).
			const UGameplayAbility* AbilityCDO = AbilityClass->GetDefaultObject<UGameplayAbility>();
			if (AbilityCDO && AbilityCDO->GetAssetTags().HasTag(MAGameplayTags::Ability_Melee_Attack))
			{
				Spec.InputID = static_cast<int32>(EMAAbilityInputID::Attack);
			}

			AbilitySystemComponent->GiveAbility(Spec);
		}
	}

	// Apply Stamina regen once on possess; Periodic Infinite GE auto-loops
	if (!StaminaRegenEffect)
	{
		// The grant-once flag below is set regardless — a null StaminaRegenEffect means regen
		// stays permanently absent for this PlayerState. Surface the misconfiguration loudly.
		UE_LOG(LogTemp, Warning, TEXT("%s: StaminaRegenEffect is not set in BP defaults — stamina will never regenerate."), *GetNameSafe(this));
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

	// Mark granted so subsequent PossessedBy calls (respawn) are no-ops for this path.
	if (PS)
	{
		PS->MarkStartupAbilitiesGranted();
	}
}

void AMultiPlayerActionCharacter::OnRep_PlayerState()
{
	Super::OnRep_PlayerState();

	// Client: same init via rep
	if (AMAPlayerState* PS = GetPlayerState<AMAPlayerState>())
	{
		AbilitySystemComponent = Cast<UMAAbilitySystemComponent>(PS->GetAbilitySystemComponent());
		AttributeSet = PS->GetAttributeSet();
		PS->GetAbilitySystemComponent()->InitAbilityActorInfo(PS, this);
		BindMoveSpeedDelegate();
	}
}

void AMultiPlayerActionCharacter::BindMoveSpeedDelegate()
{
	if (bMoveSpeedBound || !AbilitySystemComponent || !AttributeSet)
	{
		return;
	}
	bMoveSpeedBound = true;

	AbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(UMAAttributeSet::GetMoveSpeedAttribute())
		.AddUObject(this, &AMultiPlayerActionCharacter::HandleMoveSpeedChange);

	// Seed: apply current MoveSpeed to CharacterMovement immediately (replication may lag)
	if (UCharacterMovementComponent* Move = GetCharacterMovement())
	{
		Move->MaxWalkSpeed = AttributeSet->GetMoveSpeed();
	}
}

void AMultiPlayerActionCharacter::HandleMoveSpeedChange(const FOnAttributeChangeData& Data)
{
	if (UCharacterMovementComponent* Move = GetCharacterMovement())
	{
		Move->MaxWalkSpeed = Data.NewValue;
	}
}

//////////////////////////////////////////////////////////////////////////
// AMultiPlayerActionCharacter

AMultiPlayerActionCharacter::AMultiPlayerActionCharacter(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer.SetDefaultSubobjectClass<UMAPredictionMovementComponent>(ACharacter::CharacterMovementComponentName))
{
	// Set size for collision capsule
	GetCapsuleComponent()->InitCapsuleSize(42.f, 96.0f);
		
	// Don't rotate when the controller rotates. Let that just affect the camera.
	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;

	// Configure character movement
	GetCharacterMovement()->bOrientRotationToMovement = true; // Character moves in the direction of input...	
	GetCharacterMovement()->RotationRate = FRotator(0.0f, 500.0f, 0.0f); // ...at this rotation rate

	// Note: For faster iteration times these variables, and many more, can be tweaked in the Character Blueprint
	// instead of recompiling to adjust them
	GetCharacterMovement()->JumpZVelocity = 700.f;
	GetCharacterMovement()->AirControl = 0.35f;
	GetCharacterMovement()->MaxWalkSpeed = 500.f;
	GetCharacterMovement()->MinAnalogWalkSpeed = 20.f;
	GetCharacterMovement()->BrakingDecelerationWalking = 2000.f;
	GetCharacterMovement()->BrakingDecelerationFalling = 1500.0f;

	// Create a camera boom (pulls in towards the player if there is a collision)
	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(RootComponent);
	CameraBoom->TargetArmLength = 400.0f; // The camera follows at this distance behind the character	
	CameraBoom->bUsePawnControlRotation = true; // Rotate the arm based on the controller

	// Create a follow camera
	FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
	FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName); // Attach the camera to the end of the boom and let the boom adjust to match the controller orientation
	FollowCamera->bUsePawnControlRotation = false; // Camera does not rotate relative to arm

	// Held weapon visual on the right hand. Bones double as attach sockets — no socket asset
	// needed. Mesh asset + grip offset live in BP defaults; collision off (cosmetic only).
	WeaponMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("WeaponMesh"));
	WeaponMesh->SetupAttachment(GetMesh(), TEXT("hand_r"));
	WeaponMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	WeaponMesh->SetGenerateOverlapEvents(false);

	// Soft-lock targeting. Exists on every copy but only acts on the locally-controlled
	// pawn (see UMALockOnComponent::OwnerIsLocallyControlled) — no replication.
	LockOnComponent = CreateDefaultSubobject<UMALockOnComponent>(TEXT("LockOnComponent"));

	// Bends the dash slash's authored root motion onto its warp target (GA_DashSlash).
	MotionWarpingComponent = CreateDefaultSubobject<UMotionWarpingComponent>(TEXT("MotionWarping"));

	// Note: The skeletal mesh and anim blueprint references on the Mesh component (inherited from Character) 
	// are set in the derived blueprint asset named ThirdPersonCharacter (to avoid direct content references in C++)
}

//////////////////////////////////////////////////////////////////////////
// Input

void AMultiPlayerActionCharacter::NotifyControllerChanged()
{
	Super::NotifyControllerChanged();

	// Add Input Mapping Context
	if (APlayerController* PlayerController = Cast<APlayerController>(Controller))
	{
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PlayerController->GetLocalPlayer()))
		{
			Subsystem->AddMappingContext(DefaultMappingContext, 0);
		}
	}
}

void AMultiPlayerActionCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	// Set up action bindings
	if (UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(PlayerInputComponent)) {

		// Jumping
		EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Started, this, &ACharacter::Jump);
		EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Completed, this, &ACharacter::StopJumping);

		// Moving
		EnhancedInputComponent->BindAction(MoveAction, ETriggerEvent::Triggered, this, &AMultiPlayerActionCharacter::Move);

		// Looking
		EnhancedInputComponent->BindAction(LookAction, ETriggerEvent::Triggered, this, &AMultiPlayerActionCharacter::Look);

		// Attack
		if (AttackAction)
		{
			EnhancedInputComponent->BindAction(AttackAction, ETriggerEvent::Started, this, &AMultiPlayerActionCharacter::OnAttackInput);
		}

		// Sprint — Triggered (Hold) keeps activating; Completed (release) cancels
		if (SprintAction)
		{
			EnhancedInputComponent->BindAction(SprintAction, ETriggerEvent::Started, this, &AMultiPlayerActionCharacter::OnSprintPressed);
			EnhancedInputComponent->BindAction(SprintAction, ETriggerEvent::Completed, this, &AMultiPlayerActionCharacter::OnSprintReleased);
		}

		// Block — hold raises the guard, release drops it
		if (BlockAction)
		{
			EnhancedInputComponent->BindAction(BlockAction, ETriggerEvent::Started, this, &AMultiPlayerActionCharacter::OnBlockPressed);
			EnhancedInputComponent->BindAction(BlockAction, ETriggerEvent::Completed, this, &AMultiPlayerActionCharacter::OnBlockReleased);
		}

		// Dodge — single press triggers UGA_Dodge with brief i-frame
		if (DodgeAction)
		{
			EnhancedInputComponent->BindAction(DodgeAction, ETriggerEvent::Started, this, &AMultiPlayerActionCharacter::OnDodgeInput);
		}

		// Fireball — single press triggers UGA_Fireball (predicted cast + server projectile)
		if (FireballAction)
		{
			EnhancedInputComponent->BindAction(FireballAction, ETriggerEvent::Started, this, &AMultiPlayerActionCharacter::OnFireballInput);
		}

		// Dash slash — Motion-Warped root-motion katana dash
		if (DashSlashAction)
		{
			EnhancedInputComponent->BindAction(DashSlashAction, ETriggerEvent::Started, this, &AMultiPlayerActionCharacter::OnDashSlashInput);
		}

		// Scoreboard — hold Tab to peek at the standings (pure local UI, no GAS involved)
		if (ScoreboardAction)
		{
			EnhancedInputComponent->BindAction(ScoreboardAction, ETriggerEvent::Started, this, &AMultiPlayerActionCharacter::OnScoreboardPressed);
			EnhancedInputComponent->BindAction(ScoreboardAction, ETriggerEvent::Completed, this, &AMultiPlayerActionCharacter::OnScoreboardReleased);
		}

		// Lock-on — single press toggles target lock (R3 / Middle Mouse). While locked, the
		// right-stick flick that switches target is handled inside Look() via the component.
		if (LockOnAction)
		{
			EnhancedInputComponent->BindAction(LockOnAction, ETriggerEvent::Started, this, &AMultiPlayerActionCharacter::OnLockOnInput);
		}
	}
	else
	{
		UE_LOG(LogTemplateCharacter, Error, TEXT("'%s' Failed to find an Enhanced Input component! This template is built to use the Enhanced Input system. If you intend to use the legacy system, then you will need to update this C++ file."), *GetNameSafe(this));
	}
}

void AMultiPlayerActionCharacter::Move(const FInputActionValue& Value)
{
	// input is a Vector2D
	FVector2D MovementVector = Value.Get<FVector2D>();

	if (Controller != nullptr)
	{
		// find out which way is forward
		const FRotator Rotation = Controller->GetControlRotation();
		const FRotator YawRotation(0, Rotation.Yaw, 0);

		// get forward vector
		const FVector ForwardDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);
	
		// get right vector 
		const FVector RightDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);

		// add movement 
		AddMovementInput(ForwardDirection, MovementVector.Y);
		AddMovementInput(RightDirection, MovementVector.X);
	}
}

void AMultiPlayerActionCharacter::Look(const FInputActionValue& Value)
{
	// input is a Vector2D
	FVector2D LookAxisVector = Value.Get<FVector2D>();

	// While locked, the look axis drives target-switching (and the camera stays on the
	// target); HandleLookInput returns true to swallow the free-look this frame.
	if (LockOnComponent && LockOnComponent->HandleLookInput(LookAxisVector))
	{
		return;
	}

	if (Controller != nullptr)
	{
		// add yaw and pitch input to controller
		AddControllerYawInput(LookAxisVector.X);
		AddControllerPitchInput(LookAxisVector.Y);
	}
}

void AMultiPlayerActionCharacter::OnAttackInput()
{
	if (AbilitySystemComponent)
	{
		// Self-heal the zombie melee spec before routing input: instance active but the ASC
		// is no longer animating it = the montage's end delegates were skipped (observed:
		// victim-side hit-stop freeze mid-swing) and EndAbility never ran. Without this,
		// AbilityLocalInputPressed feeds every press into the dead instance's combo buffer
		// forever. Cancelling during the legit ~0.1s blend-out tail is harmless — chaining
		// is impossible there anyway, the cancel just lets this press start a fresh swing.
		FGameplayAbilitySpecHandle ZombieHandle;
		for (const FGameplayAbilitySpec& Spec : AbilitySystemComponent->GetActivatableAbilities())
		{
			if (Spec.InputID == static_cast<int32>(EMAAbilityInputID::Attack))
			{
				const UGameplayAbility* Prim = Spec.GetPrimaryInstance();
				if (Prim && Prim->IsActive()
					&& !AbilitySystemComponent->IsAnimatingAbility(const_cast<UGameplayAbility*>(Prim)))
				{
					ZombieHandle = Spec.Handle;
					UE_LOG(LogTemp, Warning, TEXT("[MBDIAG] press healing zombie melee spec (active, not animating)"));
				}
			}
		}
		if (ZombieHandle.IsValid())
		{
			AbilitySystemComponent->CancelAbilityHandle(ZombieHandle);
		}

		// InputID route instead of TryActivateAbilitiesByTag: idle spec -> activate (same as the
		// old tag path); active spec -> replicated InputPressed event = the combo input buffer.
		AbilitySystemComponent->AbilityLocalInputPressed(static_cast<int32>(EMAAbilityInputID::Attack));
	}
}

void AMultiPlayerActionCharacter::OnSprintPressed()
{
	if (AbilitySystemComponent)
	{
		FGameplayTagContainer AbilityTags;
		AbilityTags.AddTag(MAGameplayTags::Ability_Movement_Sprint);
		AbilitySystemComponent->TryActivateAbilitiesByTag(AbilityTags);
	}
}

void AMultiPlayerActionCharacter::OnSprintReleased()
{
	if (AbilitySystemComponent)
	{
		FGameplayTagContainer AbilityTags;
		AbilityTags.AddTag(MAGameplayTags::Ability_Movement_Sprint);
		AbilitySystemComponent->CancelAbilities(&AbilityTags);
	}
}

void AMultiPlayerActionCharacter::OnBlockPressed()
{
	if (AbilitySystemComponent)
	{
		FGameplayTagContainer AbilityTags;
		AbilityTags.AddTag(MAGameplayTags::Ability_Defense_Block);
		AbilitySystemComponent->TryActivateAbilitiesByTag(AbilityTags);
	}
}

void AMultiPlayerActionCharacter::OnBlockReleased()
{
	if (AbilitySystemComponent)
	{
		FGameplayTagContainer AbilityTags;
		AbilityTags.AddTag(MAGameplayTags::Ability_Defense_Block);
		AbilitySystemComponent->CancelAbilities(&AbilityTags);
	}
}

void AMultiPlayerActionCharacter::OnDodgeInput()
{
	if (AbilitySystemComponent)
	{
		FGameplayTagContainer AbilityTags;
		AbilityTags.AddTag(MAGameplayTags::Ability_Movement_Dodge);
		AbilitySystemComponent->TryActivateAbilitiesByTag(AbilityTags);
	}
}

void AMultiPlayerActionCharacter::OnDashSlashInput()
{
	if (AbilitySystemComponent)
	{
		FGameplayTagContainer AbilityTags;
		AbilityTags.AddTag(MAGameplayTags::Ability_Melee_DashSlash);
		AbilitySystemComponent->TryActivateAbilitiesByTag(AbilityTags);
	}
}

void AMultiPlayerActionCharacter::OnFireballInput()
{
	if (AbilitySystemComponent)
	{
		FGameplayTagContainer AbilityTags;
		AbilityTags.AddTag(MAGameplayTags::Ability_Ranged_Fireball);
		AbilitySystemComponent->TryActivateAbilitiesByTag(AbilityTags);
	}
}

void AMultiPlayerActionCharacter::OnScoreboardPressed()
{
	if (AMAPlayerController* PC = Cast<AMAPlayerController>(Controller))
	{
		PC->SetScoreboardVisible(true);
	}
}

void AMultiPlayerActionCharacter::OnScoreboardReleased()
{
	if (AMAPlayerController* PC = Cast<AMAPlayerController>(Controller))
	{
		PC->SetScoreboardVisible(false);
	}
}

void AMultiPlayerActionCharacter::OnLockOnInput()
{
	if (LockOnComponent)
	{
		LockOnComponent->ToggleLock();
	}
}

void AMultiPlayerActionCharacter::LockOnToggle()
{
	if (LockOnComponent)
	{
		LockOnComponent->ToggleLock();
	}
}

void AMultiPlayerActionCharacter::LockOnSwitch(float Direction)
{
	if (LockOnComponent)
	{
		LockOnComponent->SwitchTarget(Direction);
	}
}

//////////////////////////////////////////////////////////////////////////
// IMACombatantInterface

void AMultiPlayerActionCharacter::HandleDeath_Implementation()
{
	if (!HasAuthority())
	{
		return;
	}

	// Tell every client (including server) to ragdoll locally — component state doesn't replicate
	Multicast_PlayDeath();

	if (AMAPlayerController* PC = Cast<AMAPlayerController>(GetController()))
	{
		PC->ScheduleRespawn(3.f);
	}

	SetLifeSpan(5.f);
}

void AMultiPlayerActionCharacter::Multicast_PlayDeath_Implementation()
{
	if (USkeletalMeshComponent* SkelMesh = GetMesh())
	{
		if (DeathAnimation)
		{
			// Authored death: single-node playback bypasses the ABP entirely (no montage slot
			// required). Just before the clip's final frame, hand off to ragdoll — the authored
			// pose ends hovering (in-place clip), physics settles the corpse onto the ground.
			SkelMesh->PlayAnimation(DeathAnimation, false);
			const float HandoffDelay = FMath::Max(0.1f, DeathAnimation->GetPlayLength() - 0.2f);
			GetWorldTimerManager().SetTimer(DeathRagdollTimerHandle, this,
				&AMultiPlayerActionCharacter::StartDeathRagdoll, HandoffDelay, false);
		}
		else
		{
			// Legacy fallback: physics ragdoll.
			StartDeathRagdoll();
		}
	}
	if (UCapsuleComponent* Capsule = GetCapsuleComponent())
	{
		Capsule->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}
	if (UCharacterMovementComponent* Move = GetCharacterMovement())
	{
		Move->DisableMovement();
		Move->SetComponentTickEnabled(false);
	}
}

void AMultiPlayerActionCharacter::StartDeathRagdoll()
{
	if (USkeletalMeshComponent* SkelMesh = GetMesh())
	{
		SkelMesh->SetCollisionProfileName(TEXT("Ragdoll"));
		SkelMesh->SetSimulatePhysics(true);
		SkelMesh->WakeAllRigidBodies();
	}
}
