#include "UI/MAHealthBarWidget.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystem/MAAttributeSet.h"
#include "Components/ProgressBar.h"

void UMAHealthBarWidget::InitHealthBar(UAbilitySystemComponent* InASC)
{
	if (bBound || !InASC)
	{
		return;
	}
	bBound = true;
	CachedASC = InASC;

	InASC->GetGameplayAttributeValueChangeDelegate(UMAAttributeSet::GetHealthAttribute())
		.AddUObject(this, &UMAHealthBarWidget::HandleHealthChange);
	InASC->GetGameplayAttributeValueChangeDelegate(UMAAttributeSet::GetMaxHealthAttribute())
		.AddUObject(this, &UMAHealthBarWidget::HandleMaxHealthChange);

	// Seed: start both fills at the current value, chip disarmed.
	const float Health = InASC->GetNumericAttribute(UMAAttributeSet::GetHealthAttribute());
	const float Max = GetMaxHealthSafe();
	HealthPercent = Max > 0.f ? Health / Max : 0.f;
	ChipPercent = HealthPercent;
	LastDamageTime = -1e9;
	PushToBars();
}

void UMAHealthBarWidget::HandleHealthChange(const FOnAttributeChangeData& Data)
{
	ApplyHealth(Data.NewValue, Data.OldValue);
}

void UMAHealthBarWidget::HandleMaxHealthChange(const FOnAttributeChangeData& Data)
{
	// Re-derive the front fill from absolutes; never let the chip sit below it.
	if (CachedASC.IsValid() && Data.NewValue > 0.f)
	{
		HealthPercent = CachedASC->GetNumericAttribute(UMAAttributeSet::GetHealthAttribute()) / Data.NewValue;
		ChipPercent = FMath::Max(ChipPercent, HealthPercent);
		PushToBars();
	}
}

void UMAHealthBarWidget::ApplyHealth(float NewHealth, float OldHealth)
{
	const float Max = GetMaxHealthSafe();
	const float OldPercent = HealthPercent;
	HealthPercent = Max > 0.f ? FMath::Clamp(NewHealth / Max, 0.f, 1.f) : 0.f;

	if (NewHealth < OldHealth)
	{
		// Damage: chip holds the highest recent value; the hold window restarts per hit,
		// so hits inside ChipHoldSeconds accumulate into one growing chip segment.
		ChipPercent = FMath::Max(ChipPercent, OldPercent);
		LastDamageTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
	}
	else
	{
		// Heal/respawn: the chip never trails below the real value.
		ChipPercent = FMath::Max(ChipPercent, HealthPercent);
	}
	PushToBars();
}

void UMAHealthBarWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	if (!bBound || ChipPercent <= HealthPercent)
	{
		return;
	}

	const double Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
	if (Now - LastDamageTime >= ChipHoldSeconds)
	{
		ChipPercent = FMath::Max(HealthPercent, ChipPercent - ChipDrainPerSecond * InDeltaTime);
		PushToBars();
	}
}

float UMAHealthBarWidget::GetMaxHealthSafe() const
{
	return CachedASC.IsValid()
		? CachedASC->GetNumericAttribute(UMAAttributeSet::GetMaxHealthAttribute())
		: 0.f;
}

void UMAHealthBarWidget::PushToBars() const
{
	if (HealthFill)
	{
		HealthFill->SetPercent(HealthPercent);
	}
	if (ChipFill)
	{
		ChipFill->SetPercent(ChipPercent);
	}
}
