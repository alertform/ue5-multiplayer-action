// Copyright Epic Games, Inc. All Rights Reserved.

#include "MultiPlayerActionCharacter.h"
#include "Engine/LocalPlayer.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "GameFramework/Controller.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputActionValue.h"
#include "Player/MAPlayerState.h"
#include "Player/MAPlayerController.h"
#include "AbilitySystem/MAAbilitySystemComponent.h"
#include "AbilitySystem/MAAttributeSet.h"
#include "AbilitySystem/MAGameplayTags.h"
#include "AbilitySystemComponent.h"
#include "GameplayTagContainer.h"
#include "TimerManager.h"

DEFINE_LOG_CATEGORY(LogTemplateCharacter);

//////////////////////////////////////////////////////////////////////////
// GAS Init

UAbilitySystemComponent* AMultiPlayerActionCharacter::GetAbilitySystemComponent() const
{
	return AbilitySystemComponent;
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

	for (const TSubclassOf<UGameplayAbility>& AbilityClass : DefaultAbilities)
	{
		if (AbilityClass)
		{
			AbilitySystemComponent->GiveAbility(FGameplayAbilitySpec(AbilityClass, 1, INDEX_NONE, this));
		}
	}

	// Apply Stamina regen once on possess; Periodic Infinite GE auto-loops
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

AMultiPlayerActionCharacter::AMultiPlayerActionCharacter()
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

		// Dodge — single press triggers UGA_Dodge with brief i-frame
		if (DodgeAction)
		{
			EnhancedInputComponent->BindAction(DodgeAction, ETriggerEvent::Started, this, &AMultiPlayerActionCharacter::OnDodgeInput);
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
		FGameplayTagContainer AbilityTags;
		AbilityTags.AddTag(MAGameplayTags::Ability_Melee_Attack);
		AbilitySystemComponent->TryActivateAbilitiesByTag(AbilityTags);
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

void AMultiPlayerActionCharacter::OnDodgeInput()
{
	if (AbilitySystemComponent)
	{
		FGameplayTagContainer AbilityTags;
		AbilityTags.AddTag(MAGameplayTags::Ability_Movement_Dodge);
		AbilitySystemComponent->TryActivateAbilitiesByTag(AbilityTags);
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
		SkelMesh->SetCollisionProfileName(TEXT("Ragdoll"));
		SkelMesh->SetSimulatePhysics(true);
		SkelMesh->WakeAllRigidBodies();
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
