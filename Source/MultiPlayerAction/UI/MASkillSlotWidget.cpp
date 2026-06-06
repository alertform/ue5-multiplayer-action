#include "UI/MASkillSlotWidget.h"
#include "AbilitySystemComponent.h"
#include "Components/Border.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"

// Borderless slots: idle is fully transparent (no per-ability frame); the rounded SlotBorder
// only appears as a soft amber pill behind the labels while the ability is active.
const FLinearColor UMASkillSlotWidget::IdleColor(0.f, 0.f, 0.f, 0.f);
const FLinearColor UMASkillSlotWidget::ActiveColor(1.0f, 0.78f, 0.35f, 0.55f);

void UMASkillSlotWidget::InitSlot(UAbilitySystemComponent* InASC)
{
	ASC = InASC;
	bWasCoolingDown = false; // reset edge state when (re)bound
}

void UMASkillSlotWidget::NativePreConstruct()
{
	Super::NativePreConstruct();

	// Push design-time values so the slot previews correctly in the UMG editor.
	if (HotkeyLabel)
	{
		HotkeyLabel->SetText(HotkeyText);
	}
	if (AbilityLabel)
	{
		AbilityLabel->SetText(AbilityNameText);
	}
	if (CooldownOverlay)
	{
		CooldownOverlay->SetPercent(0.f);
	}
	if (SlotBorder)
	{
		SlotBorder->SetBrushColor(IdleColor);
	}
}

void UMASkillSlotWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	UAbilitySystemComponent* Comp = ASC.Get();
	if (!Comp)
	{
		return;
	}

	if (CooldownOverlay && CooldownTag.IsValid())
	{
		// Max remaining/duration across all active GEs granting the cooldown tag (normally one).
		float Percent = 0.f;
		const FGameplayEffectQuery Query =
			FGameplayEffectQuery::MakeQuery_MatchAnyOwningTags(FGameplayTagContainer(CooldownTag));
		const TArray<TPair<float, float>> Times = Comp->GetActiveEffectsTimeRemainingAndDuration(Query);
		for (const TPair<float, float>& T : Times)
		{
			if (T.Value > 0.f)
			{
				Percent = FMath::Max(Percent, T.Key / T.Value);
			}
		}

		CooldownOverlay->SetPercent(Percent);

		const bool bCooling = Percent > 0.f;
		if (bWasCoolingDown && !bCooling)
		{
			OnReady();
		}
		bWasCoolingDown = bCooling;
	}

	if (SlotBorder && ActiveTag.IsValid())
	{
		SlotBorder->SetBrushColor(Comp->HasMatchingGameplayTag(ActiveTag) ? ActiveColor : IdleColor);
	}
}
