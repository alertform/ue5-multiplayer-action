#include "UI/MALockOnReticleWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Image.h"
#include "Brushes/SlateColorBrush.h"
#include "GameFramework/PlayerController.h"

// File-unique names — adaptive unity build merges UI .cpps (C2084 lesson).
static const FLinearColor GLockReticleColor(1.0f, 0.22f, 0.18f, 0.95f); // red lock diamond
static const FVector2D GLockReticleSize(26.f, 26.f);

void UMALockOnReticleWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	// Pure overlay — never eats input.
	SetVisibility(ESlateVisibility::HitTestInvisible);

	UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("Root"));
	WidgetTree->RootWidget = Root;

	Marker = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("Marker"));

	// Solid white box (FSlateColorBrush references the engine white texture); tint via
	// ColorAndOpacity so the brush itself stays neutral. Rotated 45° => diamond.
	FSlateColorBrush Brush(FLinearColor::White);
	Brush.ImageSize = GLockReticleSize;
	Marker->SetBrush(Brush);
	Marker->SetColorAndOpacity(GLockReticleColor);
	Marker->SetRenderTransformPivot(FVector2D(0.5f, 0.5f));
	Marker->SetRenderTransformAngle(45.f);

	MarkerSlot = Root->AddChildToCanvas(Marker);
	MarkerSlot->SetAutoSize(true);
	MarkerSlot->SetAlignment(FVector2D(0.5f, 0.5f)); // position == marker center

	Marker->SetVisibility(ESlateVisibility::Collapsed);
}

void UMALockOnReticleWidget::SetTarget(AActor* InTarget)
{
	Target = InTarget;
	if (Marker && !InTarget)
	{
		Marker->SetVisibility(ESlateVisibility::Collapsed);
	}
}

void UMALockOnReticleWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	if (!Marker || !MarkerSlot)
	{
		return;
	}

	AActor* T = Target.Get();
	APlayerController* PC = GetOwningPlayer();
	if (!T || !PC)
	{
		Marker->SetVisibility(ESlateVisibility::Collapsed);
		return;
	}

	const FVector WorldLoc = T->GetActorLocation() + FVector(0.f, 0.f, ZOffset);
	FVector2D ScreenPos;
	// Returns DPI-scaled widget-space position; false when the point is behind the camera.
	if (UWidgetLayoutLibrary::ProjectWorldLocationToWidgetPosition(PC, WorldLoc, ScreenPos, false))
	{
		MarkerSlot->SetPosition(ScreenPos);
		Marker->SetVisibility(ESlateVisibility::HitTestInvisible);
	}
	else
	{
		Marker->SetVisibility(ESlateVisibility::Collapsed);
	}
}
