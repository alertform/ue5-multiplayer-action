#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "MAHealthBarWidget.generated.h"

class UAbilitySystemComponent;
class UProgressBar;
struct FOnAttributeChangeData;

/**
 * Street-Fighter-style health bar: the front fill drops instantly on damage while a
 * "chip" fill behind it holds the pre-damage value. Hits landing within ChipHoldSeconds
 * keep accumulating into the same chip segment (the hold timer resets per hit); once the
 * window expires the chip drains down to the real health.
 *
 * Self-contained View: UMAUserWidget::InitFromASC auto-wires any instance found in its
 * tree (same pattern as UMASkillSlotWidget) — lay it out in the WBP and forget about it.
 * Percent math uses the real MaxHealth attribute (no hardcoded denominators).
 */
UCLASS(Abstract)
class MULTIPLAYERACTION_API UMAHealthBarWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** Bind to the ASC's Health/MaxHealth. Idempotent — safe on respawn re-init. */
	void InitHealthBar(UAbilitySystemComponent* InASC);

	/** Overhead-bar mode: stay collapsed until the first damage, re-hide when back at full.
	 *  Call BEFORE InitHealthBar (e.g. AMATargetDummy::BeginPlay). */
	void SetHideUntilDamaged(bool bInHide) { bHideUntilDamaged = bInHide; }

protected:
	virtual void NativePreConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	/** Front bar — real health, updates instantly. */
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UProgressBar> HealthFill;

	/** Back bar — recent-damage chip; holds, then drains. Place BEHIND HealthFill in an Overlay. */
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UProgressBar> ChipFill;

	/** Seconds the chip holds after the LAST hit before draining (each hit resets the timer) */
	UPROPERTY(EditAnywhere, Category = "Chip", meta = (ClampMin = "0.0", UIMax = "10.0"))
	float ChipHoldSeconds = 3.0f;

	/** Chip drain rate in bar-fraction per second once the hold expires */
	UPROPERTY(EditAnywhere, Category = "Chip", meta = (ClampMin = "0.01", UIMax = "5.0"))
	float ChipDrainPerSecond = 0.6f;

	/** Overhead-bar death: the chip skips the hold and drains at this rate; at zero the bar
	 *  collapses instantly. Only applies in bHideUntilDamaged mode (enemy overhead bars). */
	UPROPERTY(EditAnywhere, Category = "Chip", meta = (ClampMin = "0.1", UIMax = "10.0"))
	float DeathDrainPerSecond = 2.5f;

	// --- Style (built in C++ at PreConstruct so designer-time template state can never
	// --- leak a default Slate look into runtime instances) ---

	/** Front fill — current health */
	UPROPERTY(EditAnywhere, Category = "Style")
	FLinearColor HealthColor = FLinearColor(0.85f, 0.07f, 0.06f, 1.0f);

	/** Chip fill — recent damage */
	UPROPERTY(EditAnywhere, Category = "Style")
	FLinearColor ChipColor = FLinearColor(0.35f, 0.03f, 0.03f, 0.95f);

	/** Bar track behind everything (warm near-black) */
	UPROPERTY(EditAnywhere, Category = "Style")
	FLinearColor TrackColor = FLinearColor(0.02f, 0.016f, 0.012f, 0.85f);

	/** Rounded-box outline accent */
	UPROPERTY(EditAnywhere, Category = "Style")
	FLinearColor OutlineColor = FLinearColor(1.0f, 0.78f, 0.35f, 0.45f);

	UPROPERTY(EditAnywhere, Category = "Style", meta = (ClampMin = "0.0", UIMax = "20.0"))
	float CornerRadius = 9.0f;

	UPROPERTY(EditAnywhere, Category = "Style", meta = (ClampMin = "0.0", UIMax = "4.0"))
	float OutlineWidth = 1.5f;

	/** Overhead-bar mode (see SetHideUntilDamaged) */
	UPROPERTY(EditAnywhere, Category = "Style")
	bool bHideUntilDamaged = false;

private:
	TWeakObjectPtr<UAbilitySystemComponent> CachedASC;
	bool bBound = false;

	float HealthPercent = 1.f;
	float ChipPercent = 1.f;
	double LastDamageTime = -1e9;

	/** Death sequence armed (overhead mode): fast-drain the chip, then collapse the bar. */
	bool bDeathDrain = false;

	void HandleHealthChange(const FOnAttributeChangeData& Data);
	void HandleMaxHealthChange(const FOnAttributeChangeData& Data);

	/** Recompute both fills from absolute values; damage arms the chip, heal lifts it. */
	void ApplyHealth(float NewHealth, float OldHealth);
	float GetMaxHealthSafe() const;
	void PushToBars() const;

	/** Force-build the rounded-box styles on both ProgressBars from the color properties. */
	void ApplyBarStyles();
};
