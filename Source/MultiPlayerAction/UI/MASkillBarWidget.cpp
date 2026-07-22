#include "UI/MASkillBarWidget.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystem/MAGameplayTags.h"
#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Player/MAPlayerState.h"
#include "Styling/CoreStyle.h"

static constexpr float GSkillSlotPx = 60.f;
static const FLinearColor GSkillIdleBg(0.02f, 0.02f, 0.03f, 0.72f);
static const FLinearColor GSkillActiveBg(0.24f, 0.17f, 0.05f, 0.85f);
static const FLinearColor GSkillIdleOutline(0.45f, 0.45f, 0.5f, 0.8f);
static const FLinearColor GSkillActiveOutline(1.f, 0.78f, 0.35f, 1.f);   // HUD 金色系

namespace
{
	FSlateBrush MakeCircleBrush(const FLinearColor& Bg, const FLinearColor& Outline, float OutlineWidth)
	{
		FSlateBrush Brush;
		Brush.DrawAs = ESlateBrushDrawType::RoundedBox;
		Brush.TintColor = Bg;
		Brush.OutlineSettings = FSlateBrushOutlineSettings(
			FVector4(GSkillSlotPx / 2.f, GSkillSlotPx / 2.f, GSkillSlotPx / 2.f, GSkillSlotPx / 2.f),
			Outline, OutlineWidth);
		return Brush;
	}
}

void UMASkillBarWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	SetVisibility(ESlateVisibility::HitTestInvisible);

	UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("Root"));
	WidgetTree->RootWidget = Root;

	UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("Row"));
	UCanvasPanelSlot* RowSlot = Root->AddChildToCanvas(Row);
	RowSlot->SetAnchors(FAnchors(1.f, 1.f));
	RowSlot->SetAlignment(FVector2D(1.f, 1.f));
	RowSlot->SetAutoSize(true);
	RowSlot->SetPosition(FVector2D(-24.f, -252.f));   // 小地图正上方

	struct FSlotDef
	{
		const TCHAR* Name;
		const TCHAR* Hotkey;
		FGameplayTag Cooldown;
		FGameplayTag Active;
	};
	const FSlotDef Defs[] = {
		{ TEXT("近战"), TEXT("LMB"),  MAGameplayTags::Ability_Cooldown_Melee,    MAGameplayTags::State_Attacking },
		{ TEXT("翻滚"), TEXT("Ctrl"), MAGameplayTags::Ability_Cooldown_Dodge,    MAGameplayTags::State_Dodging },
		{ TEXT("火球"), TEXT("Q"),    MAGameplayTags::Ability_Cooldown_Fireball, MAGameplayTags::State_Casting },
		{ TEXT("格挡"), TEXT("RMB"),  FGameplayTag(),                            MAGameplayTags::State_Blocking },
	};

	for (const FSlotDef& Def : Defs)
	{
		FSlotRuntime Runtime;
		Runtime.CooldownTag = Def.Cooldown;
		Runtime.ActiveTag = Def.Active;

		UVerticalBox* Column = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		if (UHorizontalBoxSlot* HS = Row->AddChildToHorizontalBox(Column))
		{
			HS->SetPadding(FMargin(5.f, 0.f));
		}

		USizeBox* Box = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		Box->SetWidthOverride(GSkillSlotPx);
		Box->SetHeightOverride(GSkillSlotPx);
		if (UVerticalBoxSlot* VS = Column->AddChildToVerticalBox(Box))
		{
			VS->SetHorizontalAlignment(HAlign_Center);
		}

		UOverlay* Stack = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass());
		Box->AddChild(Stack);

		Runtime.Circle = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
		Runtime.Circle->SetBrush(MakeCircleBrush(GSkillIdleBg, GSkillIdleOutline, 1.5f));
		if (UOverlaySlot* S = Stack->AddChildToOverlay(Runtime.Circle))
		{
			S->SetHorizontalAlignment(HAlign_Fill);
			S->SetVerticalAlignment(VAlign_Fill);
		}

		UTextBlock* Hotkey = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		Hotkey->SetText(FText::FromString(Def.Hotkey));
		Hotkey->SetFont(FCoreStyle::GetDefaultFontStyle("Bold", 13));
		Hotkey->SetColorAndOpacity(FSlateColor(FLinearColor(0.92f, 0.92f, 0.92f, 1.f)));
		Hotkey->SetShadowOffset(FVector2D(1.f, 1.f));
		Hotkey->SetShadowColorAndOpacity(FLinearColor(0.f, 0.f, 0.f, 0.9f));
		if (UOverlaySlot* S = Stack->AddChildToOverlay(Hotkey))
		{
			S->SetHorizontalAlignment(HAlign_Center);
			S->SetVerticalAlignment(VAlign_Center);
		}

		// 冷却压暗层(盖住键名,秒数在其上)
		Runtime.CooldownDim = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
		Runtime.CooldownDim->SetBrush(MakeCircleBrush(FLinearColor(0.f, 0.f, 0.f, 0.72f), FLinearColor::Transparent, 0.f));
		Runtime.CooldownDim->SetVisibility(ESlateVisibility::Collapsed);
		if (UOverlaySlot* S = Stack->AddChildToOverlay(Runtime.CooldownDim))
		{
			S->SetHorizontalAlignment(HAlign_Fill);
			S->SetVerticalAlignment(VAlign_Fill);
		}

		Runtime.CooldownText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		Runtime.CooldownText->SetFont(FCoreStyle::GetDefaultFontStyle("Bold", 16));
		Runtime.CooldownText->SetColorAndOpacity(FSlateColor(GSkillActiveOutline));
		Runtime.CooldownText->SetVisibility(ESlateVisibility::Collapsed);
		if (UOverlaySlot* S = Stack->AddChildToOverlay(Runtime.CooldownText))
		{
			S->SetHorizontalAlignment(HAlign_Center);
			S->SetVerticalAlignment(VAlign_Center);
		}

		UTextBlock* Name = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		Name->SetText(FText::FromString(Def.Name));
		Name->SetFont(FCoreStyle::GetDefaultFontStyle("Bold", 10));
		Name->SetColorAndOpacity(FSlateColor(FLinearColor(0.75f, 0.75f, 0.78f, 1.f)));
		Name->SetShadowOffset(FVector2D(1.f, 1.f));
		Name->SetShadowColorAndOpacity(FLinearColor(0.f, 0.f, 0.f, 0.8f));
		if (UVerticalBoxSlot* VS = Column->AddChildToVerticalBox(Name))
		{
			VS->SetHorizontalAlignment(HAlign_Center);
			VS->SetPadding(FMargin(0.f, 3.f, 0.f, 0.f));
		}

		Slots.Add(Runtime);
	}
}

void UMASkillBarWidget::ApplyCircleStyle(FSlotRuntime& S, bool bActive)
{
	if (S.bActiveVisual == bActive)
	{
		return;
	}
	S.bActiveVisual = bActive;
	S.Circle->SetBrush(bActive
		? MakeCircleBrush(GSkillActiveBg, GSkillActiveOutline, 3.f)
		: MakeCircleBrush(GSkillIdleBg, GSkillIdleOutline, 1.5f));
}

void UMASkillBarWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	if (!ASC.IsValid())
	{
		const APlayerController* PC = GetOwningPlayer();
		const AMAPlayerState* PS = PC ? PC->GetPlayerState<AMAPlayerState>() : nullptr;
		ASC = PS ? PS->GetAbilitySystemComponent() : nullptr;
		if (!ASC.IsValid())
		{
			return;
		}
	}
	UAbilitySystemComponent* Comp = ASC.Get();

	for (FSlotRuntime& S : Slots)
	{
		float Remaining = 0.f;
		if (S.CooldownTag.IsValid())
		{
			const FGameplayEffectQuery Query =
				FGameplayEffectQuery::MakeQuery_MatchAnyOwningTags(FGameplayTagContainer(S.CooldownTag));
			for (const TPair<float, float>& T : Comp->GetActiveEffectsTimeRemainingAndDuration(Query))
			{
				Remaining = FMath::Max(Remaining, T.Key);
			}
		}
		const bool bCooling = Remaining > 0.f;
		if (bCooling != S.bCoolingVisual)
		{
			S.bCoolingVisual = bCooling;
			S.CooldownDim->SetVisibility(bCooling ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
			S.CooldownText->SetVisibility(bCooling ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
		}
		if (bCooling)
		{
			S.CooldownText->SetText(FText::FromString(FString::Printf(TEXT("%.1f"), Remaining)));
		}

		ApplyCircleStyle(S, S.ActiveTag.IsValid() && Comp->HasMatchingGameplayTag(S.ActiveTag));
	}
}
