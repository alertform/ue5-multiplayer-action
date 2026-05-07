#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "MAUserWidget.generated.h"

class UAbilitySystemComponent;
struct FOnAttributeChangeData;

/**
 * Base class for HUD widgets that observe an ASC's attributes.
 * BP children implement the OnXChanged events; widget binds itself to the ASC,
 * keeping HUD-facing API out of Character / PlayerState.
 */
UCLASS(Abstract, BlueprintType)
class MULTIPLAYERACTION_API UMAUserWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** Called from PlayerController once the owning ASC is ready. Idempotent. */
	UFUNCTION(BlueprintCallable, Category = "GAS|HUD")
	void InitFromASC(UAbilitySystemComponent* InASC);

	UFUNCTION(BlueprintPure, Category = "GAS|HUD")
	UAbilitySystemComponent* GetCachedASC() const { return CachedASC.Get(); }

	UFUNCTION(BlueprintPure, Category = "GAS|HUD")
	float GetHealth() const;

	UFUNCTION(BlueprintPure, Category = "GAS|HUD")
	float GetMaxHealth() const;

	UFUNCTION(BlueprintPure, Category = "GAS|HUD")
	float GetHealthPercent() const;

	UFUNCTION(BlueprintPure, Category = "GAS|HUD")
	float GetStamina() const;

	UFUNCTION(BlueprintPure, Category = "GAS|HUD")
	float GetAttackPower() const;

protected:
	/** BP overrides these to push values into ProgressBars / TextBlocks. */
	UFUNCTION(BlueprintImplementableEvent, Category = "GAS|HUD")
	void OnHealthChanged(float NewValue, float OldValue);

	UFUNCTION(BlueprintImplementableEvent, Category = "GAS|HUD")
	void OnMaxHealthChanged(float NewValue, float OldValue);

	UFUNCTION(BlueprintImplementableEvent, Category = "GAS|HUD")
	void OnStaminaChanged(float NewValue, float OldValue);

	UFUNCTION(BlueprintImplementableEvent, Category = "GAS|HUD")
	void OnAttackPowerChanged(float NewValue, float OldValue);

private:
	TWeakObjectPtr<UAbilitySystemComponent> CachedASC;
	bool bBound = false;

	void HandleHealthChange(const FOnAttributeChangeData& Data);
	void HandleMaxHealthChange(const FOnAttributeChangeData& Data);
	void HandleStaminaChange(const FOnAttributeChangeData& Data);
	void HandleAttackPowerChange(const FOnAttributeChangeData& Data);
};
