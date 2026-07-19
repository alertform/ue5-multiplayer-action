#include "UI/MAMatchStatusWidget.h"
#include "MAGameState.h"
#include "Player/MAPlayerState.h"
#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "GameFramework/GameMode.h"
#include "Styling/CoreStyle.h"
#include "Engine/World.h"

// File-unique names: adaptive unity build can merge UI .cpps into one TU, so file-local
// constants must not collide across files (see the NodeGraphUtils C2084 lesson).
static const FLinearColor GMatchStatusText(0.92f, 0.92f, 0.92f, 1.f);
static const FLinearColor GMatchStatusAccent(1.0f, 0.78f, 0.35f, 1.f);   // skill-bar amber
static const FLinearColor GMatchStatusShadow(0.f, 0.f, 0.f, 0.8f);

void UMAMatchStatusWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	// Never intercept input — this is a pure readout.
	SetVisibility(ESlateVisibility::HitTestInvisible);

	UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("Root"));
	WidgetTree->RootWidget = Root;

	// Top-center stack: clock over score line.
	UVerticalBox* TopStack = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("TopStack"));
	UCanvasPanelSlot* TopSlot = Root->AddChildToCanvas(TopStack);
	TopSlot->SetAnchors(FAnchors(0.5f, 0.f));
	TopSlot->SetAlignment(FVector2D(0.5f, 0.f));
	TopSlot->SetAutoSize(true);
	TopSlot->SetPosition(FVector2D(0.f, 14.f));

	ClockText = MakeText(26, GMatchStatusAccent);
	ClockText->SetText(FText::FromString(TEXT("--:--")));
	if (UVerticalBoxSlot* ClockSlot = TopStack->AddChildToVerticalBox(ClockText))
	{
		ClockSlot->SetHorizontalAlignment(HAlign_Center);
	}

	ScoreText = MakeText(15, GMatchStatusText);
	if (UVerticalBoxSlot* ScoreSlot = TopStack->AddChildToVerticalBox(ScoreText))
	{
		ScoreSlot->SetHorizontalAlignment(HAlign_Center);
		ScoreSlot->SetPadding(FMargin(0.f, 3.f, 0.f, 0.f));
	}

	// Centered respawn countdown, hidden until dead.
	RespawnText = MakeText(24, GMatchStatusText);
	UCanvasPanelSlot* RespawnSlot = Root->AddChildToCanvas(RespawnText);
	RespawnSlot->SetAnchors(FAnchors(0.5f, 0.42f));
	RespawnSlot->SetAlignment(FVector2D(0.5f, 0.5f));
	RespawnSlot->SetAutoSize(true);
	RespawnSlot->SetPosition(FVector2D(0.f, 0.f));
	RespawnText->SetVisibility(ESlateVisibility::Collapsed);

	// Who got you — sits just above the countdown, same show/hide gate.
	KilledByText = MakeText(15, GMatchStatusAccent);
	UCanvasPanelSlot* KilledBySlot = Root->AddChildToCanvas(KilledByText);
	KilledBySlot->SetAnchors(FAnchors(0.5f, 0.42f));
	KilledBySlot->SetAlignment(FVector2D(0.5f, 0.5f));
	KilledBySlot->SetAutoSize(true);
	KilledBySlot->SetPosition(FVector2D(0.f, -30.f));
	KilledByText->SetVisibility(ESlateVisibility::Collapsed);
}

void UMAMatchStatusWidget::NativeDestruct()
{
	if (AMAGameState* GS = GetWorld() ? GetWorld()->GetGameState<AMAGameState>() : nullptr)
	{
		GS->OnKillEvent.RemoveAll(this);
	}
	Super::NativeDestruct();
}

void UMAMatchStatusWidget::HandleKill(const FString& KillerName, const FString& VictimName)
{
	// Only remember who killed THIS player; empty killer (suicide) clears the line.
	const APlayerController* PC = GetOwningPlayer();
	const APlayerState* PS = PC ? PC->PlayerState : nullptr;
	if (PS && VictimName == PS->GetPlayerName())
	{
		LastKillerName = KillerName;
	}
}

void UMAMatchStatusWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	AMAGameState* GS = GetWorld() ? GetWorld()->GetGameState<AMAGameState>() : nullptr;
	if (!GS)
	{
		return;
	}

	// GameState shows up after widget creation — bind the kill event lazily, once.
	if (!bBoundToKillEvent)
	{
		GS->OnKillEvent.AddUObject(this, &UMAMatchStatusWidget::HandleKill);
		bBoundToKillEvent = true;
	}

	const float ServerNow = GS->GetServerWorldTimeSeconds();
	const FName State = GS->GetMatchState();

	if (ClockText)
	{
		if (State == MatchState::WaitingPostMatch)
		{
			ClockText->SetText(FText::FromString(TEXT("MATCH OVER")));
			ClockText->SetVisibility(ESlateVisibility::HitTestInvisible);
		}
		else if (State == MatchState::InProgress && GS->MatchEndServerTime > 0.f)
		{
			const int32 Remaining = FMath::Max(0, FMath::CeilToInt(GS->MatchEndServerTime - ServerNow));
			ClockText->SetText(FText::FromString(FString::Printf(TEXT("%02d:%02d"), Remaining / 60, Remaining % 60)));
			ClockText->SetVisibility(ESlateVisibility::HitTestInvisible);
		}
		else
		{
			// 剧情模式（不限时）：没有对局钟就不显示 —— 数据驱动，PvP 配回时限即恢复。
			ClockText->SetVisibility(ESlateVisibility::Collapsed);
		}
	}

	if (ScoreText)
	{
		const APlayerController* PC = GetOwningPlayer();
		const AMAPlayerState* PS = PC ? PC->GetPlayerState<AMAPlayerState>() : nullptr;
		// 剧情模式（无杀数目标）：K/D 读数收起，击杀反馈由任务条/击杀 feed 承担。
		if (PS && GS->KillTarget > 0)
		{
			ScoreText->SetText(FText::FromString(FString::Printf(
				TEXT("K %d / %d    D %d"), PS->GetKills(), GS->KillTarget, PS->GetDeaths())));
			ScoreText->SetVisibility(ESlateVisibility::HitTestInvisible);
		}
		else
		{
			ScoreText->SetVisibility(ESlateVisibility::Collapsed);
		}
	}

	if (RespawnText)
	{
		// Only meaningful mid-match; post-match the pinned scoreboard owns the screen.
		const float RespawnRemaining = RespawnEndServerTime - ServerNow;
		const bool bShowRespawn = State == MatchState::InProgress && RespawnRemaining > 0.f;
		if (bShowRespawn)
		{
			RespawnText->SetText(FText::FromString(FString::Printf(
				TEXT("RESPAWN IN %d"), FMath::CeilToInt(RespawnRemaining))));
		}
		RespawnText->SetVisibility(bShowRespawn ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);

		if (KilledByText)
		{
			const bool bShowKiller = bShowRespawn && !LastKillerName.IsEmpty();
			if (bShowKiller)
			{
				KilledByText->SetText(FText::FromString(FString::Printf(
					TEXT("KILLED BY %s"), *LastKillerName)));
			}
			KilledByText->SetVisibility(bShowKiller ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
		}
	}
}

UTextBlock* UMAMatchStatusWidget::MakeText(int32 FontSize, const FLinearColor& Color)
{
	UTextBlock* Text = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	Text->SetFont(FSlateFontInfo(FCoreStyle::GetDefaultFontStyle("Bold", FontSize)));
	Text->SetColorAndOpacity(FSlateColor(Color));
	Text->SetShadowOffset(FVector2D(1.f, 1.f));
	Text->SetShadowColorAndOpacity(GMatchStatusShadow);
	return Text;
}
