#include "UI/MAAnnounceWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/TextBlock.h"
#include "MAGameState.h"
#include "Styling/CoreStyle.h"

// unity build 防碰撞：file-local 常量带文件前缀。
static const FLinearColor GAnnounceText(1.0f, 0.9f, 0.65f, 1.f); // 比 HUD 金稍亮的字幕色
static constexpr float GAnnounceHoldSeconds = 4.5f;

void UMAAnnounceWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	SetVisibility(ESlateVisibility::HitTestInvisible);

	UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("Root"));
	WidgetTree->RootWidget = Root;

	LineText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	LineText->SetFont(FCoreStyle::GetDefaultFontStyle("Bold", 17));
	LineText->SetColorAndOpacity(FSlateColor(GAnnounceText));
	LineText->SetShadowOffset(FVector2D(1.f, 1.f));
	LineText->SetShadowColorAndOpacity(FLinearColor(0.f, 0.f, 0.f, 0.85f));
	LineText->SetJustification(ETextJustify::Center);
	LineText->SetVisibility(ESlateVisibility::Collapsed);

	UCanvasPanelSlot* LineSlot = Root->AddChildToCanvas(LineText);
	LineSlot->SetAnchors(FAnchors(0.5f, 0.78f));
	LineSlot->SetAlignment(FVector2D(0.5f, 0.5f));
	LineSlot->SetAutoSize(true);
	LineSlot->SetPosition(FVector2D(0.f, 0.f));
}

void UMAAnnounceWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	// GameState 晚于 widget 到位（客户端加入时序）—— 轮询绑定，KillFeed 同款。
	if (!bBoundToGameState)
	{
		if (AMAGameState* GS = GetWorld() ? GetWorld()->GetGameState<AMAGameState>() : nullptr)
		{
			GS->OnAnnounceEvent.AddUObject(this, &UMAAnnounceWidget::HandleAnnounce);
			bBoundToGameState = true;
		}
		return;
	}

	if (HoldSeconds > 0.f)
	{
		HoldSeconds -= InDeltaTime;
		if (HoldSeconds <= 0.f)
		{
			LineText->SetVisibility(ESlateVisibility::Collapsed);
		}
	}
}

void UMAAnnounceWidget::NativeDestruct()
{
	if (AMAGameState* GS = GetWorld() ? GetWorld()->GetGameState<AMAGameState>() : nullptr)
	{
		GS->OnAnnounceEvent.RemoveAll(this);
	}
	Super::NativeDestruct();
}

void UMAAnnounceWidget::HandleAnnounce(const FString& Text)
{
	if (!LineText)
	{
		return;
	}
	LineText->SetText(FText::FromString(Text));
	LineText->SetVisibility(ESlateVisibility::HitTestInvisible);
	HoldSeconds = GAnnounceHoldSeconds;
}
