#include "UI/MAKillFeedWidget.h"
#include "MAGameState.h"
#include "GameFramework/PlayerState.h"
#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Styling/CoreStyle.h"
#include "Engine/World.h"

// File-unique names — adaptive unity build merges UI .cpps (C2084 lesson).
static const FLinearColor GKillFeedText(0.92f, 0.92f, 0.92f, 1.f);
static const FLinearColor GKillFeedAccent(1.0f, 0.78f, 0.35f, 1.f);
static const FLinearColor GKillFeedShadow(0.f, 0.f, 0.f, 0.8f);

static const float GKillFeedLineSeconds = 6.f;
static const int32 GKillFeedMaxLines = 5;

void UMAKillFeedWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	SetVisibility(ESlateVisibility::HitTestInvisible);

	UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("Root"));
	WidgetTree->RootWidget = Root;

	FeedBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("FeedBox"));
	UCanvasPanelSlot* FeedSlot = Root->AddChildToCanvas(FeedBox);
	FeedSlot->SetAnchors(FAnchors(1.f, 0.f));
	FeedSlot->SetAlignment(FVector2D(1.f, 0.f));
	FeedSlot->SetAutoSize(true);
	FeedSlot->SetPosition(FVector2D(-16.f, 16.f));
}

void UMAKillFeedWidget::NativeDestruct()
{
	if (AMAGameState* GS = GetWorld() ? GetWorld()->GetGameState<AMAGameState>() : nullptr)
	{
		GS->OnKillEvent.RemoveAll(this);
	}
	Super::NativeDestruct();
}

void UMAKillFeedWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	// The GameState may not exist when the widget is created (early HUD init) — bind lazily.
	if (!bBoundToGameState)
	{
		if (AMAGameState* GS = GetWorld() ? GetWorld()->GetGameState<AMAGameState>() : nullptr)
		{
			GS->OnKillEvent.AddUObject(this, &UMAKillFeedWidget::HandleKill);
			bBoundToGameState = true;
		}
		return;
	}

	// Expire old lines (newest live at the bottom; index 0 is always the oldest).
	const float Now = GetWorld()->GetTimeSeconds();
	while (Lines.Num() > 0 && Lines[0].ExpireTime <= Now)
	{
		FeedBox->RemoveChild(Lines[0].Text);
		Lines.RemoveAt(0);
	}
}

void UMAKillFeedWidget::HandleKill(const FString& KillerName, const FString& VictimName)
{
	if (!FeedBox)
	{
		return;
	}

	// Cap the feed — drop the oldest line to make room.
	while (Lines.Num() >= GKillFeedMaxLines)
	{
		FeedBox->RemoveChild(Lines[0].Text);
		Lines.RemoveAt(0);
	}

	const FString LineString = KillerName.IsEmpty()
		? FString::Printf(TEXT("%s died"), *VictimName)
		: FString::Printf(TEXT("%s  >  %s"), *KillerName, *VictimName);

	// Lines involving the local player render in the accent color.
	FString LocalName;
	if (const APlayerController* PC = GetOwningPlayer())
	{
		if (const APlayerState* PS = PC->PlayerState)
		{
			LocalName = PS->GetPlayerName();
		}
	}
	const bool bInvolvesLocal = !LocalName.IsEmpty()
		&& (KillerName == LocalName || VictimName == LocalName);

	UTextBlock* Text = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	Text->SetFont(FSlateFontInfo(FCoreStyle::GetDefaultFontStyle("Bold", 14)));
	Text->SetColorAndOpacity(FSlateColor(bInvolvesLocal ? GKillFeedAccent : GKillFeedText));
	Text->SetShadowOffset(FVector2D(1.f, 1.f));
	Text->SetShadowColorAndOpacity(GKillFeedShadow);
	Text->SetJustification(ETextJustify::Right);
	Text->SetText(FText::FromString(LineString));

	if (UVerticalBoxSlot* LineSlot = FeedBox->AddChildToVerticalBox(Text))
	{
		LineSlot->SetPadding(FMargin(0.f, 2.f));
		LineSlot->SetHorizontalAlignment(HAlign_Right);
	}

	FFeedLine Line;
	Line.Text = Text;
	Line.ExpireTime = GetWorld()->GetTimeSeconds() + GKillFeedLineSeconds;
	Lines.Add(Line);
}
