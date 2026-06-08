#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "MALockOnReticleWidget.generated.h"

class UImage;
class UCanvasPanelSlot;

/**
 * Lock-on marker. Code-built like the match widgets — a single diamond drawn over the
 * locked target, repositioned every frame by projecting the target's world location to
 * the viewport. Driven entirely by UMALockOnComponent via SetTarget(); collapses itself
 * when the target is null or off-screen/behind the camera.
 */
UCLASS()
class MULTIPLAYERACTION_API UMALockOnReticleWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** null hides the marker; a valid actor makes it track that actor each tick. */
	void SetTarget(AActor* InTarget);

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
	UPROPERTY()
	TObjectPtr<UImage> Marker;

	UPROPERTY()
	TObjectPtr<UCanvasPanelSlot> MarkerSlot;

	TWeakObjectPtr<AActor> Target;

	/** Vertical lift from actor origin (capsule center) toward the upper torso. */
	float ZOffset = 40.f;
};
