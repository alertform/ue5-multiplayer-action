#include "UI/MALockOnReticleWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Image.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "GameFramework/PlayerController.h"

// File-unique names — adaptive unity build merges UI .cpps (C2084 lesson).
// Elden Ring–style lock marker: a small soft white dot.
static const FVector2D GLockReticleSize(14.f, 14.f);
static const FLinearColor GLockReticleFill(1.0f, 1.0f, 1.0f, 0.95f);
static const FLinearColor GLockReticleOutline(0.0f, 0.0f, 0.0f, 0.45f); // faint rim for contrast

void UMALockOnReticleWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	// Pure overlay — never eats input.
	SetVisibility(ESlateVisibility::HitTestInvisible);

	UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("Root"));
	WidgetTree->RootWidget = Root;

	Marker = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("Marker"));

	// White filled circle (rounded-box radius = half size) with a faint dark rim so it
	// reads on bright targets — the Elden Ring lock-on dot.
	FSlateRoundedBoxBrush Dot(GLockReticleFill, GLockReticleSize.X * 0.5f,
		GLockReticleOutline, 1.0f, GLockReticleSize);
	Marker->SetBrush(Dot);

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
