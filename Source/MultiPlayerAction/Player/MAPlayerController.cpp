#include "Player/MAPlayerController.h"
#include "Player/MAPlayerState.h"
#include "UI/MAUserWidget.h"
#include "UI/MAMatchStatusWidget.h"
#include "UI/MAScoreboardWidget.h"
#include "UI/MAKillFeedWidget.h"
#include "AbilitySystem/MAAttributeSet.h"
#include "AbilitySystem/MAGameplayTags.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemInterface.h"
#include "Blueprint/UserWidget.h"
#include "GameFramework/GameMode.h"
#include "GameFramework/GameState.h"
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

	// Match UI is pure C++ — spawn straight from the classes (BP override possible but unused).
	MatchStatusWidgetClass = UMAMatchStatusWidget::StaticClass();
	ScoreboardWidgetClass = UMAScoreboardWidget::StaticClass();
	KillFeedWidgetClass = UMAKillFeedWidget::StaticClass();
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

	// Owning client renders the countdown off the synced server clock (no per-second RPCs).
	if (const AGameStateBase* GS = GetWorld()->GetGameState())
	{
		Client_OnRespawnScheduled(GS->GetServerWorldTimeSeconds() + Delay);
	}
}

void AMAPlayerController::Client_OnRespawnScheduled_Implementation(float RespawnEndServerTime)
{
	EnsureHUDInitialized();
	if (MatchStatusWidget)
	{
		MatchStatusWidget->SetRespawnEndServerTime(RespawnEndServerTime);
	}
}

void AMAPlayerController::Respawn()
{
	if (!HasAuthority())
	{
		return;
	}

	// Match ended while dead: stay on the results screen — the map restart respawns everyone.
	if (const AGameMode* GM = GetWorld()->GetAuthGameMode<AGameMode>())
	{
		if (!GM->IsMatchInProgress())
		{
			return;
		}
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

void AMAPlayerController::SetScoreboardVisible(bool bVisible)
{
	if (!ScoreboardWidget)
	{
		return;
	}

	// Post-match the board is the results screen — Tab release must not hide it.
	bool bPinned = false;
	if (const AGameState* GS = GetWorld()->GetGameState<AGameState>())
	{
		bPinned = GS->GetMatchState() == MatchState::WaitingPostMatch;
	}

	ScoreboardWidget->SetVisibility((bVisible || bPinned)
		? ESlateVisibility::HitTestInvisible
		: ESlateVisibility::Collapsed);
}

void AMAPlayerController::OnLocalMatchEnded()
{
	if (!IsLocalController())
	{
		return;
	}

	// Freeze local game input for the results screen. PC::BeginPlay reclaims GameOnly
	// after the restart travel — the exact same handoff the main menu already uses.
	FInputModeUIOnly InputMode;
	SetInputMode(InputMode);
	SetShowMouseCursor(true);

	EnsureHUDInitialized();
	SetScoreboardVisible(true);
}

void AMAPlayerController::EnsureHUDInitialized()
{
	// Skip on dedicated server / remote clients
	if (!IsLocalController())
	{
		return;
	}

	if (!HUDWidget && HUDWidgetClass)
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

	// Match overlay above the HUD bars; scoreboard above everything, hidden until Tab.
	if (!MatchStatusWidget && MatchStatusWidgetClass)
	{
		MatchStatusWidget = CreateWidget<UMAMatchStatusWidget>(this, MatchStatusWidgetClass);
		if (MatchStatusWidget)
		{
			MatchStatusWidget->AddToViewport(1);
		}
	}
	if (!ScoreboardWidget && ScoreboardWidgetClass)
	{
		ScoreboardWidget = CreateWidget<UMAScoreboardWidget>(this, ScoreboardWidgetClass);
		if (ScoreboardWidget)
		{
			ScoreboardWidget->AddToViewport(10);
			ScoreboardWidget->SetVisibility(ESlateVisibility::Collapsed);
		}
	}
	if (!KillFeedWidget && KillFeedWidgetClass)
	{
		KillFeedWidget = CreateWidget<UMAKillFeedWidget>(this, KillFeedWidgetClass);
		if (KillFeedWidget)
		{
			KillFeedWidget->AddToViewport(2);
		}
	}
}
