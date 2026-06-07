#include "UI/MAScoreboardWidget.h"
#include "MAGameState.h"
#include "Player/MAPlayerState.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "GameFramework/GameMode.h"
#include "Styling/CoreStyle.h"
#include "Engine/World.h"

// File-unique names — adaptive unity build merges UI .cpps (C2084 lesson).
static const FLinearColor GScoreboardText(0.92f, 0.92f, 0.92f, 1.f);
static const FLinearColor GScoreboardDim(0.62f, 0.62f, 0.62f, 1.f);
static const FLinearColor GScoreboardAccent(1.0f, 0.78f, 0.35f, 1.f);   // skill-bar amber
static const FLinearColor GScoreboardShadow(0.f, 0.f, 0.f, 0.8f);
static const FLinearColor GScoreboardPanelBg(0.02f, 0.02f, 0.04f, 0.86f);

static const float GScoreboardRefreshInterval = 0.25f;
static const float GScoreboardNumColWidth = 56.f;
static const float GScoreboardPingColWidth = 76.f;

void UMAScoreboardWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("Root"));
	WidgetTree->RootWidget = Root;

	// Centered translucent panel, sized by content with a fixed min width.
	UBorder* Panel = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("Panel"));
	Panel->SetBrushColor(GScoreboardPanelBg);
	Panel->SetPadding(FMargin(30.f, 22.f));
	UCanvasPanelSlot* PanelSlot = Root->AddChildToCanvas(Panel);
	PanelSlot->SetAnchors(FAnchors(0.5f, 0.5f));
	PanelSlot->SetAlignment(FVector2D(0.5f, 0.5f));
	PanelSlot->SetAutoSize(true);
	PanelSlot->SetPosition(FVector2D(0.f, 0.f));

	USizeBox* Sizer = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("Sizer"));
	Sizer->SetWidthOverride(520.f);
	Panel->SetContent(Sizer);

	UVerticalBox* Stack = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("Stack"));
	Sizer->SetContent(Stack);

	TitleText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	TitleText->SetFont(FSlateFontInfo(FCoreStyle::GetDefaultFontStyle("Bold", 26)));
	TitleText->SetColorAndOpacity(FSlateColor(GScoreboardAccent));
	TitleText->SetShadowOffset(FVector2D(1.f, 1.f));
	TitleText->SetShadowColorAndOpacity(GScoreboardShadow);
	TitleText->SetText(FText::FromString(TEXT("SCOREBOARD")));
	if (UVerticalBoxSlot* TitleSlot = Stack->AddChildToVerticalBox(TitleText))
	{
		TitleSlot->SetHorizontalAlignment(HAlign_Center);
	}

	SubtitleText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	SubtitleText->SetFont(FSlateFontInfo(FCoreStyle::GetDefaultFontStyle("Bold", 14)));
	SubtitleText->SetColorAndOpacity(FSlateColor(GScoreboardDim));
	if (UVerticalBoxSlot* SubSlot = Stack->AddChildToVerticalBox(SubtitleText))
	{
		SubSlot->SetHorizontalAlignment(HAlign_Center);
		SubSlot->SetPadding(FMargin(0.f, 4.f, 0.f, 0.f));
	}

	// Column headers share the row geometry so everything lines up.
	FScoreRow Header;
	UHorizontalBox* HeaderBox = MakeRow(Header, 13);
	Header.Name->SetText(FText::FromString(TEXT("PLAYER")));
	Header.Kills->SetText(FText::FromString(TEXT("K")));
	Header.Deaths->SetText(FText::FromString(TEXT("D")));
	Header.Ping->SetText(FText::FromString(TEXT("PING")));
	Header.Name->SetColorAndOpacity(FSlateColor(GScoreboardDim));
	Header.Kills->SetColorAndOpacity(FSlateColor(GScoreboardDim));
	Header.Deaths->SetColorAndOpacity(FSlateColor(GScoreboardDim));
	Header.Ping->SetColorAndOpacity(FSlateColor(GScoreboardDim));
	if (UVerticalBoxSlot* HeaderSlot = Stack->AddChildToVerticalBox(HeaderBox))
	{
		HeaderSlot->SetPadding(FMargin(0.f, 18.f, 0.f, 6.f));
	}

	RowsBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("RowsBox"));
	Stack->AddChildToVerticalBox(RowsBox);
}

void UMAScoreboardWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	// Accumulate even while hidden so the show-frame refreshes immediately.
	RefreshAccumulator += InDeltaTime;
	if (!IsVisible() || RefreshAccumulator < GScoreboardRefreshInterval)
	{
		return;
	}
	RefreshAccumulator = 0.f;
	RefreshBoard();
}

void UMAScoreboardWidget::RefreshBoard()
{
	const AMAGameState* GS = GetWorld() ? GetWorld()->GetGameState<AMAGameState>() : nullptr;
	if (!GS)
	{
		return;
	}

	// Title block: live board vs verdict + restart countdown.
	if (GS->GetMatchState() == MatchState::WaitingPostMatch)
	{
		TitleText->SetText(GS->WinnerName.IsEmpty()
			? FText::FromString(TEXT("DRAW"))
			: FText::FromString(FString::Printf(TEXT("WINNER  %s"), *GS->WinnerName)));

		const int32 Restart = FMath::Max(0, FMath::CeilToInt(GS->RestartServerTime - GS->GetServerWorldTimeSeconds()));
		SubtitleText->SetText(FText::FromString(FString::Printf(TEXT("RESTARTING IN %d"), Restart)));
	}
	else
	{
		TitleText->SetText(FText::FromString(TEXT("SCOREBOARD")));
		SubtitleText->SetText(FText::FromString(FString::Printf(TEXT("FIRST TO %d"), GS->KillTarget)));
	}

	// Standings: kills desc, deaths asc — same order the GameMode uses for the verdict.
	TArray<AMAPlayerState*> Ranked;
	for (APlayerState* PS : GS->PlayerArray)
	{
		if (AMAPlayerState* MPS = Cast<AMAPlayerState>(PS))
		{
			Ranked.Add(MPS);
		}
	}
	Ranked.Sort([](const AMAPlayerState& A, const AMAPlayerState& B)
	{
		if (A.GetKills() != B.GetKills())
		{
			return A.GetKills() > B.GetKills();
		}
		return A.GetDeaths() < B.GetDeaths();
	});

	const APlayerState* LocalPS = GetOwningPlayer() ? GetOwningPlayer()->PlayerState : nullptr;

	for (int32 i = 0; i < Ranked.Num(); ++i)
	{
		FScoreRow& Row = EnsureRow(i);
		Row.Box->SetVisibility(ESlateVisibility::HitTestInvisible);

		Row.Name->SetText(FText::FromString(Ranked[i]->GetPlayerName()));
		Row.Kills->SetText(FText::AsNumber(Ranked[i]->GetKills()));
		Row.Deaths->SetText(FText::AsNumber(Ranked[i]->GetDeaths()));
		Row.Ping->SetText(FText::AsNumber(FMath::RoundToInt(Ranked[i]->GetPingInMilliseconds())));

		// Highlight the local player's line.
		const FSlateColor RowColor(Ranked[i] == LocalPS ? GScoreboardAccent : GScoreboardText);
		Row.Name->SetColorAndOpacity(RowColor);
		Row.Kills->SetColorAndOpacity(RowColor);
		Row.Deaths->SetColorAndOpacity(RowColor);
		Row.Ping->SetColorAndOpacity(RowColor);
	}

	// Collapse surplus pooled rows (player left, etc.).
	for (int32 i = Ranked.Num(); i < Rows.Num(); ++i)
	{
		Rows[i].Box->SetVisibility(ESlateVisibility::Collapsed);
	}
}

UMAScoreboardWidget::FScoreRow& UMAScoreboardWidget::EnsureRow(int32 Index)
{
	while (Rows.Num() <= Index)
	{
		FScoreRow Row;
		UHorizontalBox* Box = MakeRow(Row, 16);
		if (UVerticalBoxSlot* RowSlot = RowsBox->AddChildToVerticalBox(Box))
		{
			RowSlot->SetPadding(FMargin(0.f, 3.f));
		}
		Rows.Add(Row);
	}
	return Rows[Index];
}

UHorizontalBox* UMAScoreboardWidget::MakeRow(FScoreRow& OutRow, int32 FontSize)
{
	UHorizontalBox* Box = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	OutRow.Box = Box;
	OutRow.Name = MakeCell(Box, 0.f, FontSize);
	OutRow.Kills = MakeCell(Box, GScoreboardNumColWidth, FontSize);
	OutRow.Deaths = MakeCell(Box, GScoreboardNumColWidth, FontSize);
	OutRow.Ping = MakeCell(Box, GScoreboardPingColWidth, FontSize);
	return Box;
}

UTextBlock* UMAScoreboardWidget::MakeCell(UHorizontalBox* Box, float FixedWidth, int32 FontSize)
{
	UTextBlock* Text = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	Text->SetFont(FSlateFontInfo(FCoreStyle::GetDefaultFontStyle("Bold", FontSize)));
	Text->SetColorAndOpacity(FSlateColor(GScoreboardText));
	Text->SetShadowOffset(FVector2D(1.f, 1.f));
	Text->SetShadowColorAndOpacity(GScoreboardShadow);

	if (FixedWidth > 0.f)
	{
		// Fixed-width numeric column, centered so header and rows align.
		Text->SetJustification(ETextJustify::Center);
		USizeBox* Cell = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		Cell->SetWidthOverride(FixedWidth);
		Cell->SetContent(Text);
		Box->AddChildToHorizontalBox(Cell);
	}
	else
	{
		// Name column fills the remaining width.
		Text->SetJustification(ETextJustify::Left);
		if (UHorizontalBoxSlot* CellSlot = Box->AddChildToHorizontalBox(Text))
		{
			CellSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			CellSlot->SetVerticalAlignment(VAlign_Center);
		}
	}
	return Text;
}
