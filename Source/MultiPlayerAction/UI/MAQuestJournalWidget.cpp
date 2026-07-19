#include "UI/MAQuestJournalWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Styling/CoreStyle.h"

// unity build 防碰撞：file-local 常量带文件前缀。
static const FLinearColor GQuestJournalBg(0.03f, 0.03f, 0.06f, 0.92f);
static const FLinearColor GQuestJournalTitle(1.0f, 0.78f, 0.35f, 1.f);

void UMAQuestJournalWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	SetVisibility(ESlateVisibility::Collapsed); // 根开合由 PC 控制；内容刷新才依赖 tick——
	// 注意根 Collapsed 时 NativeTick 停跑，所以刷新脉冲只在可见期发（正是想要的行为）。

	UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("Root"));
	WidgetTree->RootWidget = Root;

	UBorder* Panel = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("Panel"));
	Panel->SetBrushColor(GQuestJournalBg);
	Panel->SetPadding(FMargin(18.f, 14.f));
	UCanvasPanelSlot* PanelSlot = Root->AddChildToCanvas(Panel);
	PanelSlot->SetAnchors(FAnchors(0.f, 0.5f));
	PanelSlot->SetAlignment(FVector2D(0.f, 0.5f));
	PanelSlot->SetAutoSize(true);
	PanelSlot->SetPosition(FVector2D(28.f, 0.f));

	UVerticalBox* Stack = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("Stack"));
	Panel->SetContent(Stack);

	TitleText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	TitleText->SetFont(FCoreStyle::GetDefaultFontStyle("Bold", 18));
	TitleText->SetColorAndOpacity(FSlateColor(GQuestJournalTitle));
	TitleText->SetText(FText::FromString(TEXT("任务日志")));
	Stack->AddChildToVerticalBox(TitleText);

	LinesBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("Lines"));
	if (UVerticalBoxSlot* LinesSlot = Stack->AddChildToVerticalBox(LinesBox))
	{
		LinesSlot->SetPadding(FMargin(0.f, 8.f, 0.f, 0.f));
	}
}

void UMAQuestJournalWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	RefreshAccum += InDeltaTime;
	if (RefreshAccum >= 0.25f)
	{
		RefreshAccum = 0.f;
		OnJournalRefresh(); // lua 实现；未绑定模块时为空操作
	}
}

UTextBlock* UMAQuestJournalWidget::AddLine(int32 FontSize, FLinearColor Color)
{
	if (!LinesBox)
	{
		return nullptr;
	}
	UTextBlock* Line = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	Line->SetFont(FCoreStyle::GetDefaultFontStyle("Regular", FontSize));
	Line->SetColorAndOpacity(FSlateColor(Color));
	Line->SetShadowOffset(FVector2D(1.f, 1.f));
	Line->SetShadowColorAndOpacity(FLinearColor(0.f, 0.f, 0.f, 0.8f));
	if (UVerticalBoxSlot* LineSlot = LinesBox->AddChildToVerticalBox(Line))
	{
		LineSlot->SetPadding(FMargin(0.f, 2.f, 0.f, 0.f));
	}
	return Line;
}

void UMAQuestJournalWidget::ClearLines()
{
	if (LinesBox)
	{
		LinesBox->ClearChildren();
	}
}
