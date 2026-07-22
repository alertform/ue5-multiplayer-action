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
#include "Input/UIActionBindingHandle.h"
#include "Styling/CoreStyle.h"

// unity build 防碰撞：file-local 常量带文件前缀。
static const FLinearColor GEscMenuBg(0.02f, 0.02f, 0.05f, 0.94f);
static const FLinearColor GEscMenuTitle(1.0f, 0.78f, 0.35f, 1.f);
static const FLinearColor GEscMenuBtn(1.f, 1.f, 1.f, 0.08f);
static const FLinearColor GEscMenuBtnText(0.92f, 0.92f, 0.92f, 1.f);

UMAEscMenuWidget::UMAEscMenuWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SetIsFocusable(true);   // 键盘/手柄事件要能路由到本 widget（NativeOnKeyDown 关菜单）
}

void UMAEscMenuWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

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

void UMAEscMenuWidget::NativeOnActivated()
{
	Super::NativeOnActivated();
	OnMenuOpened();   // lua：本实例首开建菜单项（栈每次 push 都是新实例）
}

UWidget* UMAEscMenuWidget::NativeGetDesiredFocusTarget() const
{
	return FirstButton;   // 手柄：焦点落第一个按钮，方向键上下导航
}

TOptional<FUIInputConfig> UMAEscMenuWidget::GetDesiredInputConfig() const
{
	// Menu 模式：游戏输入截断、光标显示；反激活时 ActionRouter 自动还原上一配置。
	return FUIInputConfig(ECommonInputMode::Menu, EMouseCaptureMode::NoCapture);
}

FReply UMAEscMenuWidget::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	const FKey Key = InKeyEvent.GetKey();
	if (Key == EKeys::Escape || Key == EKeys::Gamepad_FaceButton_Right ||
		Key == EKeys::Gamepad_Special_Right)
	{
		DeactivateWidget();
		return FReply::Handled();
	}
	return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
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
	if (!FirstButton)
	{
		FirstButton = Button;
	}
	return Button;
}

void UMAEscMenuWidget::ClearItems()
{
	if (ItemsBox)
	{
		ItemsBox->ClearChildren();
	}
	FirstButton = nullptr;
}
