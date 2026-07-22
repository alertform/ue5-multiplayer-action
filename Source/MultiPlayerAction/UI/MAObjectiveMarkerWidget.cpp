#include "UI/MAObjectiveMarkerWidget.h"

#include "AI/MATargetDummy.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Dialogue/MADialogueComponent.h"
#include "Engine/GameViewportClient.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Narrative/MAQuestTypes.h"
#include "Player/MAPlayerState.h"
#include "Styling/CoreStyle.h"
#include "UObject/UObjectIterator.h"

static const FLinearColor GObjectiveColor(1.f, 0.78f, 0.35f, 1.f);   // HUD 金色系
static constexpr float GObjectiveScanInterval = 0.5f;
static constexpr float GObjectiveEdgeMargin = 64.f;   // 屏幕边缘留白(像素)
static constexpr float GObjectiveHideMeters = 4.f;    // 近于此不再指引
static constexpr float GObjectiveHeadOffset = 110.f;  // 浮标抬到目标头顶(uu)

void UMAObjectiveMarkerWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	SetVisibility(ESlateVisibility::HitTestInvisible);

	UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("Root"));
	WidgetTree->RootWidget = Root;

	Container = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("Container"));
	UCanvasPanelSlot* CS = Root->AddChildToCanvas(Container);
	CS->SetAutoSize(true);
	CS->SetAlignment(FVector2D(0.5f, 0.5f));   // 以容器中心对齐到目标屏幕点

	IconText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	IconText->SetFont(FCoreStyle::GetDefaultFontStyle("Bold", 22));
	IconText->SetColorAndOpacity(FSlateColor(GObjectiveColor));
	IconText->SetShadowOffset(FVector2D(1.f, 1.f));
	IconText->SetShadowColorAndOpacity(FLinearColor(0.f, 0.f, 0.f, 0.9f));
	if (UVerticalBoxSlot* S = Container->AddChildToVerticalBox(IconText))
	{
		S->SetHorizontalAlignment(HAlign_Center);
	}

	LabelText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	LabelText->SetFont(FCoreStyle::GetDefaultFontStyle("Bold", 12));
	LabelText->SetColorAndOpacity(FSlateColor(FLinearColor(0.92f, 0.92f, 0.92f, 1.f)));
	LabelText->SetShadowOffset(FVector2D(1.f, 1.f));
	LabelText->SetShadowColorAndOpacity(FLinearColor(0.f, 0.f, 0.f, 0.9f));
	if (UVerticalBoxSlot* S = Container->AddChildToVerticalBox(LabelText))
	{
		S->SetHorizontalAlignment(HAlign_Center);
		S->SetPadding(FMargin(0.f, 1.f, 0.f, 0.f));
	}

	Container->SetVisibility(ESlateVisibility::Collapsed);
}

void UMAObjectiveMarkerWidget::RefreshSources()
{
	EnemySources.Reset();
	NpcSources.Reset();
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	for (TActorIterator<AMATargetDummy> It(World); It; ++It)
	{
		EnemySources.Add(*It);
	}
	for (TObjectIterator<UMADialogueComponent> It; It; ++It)
	{
		if (It->GetWorld() == World && It->GetOwner())
		{
			NpcSources.Add(It->GetOwner());
		}
	}
}

AActor* UMAObjectiveMarkerWidget::ResolveObjective(const APawn* Pawn, bool bQuestActive, FString& OutName) const
{
	if (bQuestActive)
	{
		AActor* Nearest = nullptr;
		float BestSq = TNumericLimits<float>::Max();
		for (const TWeakObjectPtr<AActor>& E : EnemySources)
		{
			if (!E.IsValid() || E->IsHidden())
			{
				continue;   // 隐身 = 死亡待重生 / 已清场，不作导航目标
			}
			const float DSq = FVector::DistSquared(E->GetActorLocation(), Pawn->GetActorLocation());
			if (DSq < BestSq)
			{
				BestSq = DSq;
				Nearest = E.Get();
			}
		}
		OutName = TEXT("敌人");
		return Nearest;
	}

	for (const TWeakObjectPtr<AActor>& N : NpcSources)
	{
		if (!N.IsValid() || N->IsHidden())
		{
			continue;   // 剑客离场(接任务节拍)时不指引
		}
		OutName = TEXT("云游剑客");
		if (const UMADialogueComponent* Dlg = N->FindComponentByClass<UMADialogueComponent>())
		{
			OutName = Dlg->NpcName;
		}
		return N.Get();
	}
	return nullptr;
}

void UMAObjectiveMarkerWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	ScanCooldown -= InDeltaTime;
	if (ScanCooldown <= 0.f)
	{
		ScanCooldown = GObjectiveScanInterval;
		RefreshSources();
	}

	APlayerController* PC = GetOwningPlayer();
	const APawn* Pawn = GetOwningPlayerPawn();
	const AMAPlayerState* PS = PC ? PC->GetPlayerState<AMAPlayerState>() : nullptr;
	if (!PC || !Pawn || !PS)
	{
		Container->SetVisibility(ESlateVisibility::Collapsed);
		return;
	}

	const bool bQuestActive = PS->GetActiveQuest().Phase == EMAQuestPhase::Active;
	FString Name;
	AActor* Target = ResolveObjective(Pawn, bQuestActive, Name);
	if (!Target)
	{
		Container->SetVisibility(ESlateVisibility::Collapsed);
		return;
	}

	const FVector TargetLoc = Target->GetActorLocation();
	const float Meters = FVector::Dist2D(TargetLoc, Pawn->GetActorLocation()) / 100.f;
	if (Meters < GObjectiveHideMeters)
	{
		Container->SetVisibility(ESlateVisibility::Collapsed);   // 到跟前了，不挡视线
		return;
	}
	Container->SetVisibility(ESlateVisibility::HitTestInvisible);

	// 视口尺寸(像素) + DPI；投影给的是像素坐标，Canvas 槽要 Slate 单位。
	FVector2D ViewportPx(1920.f, 1080.f);
	if (GEngine && GEngine->GameViewport)
	{
		GEngine->GameViewport->GetViewportSize(ViewportPx);
	}
	const float DPI = FMath::Max(0.01f, UWidgetLayoutLibrary::GetViewportScale(this));
	const FVector2D Center = ViewportPx * 0.5f;

	// 相机背后判定：投影坐标在背后不可靠，用视线点积。
	FVector CamLoc;
	FRotator CamRot;
	PC->GetPlayerViewPoint(CamLoc, CamRot);
	const bool bBehind = FVector::DotProduct(CamRot.Vector(), (TargetLoc - CamLoc)) < 0.f;

	FVector2D ScreenPx;
	const bool bProjected = UGameplayStatics::ProjectWorldToScreen(
		PC, TargetLoc + FVector(0.f, 0.f, GObjectiveHeadOffset), ScreenPx, false);

	const float MX = ViewportPx.X - GObjectiveEdgeMargin;
	const float MY = ViewportPx.Y - GObjectiveEdgeMargin;
	bool bOnScreen = bProjected && !bBehind &&
		ScreenPx.X >= GObjectiveEdgeMargin && ScreenPx.X <= MX &&
		ScreenPx.Y >= GObjectiveEdgeMargin && ScreenPx.Y <= MY;

	FVector2D FinalPx;
	float ArrowAngle = 0.f;
	if (bOnScreen)
	{
		FinalPx = ScreenPx;
		IconText->SetText(FText::FromString(TEXT("◇")));   // 屏内：菱形浮标
		IconText->SetRenderTransformAngle(0.f);
	}
	else
	{
		// 方向向量：背后时投影镜像失真，取其相对中心的反向。
		FVector2D Dir = ScreenPx - Center;
		if (bBehind)
		{
			Dir = -Dir;
		}
		if (Dir.IsNearlyZero())
		{
			Dir = FVector2D(0.f, 1.f);
		}
		// 缩放到边缘矩形（留白内）。
		const float HalfX = FMath::Max(1.f, ViewportPx.X * 0.5f - GObjectiveEdgeMargin);
		const float HalfY = FMath::Max(1.f, ViewportPx.Y * 0.5f - GObjectiveEdgeMargin);
		const float Scale = FMath::Min(HalfX / FMath::Max(1.f, FMath::Abs(Dir.X)),
			HalfY / FMath::Max(1.f, FMath::Abs(Dir.Y)));
		FinalPx = Center + Dir * Scale;
		ArrowAngle = FMath::RadiansToDegrees(FMath::Atan2(Dir.Y, Dir.X));   // ➤ 默认朝右(东)
		IconText->SetText(FText::FromString(TEXT("➤")));
		IconText->SetRenderTransformAngle(ArrowAngle);
	}

	LabelText->SetText(FText::FromString(FString::Printf(TEXT("%s · %.0f 米"), *Name, Meters)));

	if (UCanvasPanelSlot* CS = Cast<UCanvasPanelSlot>(Container->Slot))
	{
		CS->SetPosition(FinalPx / DPI);   // 像素 → Slate 单位
	}
}
