#include "Dialogue/MADialogueComponent.h"

#include "Components/WidgetComponent.h"
#include "UI/MANpcNameplateWidget.h"

void UMADialogueComponent::BeginPlay()
{
	Super::BeginPlay();

	AActor* Owner = GetOwner();
	if (!Owner)
	{
		return;
	}

	// 头顶铭牌：screen-space、按需尺寸，挂根组件上方。DS 上没有本地玩家也无妨 ——
	// widget 的提示行靠 GetOwningPlayerPawn 判距，取不到就保持收起。
	NameplateComponent = NewObject<UWidgetComponent>(Owner, TEXT("NpcNameplate"));
	NameplateComponent->SetupAttachment(Owner->GetRootComponent());
	NameplateComponent->SetRelativeLocation(FVector(0.f, 0.f, 120.f));
	NameplateComponent->SetWidgetSpace(EWidgetSpace::Screen);
	NameplateComponent->SetDrawAtDesiredSize(true);
	NameplateComponent->SetWidgetClass(UMANpcNameplateWidget::StaticClass());
	NameplateComponent->RegisterComponent();

	if (UMANpcNameplateWidget* Nameplate = Cast<UMANpcNameplateWidget>(NameplateComponent->GetUserWidgetObject()))
	{
		Nameplate->Setup(NpcName, InteractRadius, Owner);
	}
}
