#include "UI/MAQuestTrackerWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "GameFramework/GameStateBase.h"
#include "Player/MAPlayerState.h"
#include "Styling/CoreStyle.h"

// unity build 防碰撞：file-local 常量带文件前缀（NodeGraphUtils C2084 教训）。
static const FLinearColor GQuestTrackerAccent(1.0f, 0.78f, 0.35f, 1.f);   // skill-bar amber
static const FLinearColor GQuestTrackerText(0.92f, 0.92f, 0.92f, 1.f);
static constexpr float GQuestTrackerTerminalHold = 4.f;

void UMAQuestTrackerWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	// 纯读数，不拦输入。
	SetVisibility(ESlateVisibility::HitTestInvisible);

	UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("Root"));
	WidgetTree->RootWidget = Root;

	UVerticalBox* Stack = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("Stack"));
	UCanvasPanelSlot* StackSlot = Root->AddChildToCanvas(Stack);
	StackSlot->SetAnchors(FAnchors(1.f, 0.35f));
	StackSlot->SetAlignment(FVector2D(1.f, 0.f));
	StackSlot->SetAutoSize(true);
	StackSlot->SetPosition(FVector2D(-24.f, 0.f));

	auto MakeText = [this](int32 FontSize, const FLinearColor& Color) -> UTextBlock*
	{
		UTextBlock* Text = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		Text->SetFont(FCoreStyle::GetDefaultFontStyle("Bold", FontSize));
		Text->SetColorAndOpacity(FSlateColor(Color));
		Text->SetShadowOffset(FVector2D(1.f, 1.f));
		Text->SetShadowColorAndOpacity(FLinearColor(0.f, 0.f, 0.f, 0.8f));
		return Text;
	};

	TitleText = MakeText(14, GQuestTrackerAccent);
	TitleText->SetText(FText::FromString(TEXT("任务")));
	if (UVerticalBoxSlot* TitleSlot = Stack->AddChildToVerticalBox(TitleText))
	{
		TitleSlot->SetHorizontalAlignment(HAlign_Right);
	}

	ProgressText = MakeText(16, GQuestTrackerText);
	if (UVerticalBoxSlot* ProgressSlot = Stack->AddChildToVerticalBox(ProgressText))
	{
		ProgressSlot->SetHorizontalAlignment(HAlign_Right);
		ProgressSlot->SetPadding(FMargin(0.f, 2.f, 0.f, 0.f));
	}

	SetVisibility(ESlateVisibility::Collapsed);
}

void UMAQuestTrackerWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	const APlayerController* PC = GetOwningPlayer();
	const AMAPlayerState* PS = PC ? PC->GetPlayerState<AMAPlayerState>() : nullptr;
	if (!PS)
	{
		return;
	}
	const FMAQuestState& Q = PS->GetActiveQuest();

	// 终态转换瞬间开始驻留倒计时。
	if (Q.Phase != LastPhase &&
		(Q.Phase == EMAQuestPhase::Completed || Q.Phase == EMAQuestPhase::Failed))
	{
		TerminalHoldSeconds = GQuestTrackerTerminalHold;
	}
	LastPhase = Q.Phase;

	switch (Q.Phase)
	{
	case EMAQuestPhase::Active:
	{
		SetVisibility(ESlateVisibility::HitTestInvisible);
		FString Line = FString::Printf(TEXT("击杀 %d/%d"), Q.Progress, Q.TargetKills);
		if (Q.DeadlineServerTime > 0.f)
		{
			const AGameStateBase* GS = GetWorld() ? GetWorld()->GetGameState() : nullptr;
			const double Now = GS ? GS->GetServerWorldTimeSeconds() : 0.0;
			const int32 Remain = FMath::Max(0, FMath::FloorToInt(Q.DeadlineServerTime - Now));
			Line += FString::Printf(TEXT("  %02d:%02d"), Remain / 60, Remain % 60);
		}
		ProgressText->SetText(FText::FromString(Line));
		break;
	}
	case EMAQuestPhase::Completed:
	case EMAQuestPhase::Failed:
		if (TerminalHoldSeconds > 0.f)
		{
			TerminalHoldSeconds -= InDeltaTime;
			SetVisibility(ESlateVisibility::HitTestInvisible);
			ProgressText->SetText(FText::FromString(Q.Phase == EMAQuestPhase::Completed
				? TEXT("任务完成！奖励已发放") : TEXT("任务超时失败")));
		}
		else
		{
			SetVisibility(ESlateVisibility::Collapsed);
		}
		break;
	default:
		SetVisibility(ESlateVisibility::Collapsed);
		break;
	}
}
