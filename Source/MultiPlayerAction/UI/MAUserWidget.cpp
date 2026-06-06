#include "UI/MAUserWidget.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystem/MAAttributeSet.h"
#include "Blueprint/WidgetTree.h"
#include "UI/MAHealthBarWidget.h"
#include "UI/MASkillSlotWidget.h"

void UMAUserWidget::InitFromASC(UAbilitySystemComponent* InASC)
{
	if (bBound || !InASC)
	{
		return;
	}
	bBound = true;
	CachedASC = InASC;

	InASC->GetGameplayAttributeValueChangeDelegate(UMAAttributeSet::GetHealthAttribute())
		.AddUObject(this, &UMAUserWidget::HandleHealthChange);
	InASC->GetGameplayAttributeValueChangeDelegate(UMAAttributeSet::GetMaxHealthAttribute())
		.AddUObject(this, &UMAUserWidget::HandleMaxHealthChange);
	InASC->GetGameplayAttributeValueChangeDelegate(UMAAttributeSet::GetStaminaAttribute())
		.AddUObject(this, &UMAUserWidget::HandleStaminaChange);
	InASC->GetGameplayAttributeValueChangeDelegate(UMAAttributeSet::GetAttackPowerAttribute())
		.AddUObject(this, &UMAUserWidget::HandleAttackPowerChange);

	// Seed BP with current values so bars start filled correctly
	const float Health = InASC->GetNumericAttribute(UMAAttributeSet::GetHealthAttribute());
	const float MaxHealth = InASC->GetNumericAttribute(UMAAttributeSet::GetMaxHealthAttribute());
	const float Stamina = InASC->GetNumericAttribute(UMAAttributeSet::GetStaminaAttribute());
	const float AttackPower = InASC->GetNumericAttribute(UMAAttributeSet::GetAttackPowerAttribute());

	OnMaxHealthChanged(MaxHealth, MaxHealth);
	OnHealthChanged(Health, Health);
	OnStaminaChanged(Stamina, Stamina);
	OnAttackPowerChanged(AttackPower, AttackPower);

	// Auto-wire any skill slots / health bars placed in the BP child's tree — WBP_HUD just
	// lays them out; the sub-widgets bind themselves.
	if (WidgetTree)
	{
		WidgetTree->ForEachWidget([InASC](UWidget* W)
		{
			if (UMASkillSlotWidget* SkillSlot = Cast<UMASkillSlotWidget>(W))
			{
				SkillSlot->InitSlot(InASC);
			}
			else if (UMAHealthBarWidget* HealthBar = Cast<UMAHealthBarWidget>(W))
			{
				HealthBar->InitHealthBar(InASC);
			}
		});
	}
}

void UMAUserWidget::HandleHealthChange(const FOnAttributeChangeData& Data)
{
	OnHealthChanged(Data.NewValue, Data.OldValue);
}

void UMAUserWidget::HandleMaxHealthChange(const FOnAttributeChangeData& Data)
{
	OnMaxHealthChanged(Data.NewValue, Data.OldValue);
}

void UMAUserWidget::HandleStaminaChange(const FOnAttributeChangeData& Data)
{
	OnStaminaChanged(Data.NewValue, Data.OldValue);
}

void UMAUserWidget::HandleAttackPowerChange(const FOnAttributeChangeData& Data)
{
	OnAttackPowerChanged(Data.NewValue, Data.OldValue);
}

float UMAUserWidget::GetHealth() const
{
	return CachedASC.IsValid() ? CachedASC->GetNumericAttribute(UMAAttributeSet::GetHealthAttribute()) : 0.f;
}

float UMAUserWidget::GetMaxHealth() const
{
	return CachedASC.IsValid() ? CachedASC->GetNumericAttribute(UMAAttributeSet::GetMaxHealthAttribute()) : 0.f;
}

float UMAUserWidget::GetHealthPercent() const
{
	if (!CachedASC.IsValid()) return 0.f;
	const float Max = CachedASC->GetNumericAttribute(UMAAttributeSet::GetMaxHealthAttribute());
	return Max > 0.f ? CachedASC->GetNumericAttribute(UMAAttributeSet::GetHealthAttribute()) / Max : 0.f;
}

float UMAUserWidget::GetStamina() const
{
	return CachedASC.IsValid() ? CachedASC->GetNumericAttribute(UMAAttributeSet::GetStaminaAttribute()) : 0.f;
}

float UMAUserWidget::GetAttackPower() const
{
	return CachedASC.IsValid() ? CachedASC->GetNumericAttribute(UMAAttributeSet::GetAttackPowerAttribute()) : 0.f;
}
