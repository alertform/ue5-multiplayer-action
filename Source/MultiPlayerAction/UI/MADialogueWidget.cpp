#include "UI/MADialogueWidget.h"

#include "Player/MAPlayerController.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/ButtonSlot.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/EditableTextBox.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/ScrollBox.h"
#include "Components/ScrollBoxSlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Styling/CoreStyle.h"

// File-unique names — adaptive unity build merges UI .cpps (C2084 lesson).
static const FLinearColor GDialoguePanelBg(0.02f, 0.02f, 0.04f, 0.88f);
static const FLinearColor GDialogueNpcText(0.92f, 0.92f, 0.92f, 1.f);
static const FLinearColor GDialoguePlayerText(1.0f, 0.78f, 0.35f, 1.f);   // skill-bar amber
static const FLinearColor GDialogueDim(0.55f, 0.55f, 0.58f, 1.f);
static const FLinearColor GDialogueError(0.95f, 0.35f, 0.30f, 1.f);
static const FLinearColor GDialogueTitle(1.0f, 0.88f, 0.25f, 1.f);

static const float GDialoguePanelWidth = 660.f;
static const float GDialogueHistoryHeight = 280.f;

void UMADialogueWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("DialogueRoot"));
	WidgetTree->RootWidget = Root;

	UBorder* Panel = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("DialoguePanel"));
	Panel->SetBrushColor(GDialoguePanelBg);
	Panel->SetPadding(FMargin(18.f, 14.f));
	UCanvasPanelSlot* PanelSlot = Root->AddChildToCanvas(Panel);
	PanelSlot->SetAnchors(FAnchors(0.5f, 1.f));
	PanelSlot->SetAlignment(FVector2D(0.5f, 1.f));
	PanelSlot->SetPosition(FVector2D(0.f, -48.f));
	PanelSlot->SetAutoSize(true);

	USizeBox* Sizer = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("DialogueSizer"));
	Sizer->SetWidthOverride(GDialoguePanelWidth);
	Panel->SetContent(Sizer);

	UVerticalBox* Stack = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("DialogueStack"));
	Sizer->SetContent(Stack);

	// --- 标题行：NPC 名 + ✕ ---
	UHorizontalBox* Header = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("DialogueHeader"));
	Stack->AddChildToVerticalBox(Header);

	TitleText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	TitleText->SetFont(FSlateFontInfo(FCoreStyle::GetDefaultFontStyle("Bold", 18)));
	TitleText->SetColorAndOpacity(FSlateColor(GDialogueTitle));
	if (UHorizontalBoxSlot* TitleSlot = Header->AddChildToHorizontalBox(TitleText))
	{
		TitleSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		TitleSlot->SetVerticalAlignment(VAlign_Center);
	}

	CloseButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("DialogueClose"));
	CloseButton->SetBackgroundColor(FLinearColor(1.f, 1.f, 1.f, 0.08f));
	CloseButton->OnClicked.AddDynamic(this, &UMADialogueWidget::OnCloseClicked);
	UTextBlock* CloseGlyph = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	CloseGlyph->SetFont(FSlateFontInfo(FCoreStyle::GetDefaultFontStyle("Bold", 14)));
	CloseGlyph->SetColorAndOpacity(FSlateColor(GDialogueDim));
	CloseGlyph->SetText(FText::FromString(TEXT("✕")));
	CloseButton->AddChild(CloseGlyph);
	Header->AddChildToHorizontalBox(CloseButton);

	// --- 历史区：固定高度滚动 ---
	USizeBox* HistorySizer = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("DialogueHistorySizer"));
	HistorySizer->SetHeightOverride(GDialogueHistoryHeight);
	if (UVerticalBoxSlot* HistorySlot = Stack->AddChildToVerticalBox(HistorySizer))
	{
		HistorySlot->SetPadding(FMargin(0.f, 10.f, 0.f, 0.f));
	}

	HistoryBox = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass(), TEXT("DialogueHistory"));
	HistorySizer->SetContent(HistoryBox);

	// --- 状态行 ---
	StatusText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	StatusText->SetFont(FSlateFontInfo(FCoreStyle::GetDefaultFontStyle("Italic", 12)));
	StatusText->SetColorAndOpacity(FSlateColor(GDialogueDim));
	StatusText->SetText(FText::GetEmpty());
	if (UVerticalBoxSlot* StatusSlot = Stack->AddChildToVerticalBox(StatusText))
	{
		StatusSlot->SetPadding(FMargin(0.f, 6.f, 0.f, 2.f));
	}

	// --- 输入框 ---
	InputBox = WidgetTree->ConstructWidget<UEditableTextBox>(UEditableTextBox::StaticClass(), TEXT("DialogueInput"));
	InputBox->SetHintText(FText::FromString(TEXT("输入想说的话，回车发送，Esc 关闭……")));
	// 默认 bClearKeyboardFocusOnCommit=true 会在回车提交后自动清焦点，
	// 派生出第二个 OnCleared 提交事件 —— 会被下面的 Esc 分支误判成关窗。
	// 关掉它：焦点留在框内，顺带支持连续输入。
	InputBox->SetClearKeyboardFocusOnCommit(false);
	InputBox->OnTextCommitted.AddDynamic(this, &UMADialogueWidget::OnInputCommitted);
	Stack->AddChildToVerticalBox(InputBox);
}

void UMADialogueWidget::OpenFor(const FString& NpcName, const FString& Greeting)
{
	TitleText->SetText(FText::FromString(NpcName));
	HistoryBox->ClearChildren();
	CurrentNpcLine = nullptr;
	CurrentNpcText.Reset();
	StatusText->SetText(FText::GetEmpty());
	InputBox->SetText(FText::GetEmpty());

	if (!Greeting.IsEmpty())
	{
		MakeLine(GDialogueNpcText, 14)->SetText(FText::FromString(Greeting));
	}
	ScrollToEnd();
}

void UMADialogueWidget::AppendPlayerLine(const FString& Text)
{
	// 新的一问开始 → 先封掉可能还开着的上一条 NPC 行。
	FinishNpcLine();
	MakeLine(GDialoguePlayerText, 14)->SetText(FText::FromString(FString::Printf(TEXT("你：%s"), *Text)));
	ScrollToEnd();
}

void UMADialogueWidget::SetWaitingStatus()
{
	StatusText->SetText(FText::FromString(TEXT("对方正在输入……")));
}

void UMADialogueWidget::AppendNpcDelta(const FString& Delta)
{
	if (!CurrentNpcLine)
	{
		CurrentNpcLine = MakeLine(GDialogueNpcText, 14);
		CurrentNpcText.Reset();
	}
	CurrentNpcText += Delta;
	CurrentNpcLine->SetText(FText::FromString(CurrentNpcText));
	ScrollToEnd();
}

void UMADialogueWidget::FinishNpcLine()
{
	CurrentNpcLine = nullptr;
	CurrentNpcText.Reset();
	StatusText->SetText(FText::GetEmpty());
}

void UMADialogueWidget::ShowError(const FString& Message)
{
	FinishNpcLine();
	MakeLine(GDialogueError, 12)->SetText(FText::FromString(Message));
	ScrollToEnd();
}

void UMADialogueWidget::FocusInput()
{
	if (InputBox)
	{
		InputBox->SetKeyboardFocus();
	}
}

void UMADialogueWidget::OnInputCommitted(const FText& Text, ETextCommit::Type CommitMethod)
{
	AMAPlayerController* PC = GetOwningPlayer<AMAPlayerController>();
	if (!PC)
	{
		return;
	}

	// Esc 在输入框上表现为 OnCleared —— 当作关闭对话。
	if (CommitMethod == ETextCommit::OnCleared)
	{
		PC->CloseDialogue();
		return;
	}

	if (CommitMethod != ETextCommit::OnEnter)
	{
		return;
	}

	const FString Clean = Text.ToString().TrimStartAndEnd();
	if (Clean.IsEmpty())
	{
		return;
	}

	InputBox->SetText(FText::GetEmpty());
	PC->SubmitDialogueText(Clean);
	// 回车提交会让输入框丢焦点 —— 拉回来保证连续输入。
	FocusInput();
}

void UMADialogueWidget::OnCloseClicked()
{
	if (AMAPlayerController* PC = GetOwningPlayer<AMAPlayerController>())
	{
		PC->CloseDialogue();
	}
}

UTextBlock* UMADialogueWidget::MakeLine(const FLinearColor& Color, int32 FontSize)
{
	UTextBlock* Line = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	Line->SetFont(FSlateFontInfo(FCoreStyle::GetDefaultFontStyle("Regular", FontSize)));
	Line->SetColorAndOpacity(FSlateColor(Color));
	Line->SetAutoWrapText(true);
	if (UScrollBoxSlot* LineSlot = Cast<UScrollBoxSlot>(HistoryBox->AddChild(Line)))
	{
		LineSlot->SetPadding(FMargin(0.f, 3.f));
	}
	return Line;
}

void UMADialogueWidget::ScrollToEnd()
{
	HistoryBox->ScrollToEnd();
}
