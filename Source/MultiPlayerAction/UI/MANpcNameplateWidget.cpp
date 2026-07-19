#include "UI/MANpcNameplateWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "GameFramework/Pawn.h"
#include "Styling/CoreStyle.h"

// unity build 防碰撞：file-local 常量带文件前缀。
static const FLinearColor GNpcNameplateName(0.55f, 0.85f, 1.0f, 1.f);   // 冷蓝 —— 与敌人红/HUD 金区分
static const FLinearColor GNpcNameplateHint(0.92f, 0.92f, 0.92f, 0.9f);

void UMANpcNameplateWidget::Setup(const FString& InNpcName, float InInteractRadius, AActor* InNpcActor)
{
	NpcActor = InNpcActor;
	InteractRadiusSq = InInteractRadius * InInteractRadius;
	if (NameText)
	{
		NameText->SetText(FText::FromString(InNpcName));
	}
}

void UMANpcNameplateWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	SetVisibility(ESlateVisibility::HitTestInvisible);

	UVerticalBox* Stack = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("Stack"));
	WidgetTree->RootWidget = Stack;

	auto MakeText = [this](int32 FontSize, const FLinearColor& Color) -> UTextBlock*
	{
		UTextBlock* Text = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		Text->SetFont(FCoreStyle::GetDefaultFontStyle("Bold", FontSize));
		Text->SetColorAndOpacity(FSlateColor(Color));
		Text->SetShadowOffset(FVector2D(1.f, 1.f));
		Text->SetShadowColorAndOpacity(FLinearColor(0.f, 0.f, 0.f, 0.9f));
		Text->SetJustification(ETextJustify::Center);
		return Text;
	};

	NameText = MakeText(14, GNpcNameplateName);
	if (UVerticalBoxSlot* NameSlot = Stack->AddChildToVerticalBox(NameText))
	{
		NameSlot->SetHorizontalAlignment(HAlign_Center);
	}

	HintText = MakeText(11, GNpcNameplateHint);
	HintText->SetText(FText::FromString(TEXT("按 T 对话 · 可接任务")));
	HintText->SetVisibility(ESlateVisibility::Collapsed);
	if (UVerticalBoxSlot* HintSlot = Stack->AddChildToVerticalBox(HintText))
	{
		HintSlot->SetHorizontalAlignment(HAlign_Center);
	}
}

void UMANpcNameplateWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	// 根常显（Collapsed 根不 tick 的教训），只切提示行。
	const AActor* Npc = NpcActor.Get();
	const APawn* LocalPawn = GetOwningPlayerPawn();
	if (!Npc || !LocalPawn || !HintText)
	{
		return;
	}
	const bool bInRange =
		FVector::DistSquared(Npc->GetActorLocation(), LocalPawn->GetActorLocation()) <= InteractRadiusSq;
	HintText->SetVisibility(bInRange ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
}
