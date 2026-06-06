#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "GameplayTagContainer.h"
#include "MASkillSlotWidget.generated.h"

class UAbilitySystemComponent;
class UProgressBar;
class UTextBlock;
class UBorder;

/**
 * One keycap-style skill slot. All logic lives in NativeTick (cooldown sweep from the ASC's
 * active cooldown GE, active-state highlight from a gameplay tag); the BP child is pure layout.
 * Slots are auto-wired by UMAUserWidget::InitFromASC scanning its widget tree — place instances
 * in WBP_HUD and configure CooldownTag/ActiveTag/HotkeyText per instance, nothing else.
 */
UCLASS(Abstract)
class MULTIPLAYERACTION_API UMASkillSlotWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** Called by the owning HUD widget once the ASC is ready. */
	void InitSlot(UAbilitySystemComponent* InASC);

protected:
	virtual void NativePreConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	/** Cooldown tag granted by this ability's cooldown GE; unset = no sweep (e.g. Sprint). */
	UPROPERTY(EditAnywhere, Category = "SkillSlot")
	FGameplayTag CooldownTag;

	/** Tag owned while the ability is active (Sprint/Dodge state); unset = no highlight. */
	UPROPERTY(EditAnywhere, Category = "SkillSlot")
	FGameplayTag ActiveTag;

	/** Keycap label, e.g. "LMB", "Q". */
	UPROPERTY(EditAnywhere, Category = "SkillSlot")
	FText HotkeyText;

	/** Ability display name shown above the hotkey, e.g. "Attack". */
	UPROPERTY(EditAnywhere, Category = "SkillSlot")
	FText AbilityNameText;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> HotkeyLabel;

	/** Top line of the label stack; optional so older layouts without it still bind. */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> AbilityLabel;

	/** Bottom-to-top fill; percent = remaining/duration so the mask recedes as the CD runs out. */
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UProgressBar> CooldownOverlay;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UBorder> SlotBorder;

	/** Fired on the cooldown's falling edge (>0 → 0); BP child may play a ready flash. */
	UFUNCTION(BlueprintImplementableEvent, Category = "SkillSlot")
	void OnReady();

private:
	TWeakObjectPtr<UAbilitySystemComponent> ASC;
	bool bWasCoolingDown = false;

	static const FLinearColor IdleColor;
	static const FLinearColor ActiveColor;
};
