#include "UI/MALockOnReticleWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/Image.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "GameFramework/PlayerController.h"

// File-unique names — adaptive unity build merges UI .cpps (C2084 lesson).
// Elden Ring–style lock marker: a small white dot with a soft, blurred edge. Slate has no
// edge-blur on a brush, so we fake a Gaussian falloff by stacking concentric white circles —
// big+faint at the back, small+bright at the front. Their overlapping translucency composites
// into a bright core that fades out smoothly, no texture asset required.
namespace
{
	struct FLockRing { float Size; float Alpha; };
	static const FLockRing GLockRings[] = {
		{ 22.f, 0.04f },
		{ 18.f, 0.07f },
		{ 14.5f, 0.12f },
		{ 11.5f, 0.20f },
		{ 9.0f, 0.38f },
		{ 6.5f, 0.95f }, // bright core
	};
}

void UMALockOnReticleWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	// Pure overlay — never eats input.
	SetVisibility(ESlateVisibility::HitTestInvisible);

	UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("Root"));
	WidgetTree->RootWidget = Root;

	Glow = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("Glow"));

	for (const FLockRing& Ring : GLockRings)
	{
		UImage* Layer = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
		// Rounded-box radius = half the size => a perfect circle at native size.
		FSlateRoundedBoxBrush Circle(FLinearColor(1.f, 1.f, 1.f, Ring.Alpha), Ring.Size * 0.5f,
			FVector2D(Ring.Size, Ring.Size));
		Layer->SetBrush(Circle);

		if (UOverlaySlot* LayerSlot = Glow->AddChildToOverlay(Layer))
		{
			// Center-align so every ring shares the same center (concentric) at its native size.
			LayerSlot->SetHorizontalAlignment(HAlign_Center);
			LayerSlot->SetVerticalAlignment(VAlign_Center);
		}
	}

	GlowSlot = Root->AddChildToCanvas(Glow);
	GlowSlot->SetAutoSize(true);
	GlowSlot->SetAlignment(FVector2D(0.5f, 0.5f)); // position == dot center

	Glow->SetVisibility(ESlateVisibility::Collapsed);
}

void UMALockOnReticleWidget::SetTarget(AActor* InTarget)
{
	Target = InTarget;
	if (Glow && !InTarget)
	{
		Glow->SetVisibility(ESlateVisibility::Collapsed);
	}
}

void UMALockOnReticleWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	if (!Glow || !GlowSlot)
	{
		return;
	}

	AActor* T = Target.Get();
	APlayerController* PC = GetOwningPlayer();
	if (!T || !PC)
	{
		Glow->SetVisibility(ESlateVisibility::Collapsed);
		return;
	}

	const FVector WorldLoc = T->GetActorLocation() + FVector(0.f, 0.f, ZOffset);
	FVector2D ScreenPos;
	// Returns DPI-scaled widget-space position; false when the point is behind the camera.
	if (UWidgetLayoutLibrary::ProjectWorldLocationToWidgetPosition(PC, WorldLoc, ScreenPos, false))
	{
		GlowSlot->SetPosition(ScreenPos);
		Glow->SetVisibility(ESlateVisibility::HitTestInvisible);
	}
	else
	{
		Glow->SetVisibility(ESlateVisibility::Collapsed);
	}
}
