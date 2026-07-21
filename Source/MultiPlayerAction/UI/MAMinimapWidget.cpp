#include "UI/MAMinimapWidget.h"

#include "AI/MATargetDummy.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/BorderSlot.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Dialogue/MADialogueComponent.h"
#include "EngineUtils.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Styling/CoreStyle.h"
#include "UObject/UObjectIterator.h"
#include "World/MAMapDefinition.h"

// 窗口显示的世界跨度(uu):越小越放大。运行时可调,方便换大场景后找手感。
static TAutoConsoleVariable<float> CVarMinimapViewSpan(
	TEXT("ma.Minimap.ViewSpan"), 3500.f,
	TEXT("Minimap window world span in uu (smaller = more zoomed in)."));

static constexpr float GMinimapWindowPx = 216.f;   // 内容区边长(SizeBox 220 - 边框 2*2)
static constexpr float GMinimapScanInterval = 0.5f;
static const FLinearColor GMinimapEnemyColor(0.9f, 0.15f, 0.1f, 1.f);
static const FLinearColor GMinimapNpcColor(0.35f, 0.85f, 1.f, 1.f);

void UMAMinimapWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	SetVisibility(ESlateVisibility::HitTestInvisible);

	UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("Root"));
	WidgetTree->RootWidget = Root;

	Frame = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("Frame"));
	Frame->SetWidthOverride(220.f);
	Frame->SetHeightOverride(220.f);
	UCanvasPanelSlot* FrameSlot = Root->AddChildToCanvas(Frame);
	FrameSlot->SetAnchors(FAnchors(1.f, 1.f));
	FrameSlot->SetAlignment(FVector2D(1.f, 1.f));
	FrameSlot->SetAutoSize(true);
	FrameSlot->SetPosition(FVector2D(-24.f, -24.f));

	EdgeBorder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("Edge"));
	EdgeBorder->SetBrushColor(FLinearColor(0.f, 0.f, 0.f, 0.55f));
	EdgeBorder->SetPadding(FMargin(2.f));
	Frame->AddChild(EdgeBorder);

	UOverlay* Stack = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("Stack"));
	EdgeBorder->AddChild(Stack);

	ClipPanel = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("ClipPanel"));
	ClipPanel->SetClipping(EWidgetClipping::ClipToBounds);
	if (UOverlaySlot* S = Stack->AddChildToOverlay(ClipPanel))
	{
		S->SetHorizontalAlignment(HAlign_Fill);
		S->SetVerticalAlignment(VAlign_Fill);
	}
	MapImage = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("MapImage"));
	ClipPanel->AddChildToCanvas(MapImage);

	IconCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("IconCanvas"));
	IconCanvas->SetClipping(EWidgetClipping::ClipToBounds);
	if (UOverlaySlot* S = Stack->AddChildToOverlay(IconCanvas))
	{
		S->SetHorizontalAlignment(HAlign_Fill);
		S->SetVerticalAlignment(VAlign_Fill);
	}

	PlayerMarker = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("PlayerMarker"));
	PlayerMarker->SetText(FText::FromString(TEXT("▲")));
	PlayerMarker->SetFont(FCoreStyle::GetDefaultFontStyle("Bold", 13));
	PlayerMarker->SetColorAndOpacity(FSlateColor(FLinearColor::White));
	PlayerMarker->SetShadowOffset(FVector2D(1.f, 1.f));
	PlayerMarker->SetShadowColorAndOpacity(FLinearColor(0.f, 0.f, 0.f, 0.9f));
	if (UOverlaySlot* S = Stack->AddChildToOverlay(PlayerMarker))
	{
		S->SetHorizontalAlignment(HAlign_Center);
		S->SetVerticalAlignment(VAlign_Center);
	}

	// 地图定义就位前整个收起(收 Frame 不收根 —— Collapsed 根不 tick)。
	Frame->SetVisibility(ESlateVisibility::Collapsed);
}

void UMAMinimapWidget::RefreshIconSources()
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

UImage* UMAMinimapWidget::AcquireIcon(int32 Index, const FLinearColor& Color)
{
	while (!IconPool.IsValidIndex(Index))
	{
		UImage* Icon = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
		IconCanvas->AddChildToCanvas(Icon);
		IconPool.Add(Icon);
	}
	UImage* Icon = IconPool[Index];
	Icon->SetVisibility(ESlateVisibility::HitTestInvisible);
	Icon->SetColorAndOpacity(Color);
	return Icon;
}

void UMAMinimapWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	SourceScanCooldown -= InDeltaTime;
	if (SourceScanCooldown <= 0.f)
	{
		SourceScanCooldown = GMinimapScanInterval;
		RefreshIconSources();
		if (!MapDef.IsValid())
		{
			for (TActorIterator<AMAMapDefinition> It(GetWorld()); It; ++It)
			{
				MapDef = *It;
				break;
			}
			if (MapDef.IsValid() && MapDef->GetMinimapTexture())
			{
				if (UMaterialInterface* Mat = MapDef->GetMinimapMaterial())
				{
					// 材质驱动：UV 窗口/旋转/圆形遮罩在材质里，widget 只喂参数。
					MapMID = UMaterialInstanceDynamic::Create(Mat, this);
					MapMID->SetTextureParameterValue(TEXT("MapTexture"), MapDef->GetMinimapTexture());
					MapImage->SetBrushFromMaterial(MapMID);
					if (UCanvasPanelSlot* S = Cast<UCanvasPanelSlot>(MapImage->Slot))
					{
						S->SetPosition(FVector2D::ZeroVector);
						S->SetSize(FVector2D(GMinimapWindowPx, GMinimapWindowPx));
					}
					MapImage->SetRenderTransformAngle(0.f);
					EdgeBorder->SetBrushColor(FLinearColor::Transparent);   // 圆环框由材质画
				}
				else
				{
					MapImage->SetBrushFromTexture(MapDef->GetMinimapTexture());
				}
			}
		}
	}

	const APawn* Pawn = GetOwningPlayerPawn();
	if (!MapDef.IsValid() || !MapDef->GetMinimapTexture() || !Pawn)
	{
		Frame->SetVisibility(ESlateVisibility::Collapsed);
		return;
	}
	Frame->SetVisibility(ESlateVisibility::HitTestInvisible);

	const float ViewSpan = FMath::Max(200.f, CVarMinimapViewSpan.GetValueOnGameThread());
	const float WorldSpan = MapDef->GetWorldSpan();
	const float DrawnPx = GMinimapWindowPx * WorldSpan / ViewSpan;
	const FVector PawnLoc = Pawn->GetActorLocation();
	const FVector2D PlayerUV = MapDef->WorldToMapUV(PawnLoc);
	const float Yaw = GetOwningPlayer() ? static_cast<float>(GetOwningPlayer()->GetControlRotation().Yaw) : 0.f;
	const float Half = GMinimapWindowPx * 0.5f;

	if (MapMID)
	{
		// 材质驱动:采样端旋转与显示端方向相反,角度取 +Yaw(与图标层的 -Yaw 互补)。
		MapMID->SetScalarParameterValue(TEXT("CenterU"), PlayerUV.X);
		MapMID->SetScalarParameterValue(TEXT("CenterV"), PlayerUV.Y);
		MapMID->SetScalarParameterValue(TEXT("ViewScale"), ViewSpan / WorldSpan);
		MapMID->SetScalarParameterValue(TEXT("Angle"), FMath::DegreesToRadians(Yaw));
	}
	else
	{
		// 方形回退:玩家 UV 点平移到窗口中心,再绕该点旋转(玩家朝向恒朝上)。
		if (UCanvasPanelSlot* S = Cast<UCanvasPanelSlot>(MapImage->Slot))
		{
			S->SetSize(FVector2D(DrawnPx, DrawnPx));
			S->SetPosition(FVector2D(Half - PlayerUV.X * DrawnPx, Half - PlayerUV.Y * DrawnPx));
		}
		MapImage->SetRenderTransformPivot(PlayerUV);
		MapImage->SetRenderTransformAngle(-Yaw);
	}

	// 图标:世界偏移 → 屏幕偏移(上=+X 约定),再转同一角度。
	const float K = GMinimapWindowPx / ViewSpan;
	const float A = FMath::DegreesToRadians(-Yaw);
	const float CosA = FMath::Cos(A);
	const float SinA = FMath::Sin(A);
	auto ToWindow = [&](const FVector& WorldPos) -> FVector2D
	{
		const float Sx = static_cast<float>(WorldPos.Y - PawnLoc.Y) * K;
		const float Sy = static_cast<float>(-(WorldPos.X - PawnLoc.X)) * K;
		return FVector2D(CosA * Sx - SinA * Sy, SinA * Sx + CosA * Sy);
	};

	const bool bRound = MapMID != nullptr;   // 圆形遮罩下图标按半径裁剪/钳制
	int32 IconIndex = 0;
	for (const TWeakObjectPtr<AActor>& Enemy : EnemySources)
	{
		if (!Enemy.IsValid() || Enemy->IsHidden())
		{
			continue;
		}
		const FVector2D P = ToWindow(Enemy->GetActorLocation());
		const bool bOutside = bRound
			? P.Size() > Half - 10.f
			: (FMath::Abs(P.X) > Half - 4.f || FMath::Abs(P.Y) > Half - 4.f);
		if (bOutside)
		{
			continue;   // 敌人出窗即不画(方向指示只留给 NPC)
		}
		UImage* Icon = AcquireIcon(IconIndex++, GMinimapEnemyColor);
		if (UCanvasPanelSlot* S = Cast<UCanvasPanelSlot>(Icon->Slot))
		{
			S->SetSize(FVector2D(7.f, 7.f));
			S->SetPosition(FVector2D(Half + P.X - 3.5f, Half + P.Y - 3.5f));
		}
	}
	for (const TWeakObjectPtr<AActor>& Npc : NpcSources)
	{
		if (!Npc.IsValid() || Npc->IsHidden())
		{
			continue;   // 剑客离场(接单期间)时地图上也消失,与世界表现一致
		}
		FVector2D P = ToWindow(Npc->GetActorLocation());
		const float Limit = bRound ? Half - 14.f : Half - 8.f;
		bool bClamped;
		if (bRound)
		{
			bClamped = P.Size() > Limit;
			if (bClamped && P.Size() > KINDA_SMALL_NUMBER)
			{
				P *= Limit / P.Size();   // 沿方向钳到圆环内侧
			}
		}
		else
		{
			bClamped = FMath::Abs(P.X) > Limit || FMath::Abs(P.Y) > Limit;
			if (bClamped)
			{
				P.X = FMath::Clamp(P.X, -Limit, Limit);
				P.Y = FMath::Clamp(P.Y, -Limit, Limit);
			}
		}
		FLinearColor Color = GMinimapNpcColor;
		Color.A = bClamped ? 0.7f : 1.f;
		UImage* Icon = AcquireIcon(IconIndex++, Color);
		if (UCanvasPanelSlot* S = Cast<UCanvasPanelSlot>(Icon->Slot))
		{
			S->SetSize(FVector2D(9.f, 9.f));
			S->SetPosition(FVector2D(Half + P.X - 4.5f, Half + P.Y - 4.5f));
		}
	}
	for (int32 i = IconIndex; i < IconPool.Num(); ++i)
	{
		IconPool[i]->SetVisibility(ESlateVisibility::Collapsed);
	}
}
