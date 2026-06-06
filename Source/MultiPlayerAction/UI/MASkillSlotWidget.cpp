#include "UI/MASkillSlotWidget.h"
#include "AbilitySystemComponent.h"
#include "Components/Border.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"

const FLinearColor UMASkillSlotWidget::IdleColor(0.02f, 0.02f, 0.025f, 0.85f);
const FLinearColor UMASkillSlotWidget::ActiveColor(1.0f, 0.78f, 0.35f, 0.95f);

void UMASkillSlotWidget::InitSlot(UAbilitySystemComponent* InASC)
{
	ASC = InASC;
}

void UMASkillSlotWidget::NativePreConstruct()
{
	Super::NativePreConstruct();

	// Push design-time values so the slot previews correctly in the UMG editor.
	if (HotkeyLabel)
	{
		HotkeyLabel->SetText(HotkeyText);
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
