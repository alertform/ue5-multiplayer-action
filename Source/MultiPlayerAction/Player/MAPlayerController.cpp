#include "Player/MAPlayerController.h"
#include "Player/MAPlayerState.h"
#include "UI/MAUserWidget.h"
#include "UI/MAMatchStatusWidget.h"
#include "UI/MAQuestTrackerWidget.h"
#include "UI/MAScoreboardWidget.h"
#include "UI/MAKillFeedWidget.h"
#include "UI/MADialogueWidget.h"
#include "UI/MATouchControlsWidget.h"
#include "Dialogue/MADialogueComponent.h"
#include "Dialogue/MADialogueSubsystem.h"
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
#include "UObject/UObjectIterator.h"
#include "GameFramework/Pawn.h"

// 对话交互的客户端侧诊断——默认静默，测试时 -LogCmds="LogMADialogueInput Verbose" 开启
DEFINE_LOG_CATEGORY_STATIC(LogMADialogueInput, Log, All);

static TAutoConsoleVariable<int32> CVarTouchControls(
	TEXT("ma.TouchControls"), -1,
	TEXT("-1 = auto (touch platforms only), 0 = force off, 1 = force on (PC 上鼠标点按调试)."));

static bool ShouldShowTouchControls()
{
	switch (CVarTouchControls.GetValueOnGameThread())
	{
	case 0:  return false;
	case 1:  return true;
	default:
#if PLATFORM_ANDROID || PLATFORM_IOS
		return true;
#else
		return false;
#endif
	}
}

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
	TouchControlsWidgetClass = UMATouchControlsWidget::StaticClass();
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

// ===== NPC LLM 流式对话 =====

void AMAPlayerController::OnInteractPressed()
{
	UE_LOG(LogMADialogueInput, Verbose, TEXT("OnInteractPressed: bOpen=%d pawn=%s"),
		bDialogueOpen ? 1 : 0, GetPawn() ? *GetPawn()->GetActorLocation().ToCompactString() : TEXT("none"));
	if (bDialogueOpen)
	{
		return;
	}

	UMADialogueComponent* Npc = FindNearbyDialogueNpc();
	if (!Npc)
	{
		return;
	}

	if (!DialogueWidget)
	{
		DialogueWidget = CreateWidget<UMADialogueWidget>(this, UMADialogueWidget::StaticClass());
		if (!DialogueWidget)
		{
			return;
		}
		DialogueWidget->AddToViewport(20);
	}

	// 本地立即开窗、上开场白 —— 不等 server 往返。
	DialogueWidget->SetVisibility(ESlateVisibility::Visible);
	DialogueWidget->OpenFor(Npc->NpcName, Npc->Greeting);
	bDialogueOpen = true;
	ClientDialogueMessageId = INDEX_NONE;

	FInputModeUIOnly InputMode;
	InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	SetInputMode(InputMode);
	SetShowMouseCursor(true);
	DialogueWidget->FocusInput();

	Server_StartDialogue(Npc->GetOwner());
}

UMADialogueComponent* AMAPlayerController::FindNearbyDialogueNpc() const
{
	const APawn* MyPawn = GetPawn();
	if (!MyPawn)
	{
		return nullptr;
	}

	// 演示规模下全量遍历即可（世界里就几个 NPC）；GetWorld 比对天然滤掉 CDO/别的世界。
	UMADialogueComponent* Best = nullptr;
	float BestDistSq = TNumericLimits<float>::Max();
	for (UMADialogueComponent* Comp : TObjectRange<UMADialogueComponent>())
	{
		if (!IsValid(Comp) || Comp->GetWorld() != GetWorld() || !Comp->GetOwner())
		{
			continue;
		}
		const float DistSq = FVector::DistSquared(
			MyPawn->GetActorLocation(), Comp->GetOwner()->GetActorLocation());
		UE_LOG(LogMADialogueInput, Verbose, TEXT("candidate %s dist=%.0f radius=%.0f"),
			*Comp->NpcName, FMath::Sqrt(DistSq), Comp->InteractRadius);
		if (DistSq <= FMath::Square(Comp->InteractRadius) && DistSq < BestDistSq)
		{
			Best = Comp;
			BestDistSq = DistSq;
		}
	}
	return Best;
}

void AMAPlayerController::SubmitDialogueText(const FString& Text)
{
	if (!bDialogueOpen || !DialogueWidget)
	{
		return;
	}
	DialogueWidget->AppendPlayerLine(Text);
	DialogueWidget->SetWaitingStatus();
	Server_SendDialogueMessage(Text);
}

void AMAPlayerController::CloseDialogue()
{
	if (!bDialogueOpen)
	{
		return;
	}
	bDialogueOpen = false;
	ClientDialogueMessageId = INDEX_NONE;

	if (DialogueWidget)
	{
		DialogueWidget->SetVisibility(ESlateVisibility::Collapsed);
	}

	FInputModeGameOnly InputMode;
	SetInputMode(InputMode);
	SetShowMouseCursor(false);

	Server_EndDialogue();
}

void AMAPlayerController::Server_StartDialogue_Implementation(AActor* NpcActor)
{
	if (!NpcActor)
	{
		return;
	}
	if (UMADialogueSubsystem* Dialogue = GetWorld()->GetSubsystem<UMADialogueSubsystem>())
	{
		Dialogue->StartSession(this, NpcActor->FindComponentByClass<UMADialogueComponent>());
	}
}

void AMAPlayerController::Server_SendDialogueMessage_Implementation(const FString& Text)
{
	if (UMADialogueSubsystem* Dialogue = GetWorld()->GetSubsystem<UMADialogueSubsystem>())
	{
		Dialogue->SendPlayerMessage(this, Text);
	}
}

void AMAPlayerController::Server_EndDialogue_Implementation()
{
	if (UMADialogueSubsystem* Dialogue = GetWorld()->GetSubsystem<UMADialogueSubsystem>())
	{
		Dialogue->EndSession(this);
	}
}

void AMAPlayerController::Client_DialogueDelta_Implementation(int32 MessageId, const FString& Text)
{
	if (!bDialogueOpen || !DialogueWidget)
	{
		return;
	}
	if (MessageId != ClientDialogueMessageId)
	{
		// 新回复开始（或上一条被打断）——封旧行，起新行。
		DialogueWidget->FinishNpcLine();
		ClientDialogueMessageId = MessageId;
	}
	DialogueWidget->AppendNpcDelta(Text);
}

void AMAPlayerController::Client_DialogueCompleted_Implementation(int32 MessageId)
{
	if (!bDialogueOpen || !DialogueWidget || MessageId != ClientDialogueMessageId)
	{
		return;
	}
	DialogueWidget->FinishNpcLine();
	ClientDialogueMessageId = INDEX_NONE;
}

void AMAPlayerController::Client_DialogueError_Implementation(const FString& Message)
{
	if (!bDialogueOpen || !DialogueWidget)
	{
		return;
	}
	DialogueWidget->ShowError(Message);
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
	if (!QuestTrackerWidget)
	{
		QuestTrackerWidget = CreateWidget<UMAQuestTrackerWidget>(this, UMAQuestTrackerWidget::StaticClass());
		if (QuestTrackerWidget)
		{
			QuestTrackerWidget->AddToViewport(1);
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

	// 触屏操作层：仅触屏平台（或 ma.TouchControls 1 强制）时挂载，盖在 HUD 之上。
	if (!TouchControlsWidget && TouchControlsWidgetClass && ShouldShowTouchControls())
	{
		TouchControlsWidget = CreateWidget<UMATouchControlsWidget>(this, TouchControlsWidgetClass);
		if (TouchControlsWidget)
		{
			TouchControlsWidget->AddToViewport(5);
		}
	}
}
