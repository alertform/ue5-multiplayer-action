#include "UI/MATouchControlsWidget.h"

#include "MultiPlayerActionCharacter.h"
#include "Player/MAPlayerController.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/SafeZone.h"
#include "Components/TextBlock.h"
#include "Styling/CoreStyle.h"

// File-unique names — adaptive unity build merges UI .cpps (C2084 lesson).
static const FLinearColor GTouchBtnBg(1.f, 1.f, 1.f, 0.14f);
static const FLinearColor GTouchBtnText(1.f, 1.f, 1.f, 0.85f);
static const FLinearColor GTouchBtnShadow(0.f, 0.f, 0.f, 0.7f);

void UMATouchControlsWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	SetVisibility(ESlateVisibility::SelfHitTestInvisible);

	// SafeZone 根：刘海/圆角屏自动内缩，按钮永不被遮。
	USafeZone* Safe = WidgetTree->ConstructWidget<USafeZone>(USafeZone::StaticClass(), TEXT("TouchSafeZone"));
	WidgetTree->RootWidget = Safe;

	UCanvasPanel* Canvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("TouchCanvas"));
	Safe->AddChild(Canvas);

	// 右下技能簇（拇指弧线布局：攻击最大、居右下角内侧）。
	UButton* Attack = MakeButton(Canvas, TEXT("攻击"), 132.f, FVector2D(1.f, 1.f), FVector2D(-170.f, -170.f));
	Attack->OnClicked.AddDynamic(this, &UMATouchControlsWidget::OnAttackClicked);

	UButton* Dodge = MakeButton(Canvas, TEXT("闪避"), 88.f, FVector2D(1.f, 1.f), FVector2D(-330.f, -120.f));
	Dodge->OnClicked.AddDynamic(this, &UMATouchControlsWidget::OnDodgeClicked);

	UButton* Dash = MakeButton(Canvas, TEXT("突进"), 88.f, FVector2D(1.f, 1.f), FVector2D(-300.f, -260.f));
	Dash->OnClicked.AddDynamic(this, &UMATouchControlsWidget::OnDashSlashClicked);

	UButton* Fireball = MakeButton(Canvas, TEXT("火球"), 88.f, FVector2D(1.f, 1.f), FVector2D(-160.f, -330.f));
	Fireball->OnClicked.AddDynamic(this, &UMATouchControlsWidget::OnFireballClicked);

	UButton* Jump = MakeButton(Canvas, TEXT("跳跃"), 76.f, FVector2D(1.f, 1.f), FVector2D(-40.f, -320.f));
	Jump->OnPressed.AddDynamic(this, &UMATouchControlsWidget::OnJumpPressed);
	Jump->OnReleased.AddDynamic(this, &UMATouchControlsWidget::OnJumpReleased);

	// 长按键：格挡（右簇外侧）、疾跑（左下、摇杆上方）。
	UButton* Block = MakeButton(Canvas, TEXT("格挡"), 96.f, FVector2D(1.f, 1.f), FVector2D(-440.f, -230.f));
	Block->OnPressed.AddDynamic(this, &UMATouchControlsWidget::OnBlockPressed);
	Block->OnReleased.AddDynamic(this, &UMATouchControlsWidget::OnBlockReleased);

	UButton* Sprint = MakeButton(Canvas, TEXT("疾跑"), 80.f, FVector2D(0.f, 1.f), FVector2D(60.f, -330.f));
	Sprint->OnPressed.AddDynamic(this, &UMATouchControlsWidget::OnSprintPressed);
	Sprint->OnReleased.AddDynamic(this, &UMATouchControlsWidget::OnSprintReleased);

	// 交互键：底部中央（走到 NPC 附近点它 = 键盘 T）。
	UButton* Interact = MakeButton(Canvas, TEXT("对话"), 76.f, FVector2D(0.5f, 1.f), FVector2D(120.f, -60.f));
	Interact->OnClicked.AddDynamic(this, &UMATouchControlsWidget::OnInteractClicked);
}

UButton* UMATouchControlsWidget::MakeButton(UCanvasPanel* Canvas, const FString& Label, float Size,
	const FVector2D& Anchor, const FVector2D& Offset)
{
	UButton* Button = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass());
	Button->SetBackgroundColor(GTouchBtnBg);
	Button->SetVisibility(ESlateVisibility::Visible);

	UTextBlock* Text = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	Text->SetFont(FSlateFontInfo(FCoreStyle::GetDefaultFontStyle("Bold", FMath::RoundToInt(Size * 0.22f))));
	Text->SetColorAndOpacity(FSlateColor(GTouchBtnText));
	Text->SetShadowOffset(FVector2D(1.f, 1.f));
	Text->SetShadowColorAndOpacity(GTouchBtnShadow);
	Text->SetText(FText::FromString(Label));
	Button->AddChild(Text);

	if (UCanvasPanelSlot* PanelSlot = Canvas->AddChildToCanvas(Button))
	{
		PanelSlot->SetAnchors(FAnchors(Anchor.X, Anchor.Y));
		PanelSlot->SetAlignment(FVector2D(0.5f, 0.5f));
		PanelSlot->SetPosition(Offset);
		PanelSlot->SetSize(FVector2D(Size, Size));
	}
	return Button;
}

bool UMATouchControlsWidget::CombatInputAllowed() const
{
	const AMAPlayerController* PC = GetOwningPlayer<AMAPlayerController>();
	return PC && !PC->IsDialogueOpen();
}

AMultiPlayerActionCharacter* UMATouchControlsWidget::GetMACharacter() const
{
	const APlayerController* PC = GetOwningPlayer();
	return PC ? Cast<AMultiPlayerActionCharacter>(PC->GetPawn()) : nullptr;
}

void UMATouchControlsWidget::OnAttackClicked()
{
	if (!CombatInputAllowed()) { return; }
	if (AMultiPlayerActionCharacter* C = GetMACharacter()) { C->TouchAttack(); }
}

void UMATouchControlsWidget::OnDodgeClicked()
{
	if (!CombatInputAllowed()) { return; }
	if (AMultiPlayerActionCharacter* C = GetMACharacter()) { C->TouchDodge(); }
}

void UMATouchControlsWidget::OnFireballClicked()
{
	if (!CombatInputAllowed()) { return; }
	if (AMultiPlayerActionCharacter* C = GetMACharacter()) { C->TouchFireball(); }
}

void UMATouchControlsWidget::OnDashSlashClicked()
{
	if (!CombatInputAllowed()) { return; }
	if (AMultiPlayerActionCharacter* C = GetMACharacter()) { C->TouchDashSlash(); }
}

void UMATouchControlsWidget::OnJumpPressed()
{
	if (!CombatInputAllowed()) { return; }
	if (AMultiPlayerActionCharacter* C = GetMACharacter()) { C->Jump(); }
}

void UMATouchControlsWidget::OnJumpReleased()
{
	if (AMultiPlayerActionCharacter* C = GetMACharacter()) { C->StopJumping(); }
}

void UMATouchControlsWidget::OnBlockPressed()
{
	if (!CombatInputAllowed()) { return; }
	if (AMultiPlayerActionCharacter* C = GetMACharacter()) { C->TouchBlockPressed(); }
}

void UMATouchControlsWidget::OnBlockReleased()
{
	// 松手永远放行 —— 防止对话中开窗导致格挡卡在按下态。
	if (AMultiPlayerActionCharacter* C = GetMACharacter()) { C->TouchBlockReleased(); }
}

void UMATouchControlsWidget::OnSprintPressed()
{
	if (!CombatInputAllowed()) { return; }
	if (AMultiPlayerActionCharacter* C = GetMACharacter()) { C->TouchSprintPressed(); }
}

void UMATouchControlsWidget::OnSprintReleased()
{
	if (AMultiPlayerActionCharacter* C = GetMACharacter()) { C->TouchSprintReleased(); }
}

void UMATouchControlsWidget::OnInteractClicked()
{
	if (AMAPlayerController* PC = GetOwningPlayer<AMAPlayerController>())
	{
		PC->OnInteractPressed(); // 已开窗时该函数自身早退
	}
}
