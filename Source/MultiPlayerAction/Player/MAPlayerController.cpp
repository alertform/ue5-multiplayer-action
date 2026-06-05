#include "Player/MAPlayerController.h"
#include "Player/MAPlayerState.h"
#include "UI/MAUserWidget.h"
#include "AbilitySystem/MAAttributeSet.h"
#include "AbilitySystem/MAGameplayTags.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemInterface.h"
#include "GameFramework/GameModeBase.h"
#include "TimerManager.h"
#include "Engine/World.h"
#include "UObject/ConstructorHelpers.h"

AMAPlayerController::AMAPlayerController()
{
	// Bind WBP_HUD asset directly here so the C++ class is self-sufficient
	// (no BP_MAPlayerController child needed; matches the BP_ThirdPersonCharacter
	// ClassFinder pattern in MultiPlayerActionGameMode.cpp)
	static ConstructorHelpers::FClassFinder<UMAUserWidget> HUDWidgetBPClass(TEXT("/Game/Blueprints/UI/WBP_HUD"));
	if (HUDWidgetBPClass.Succeeded())
	{
		HUDWidgetClass = HUDWidgetBPClass.Class;
	}
}

void AMAPlayerController::BeginPlay()
{
	Super::BeginPlay();

	if (IsLocalController())
	{
		// The front-end menu leaves the viewport in UIOnly input mode; map travel does NOT
		// reset it — reclaim game input explicitly or all keyboard input dies in-world.
		FInputModeGameOnly InputMode;
		SetInputMode(InputMode);
		SetShowMouseCursor(false);
	}

	EnsureHUDInitialized();
}

void AMAPlayerController::OnRep_PlayerState()
{
	Super::OnRep_PlayerState();
	// Client path: PS just replicated — retry HUD init in case BeginPlay ran first
	EnsureHUDInitialized();
}

void AMAPlayerController::ScheduleRespawn(float Delay)
{
	if (!HasAuthority())
	{
		return;
	}
	GetWorldTimerManager().SetTimer(RespawnTimerHandle, this, &AMAPlayerController::Respawn, Delay, false);
}

void AMAPlayerController::Respawn()
{
	if (!HasAuthority())
	{
		return;
	}

	if (AMAPlayerState* PS = GetPlayerState<AMAPlayerState>())
	{
		if (UAbilitySystemComponent* ASC = PS->GetAbilitySystemComponent())
		{
			ASC->RemoveLooseGameplayTag(MAGameplayTags::State_Dead);
			ASC->SetNumericAttributeBase(UMAAttributeSet::GetHealthAttribute(),
				ASC->GetNumericAttribute(UMAAttributeSet::GetMaxHealthAttribute()));
			ASC->SetNumericAttributeBase(UMAAttributeSet::GetStaminaAttribute(),
				ASC->GetNumericAttribute(UMAAttributeSet::GetMaxStaminaAttribute()));
		}
	}

	// Detach from the ragdoll so RestartPlayer actually spawns a NEW pawn
	// (default impl just teleports the existing pawn if one is still attached)
	if (GetPawn())
	{
		UnPossess();
	}

	if (AGameModeBase* GM = GetWorld()->GetAuthGameMode())
	{
		GM->RestartPlayer(this);
	}
}

void AMAPlayerController::DamageSelf(float Amount)
{
	// Exec runs on the local PC; route to server RPC so the authoritative ASC is the one mutated
	Server_DamageSelf(Amount);
}

void AMAPlayerController::Server_DamageSelf_Implementation(float Amount)
{
	AMAPlayerState* PS = GetPlayerState<AMAPlayerState>();
	if (!PS) return;
	UAbilitySystemComponent* ASC = PS->GetAbilitySystemComponent();
	if (!ASC) return;

	const float Cur = ASC->GetNumericAttribute(UMAAttributeSet::GetHealthAttribute());
	ASC->SetNumericAttributeBase(UMAAttributeSet::GetHealthAttribute(), FMath::Max(0.f, Cur - Amount));

	// Direct SetNumericAttributeBase bypasses the GE pipeline — manually trip the death check
	UMAAttributeSet::CheckDeath(ASC);
}

void AMAPlayerController::EnsureHUDInitialized()
{
	// Skip on dedicated server / remote clients
	if (!IsLocalController() || !HUDWidgetClass)
	{
		return;
	}

	if (!HUDWidget)
	{
		HUDWidget = CreateWidget<UMAUserWidget>(this, HUDWidgetClass);
		if (HUDWidget)
		{
			HUDWidget->AddToViewport();
		}
	}

	// Bind to ASC if PS replicated. Idempotent on the widget side.
	if (HUDWidget)
	{
		if (AMAPlayerState* PS = GetPlayerState<AMAPlayerState>())
		{
			HUDWidget->InitFromASC(PS->GetAbilitySystemComponent());
		}
	}
}
