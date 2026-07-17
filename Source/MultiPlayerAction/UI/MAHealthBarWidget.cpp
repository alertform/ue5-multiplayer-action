#include "UI/MAHealthBarWidget.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystem/MAAttributeSet.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/ProgressBar.h"

namespace
{
	FSlateBrush MakeRoundedBrush(const FLinearColor& Tint, float Radius,
		const FLinearColor& OutlineColor, float OutlineWidth)
	{
		FSlateBrush Brush;
		Brush.DrawAs = ESlateBrushDrawType::RoundedBox;
		Brush.TintColor = Tint;
		Brush.OutlineSettings = FSlateBrushOutlineSettings(
			FVector4(Radius, Radius, Radius, Radius), OutlineColor, OutlineWidth);
		Brush.OutlineSettings.RoundingType = ESlateBrushRoundingType::FixedRadius;
		return Brush;
	}

	FSlateBrush MakeNoDrawBrush()
	{
		FSlateBrush Brush;
		Brush.DrawAs = ESlateBrushDrawType::NoDrawType;
		Brush.TintColor = FLinearColor::Transparent;
		return Brush;
	}
}

void UMAHealthBarWidget::NativePreConstruct()
{
	Super::NativePreConstruct();
	ApplyBarStyles();
	EnsureOutlineLayer();
}

void UMAHealthBarWidget::ApplyBarStyles()
{
	// Built in code on every (pre)construct: designer previews and runtime instances are
	// guaranteed identical regardless of what style state the widget templates carry.
	if (ChipFill)
	{
		FProgressBarStyle Style = ChipFill->GetWidgetStyle();
		// 轨道不再自带描边 —— 描边由 EnsureOutlineLayer 的顶层 UImage 独立绘制，
		// 否则填充条会把画在同一矩形边缘的描边盖掉（"内条比外框宽"）。
		Style.BackgroundImage = MakeRoundedBrush(TrackColor, CornerRadius, FLinearColor::Transparent, 0.f);
		Style.FillImage = MakeRoundedBrush(ChipColor, CornerRadius, FLinearColor::Transparent, 0.f);
		ChipFill->SetWidgetStyle(Style);
		// UProgressBar's CDO defaults FillColorAndOpacity to BLUE (0, 0.5, 1) and MULTIPLIES
		// it over the fill brush — neutralize it or every brush color ships tinted teal.
		ChipFill->SetFillColorAndOpacity(FLinearColor::White);
	}
	if (HealthFill)
	{
		// Transparent track — the chip bar's track shows through from behind.
		FProgressBarStyle Style = HealthFill->GetWidgetStyle();
		Style.BackgroundImage = MakeNoDrawBrush();
		Style.FillImage = MakeRoundedBrush(HealthColor, CornerRadius, FLinearColor::Transparent, 0.f);
		HealthFill->SetWidgetStyle(Style);
		HealthFill->SetFillColorAndOpacity(FLinearColor::White);
	}
}

void UMAHealthBarWidget::EnsureOutlineLayer()
{
	if (OutlineImage || !ChipFill)
	{
		return;
	}
	UOverlay* ParentOverlay = Cast<UOverlay>(ChipFill->GetParent());
	if (!ParentOverlay || !WidgetTree)
	{
		return; // WBP 没按 Overlay 摆放就不加层（保持旧观感，不崩）
	}

	OutlineImage = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("BarOutline"));
	// 体色全透明、只画描边的圆角盒 —— 叠在两条填充之上，框永远完整。
	OutlineImage->SetBrush(MakeRoundedBrush(FLinearColor::Transparent, CornerRadius, OutlineColor, OutlineWidth));
	OutlineImage->SetVisibility(ESlateVisibility::HitTestInvisible);
	if (UOverlaySlot* OutlineSlot = ParentOverlay->AddChildToOverlay(OutlineImage))
	{
		OutlineSlot->SetHorizontalAlignment(HAlign_Fill);
		OutlineSlot->SetVerticalAlignment(VAlign_Fill);
	}
}

void UMAHealthBarWidget::InitHealthBar(UAbilitySystemComponent* InASC)
{
	if (bBound || !InASC)
	{
		return;
	}
	bBound = true;
	CachedASC = InASC;

	// Overhead-bar mode: invisible until something actually happens to this character.
	if (bHideUntilDamaged)
	{
		SetVisibility(ESlateVisibility::Collapsed);
	}

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

		// Overhead-bar mode: first blood reveals the bar.
		if (bHideUntilDamaged)
		{
			SetVisibility(ESlateVisibility::SelfHitTestInvisible);

			// Death: skip the chip hold entirely — fast-drain to zero, then vanish
			// (handled in NativeTick). A corpse should not wear a lingering bar.
			if (NewHealth <= 0.f)
			{
				bDeathDrain = true;
			}
		}
	}
	else
	{
		// Heal/respawn: the chip never trails below the real value.
		ChipPercent = FMath::Max(ChipPercent, HealthPercent);
		bDeathDrain = false; // respawn mid-drain cancels the death sequence

		// Overhead-bar mode: back at full (dummy respawn) -> tuck the bar away again.
		if (bHideUntilDamaged && HealthPercent >= 1.f - KINDA_SMALL_NUMBER)
		{
			ChipPercent = HealthPercent;
			SetVisibility(ESlateVisibility::Collapsed);
		}
	}
	PushToBars();
}

void UMAHealthBarWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	if (!bBound)
	{
		return;
	}

	// Overhead-bar death sequence: no hold — drain straight to zero, then collapse instantly.
	if (bDeathDrain)
	{
		ChipPercent = FMath::Max(0.f, ChipPercent - DeathDrainPerSecond * InDeltaTime);
		PushToBars();
		if (ChipPercent <= 0.f)
		{
			bDeathDrain = false;
			SetVisibility(ESlateVisibility::Collapsed);
		}
		return;
	}

	if (ChipPercent <= HealthPercent)
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
