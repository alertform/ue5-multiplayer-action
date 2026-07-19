#include "UI/MAEscMenuWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/ButtonSlot.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Styling/CoreStyle.h"

// unity build 防碰撞：file-local 常量带文件前缀。
static const FLinearColor GEscMenuBg(0.02f, 0.02f, 0.05f, 0.94f);
static const FLinearColor GEscMenuTitle(1.0f, 0.78f, 0.35f, 1.f);
static const FLinearColor GEscMenuBtn(1.f, 1.f, 1.f, 0.08f);
static const FLinearColor GEscMenuBtnText(0.92f, 0.92f, 0.92f, 1.f);

void UMAEscMenuWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	SetVisibility(ESlateVisibility::Collapsed); // 开合由 PC 控制

	UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("Root"));
	WidgetTree->RootWidget = Root;

	UBorder* Panel = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("Panel"));
	Panel->SetBrushColor(GEscMenuBg);
	Panel->SetPadding(FMargin(34.f, 24.f));
	UCanvasPanelSlot* PanelSlot = Root->AddChildToCanvas(Panel);
	PanelSlot->SetAnchors(FAnchors(0.5f, 0.5f));
	PanelSlot->SetAlignment(FVector2D(0.5f, 0.5f));
	PanelSlot->SetAutoSize(true);

	UVerticalBox* Stack = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("Stack"));
	Panel->SetContent(Stack);

	UTextBlock* Title = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	Title->SetFont(FCoreStyle::GetDefaultFontStyle("Bold", 20));
	Title->SetColorAndOpacity(FSlateColor(GEscMenuTitle));
	Title->SetJustification(ETextJustify::Center);
	Title->SetText(FText::FromString(TEXT("菜单")));
	if (UVerticalBoxSlot* TitleSlot = Stack->AddChildToVerticalBox(Title))
	{
		TitleSlot->SetHorizontalAlignment(HAlign_Fill);
		TitleSlot->SetPadding(FMargin(0.f, 0.f, 0.f, 12.f));
	}

	ItemsBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("Items"));
	Stack->AddChildToVerticalBox(ItemsBox);
}

UButton* UMAEscMenuWidget::AddButton(const FString& Label)
{
	if (!ItemsBox)
	{
		return nullptr;
	}
	UButton* Button = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass());
	Button->SetBackgroundColor(GEscMenuBtn);

	UTextBlock* Text = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	Text->SetFont(FCoreStyle::GetDefaultFontStyle("Bold", 15));
	Text->SetColorAndOpacity(FSlateColor(GEscMenuBtnText));
	Text->SetJustification(ETextJustify::Center);
	Text->SetText(FText::FromString(Label));
	Button->AddChild(Text);
	if (UButtonSlot* TextSlot = Cast<UButtonSlot>(Text->Slot))
	{
		TextSlot->SetPadding(FMargin(28.f, 8.f));
	}

	if (UVerticalBoxSlot* BtnSlot = ItemsBox->AddChildToVerticalBox(Button))
	{
		BtnSlot->SetHorizontalAlignment(HAlign_Fill);
		BtnSlot->SetPadding(FMargin(0.f, 4.f, 0.f, 0.f));
	}
	return Button;
}

void UMAEscMenuWidget::ClearItems()
{
	if (ItemsBox)
	{
		ItemsBox->ClearChildren();
	}
}
