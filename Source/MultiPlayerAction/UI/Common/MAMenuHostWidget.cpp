#include "UI/Common/MAMenuHostWidget.h"

#include "Blueprint/WidgetTree.h"
#include "CommonActivatableWidget.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Widgets/CommonActivatableWidgetContainer.h"

void UMAMenuHostWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	SetVisibility(ESlateVisibility::SelfHitTestInvisible);

	UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("Root"));
	WidgetTree->RootWidget = Root;

	Stack = WidgetTree->ConstructWidget<UCommonActivatableWidgetStack>(
		UCommonActivatableWidgetStack::StaticClass(), TEXT("MenuStack"));
	UCanvasPanelSlot* S = Root->AddChildToCanvas(Stack);
	S->SetAnchors(FAnchors(0.f, 0.f, 1.f, 1.f));
	S->SetOffsets(FMargin(0.f));
}

UCommonActivatableWidget* UMAMenuHostWidget::Push(TSubclassOf<UCommonActivatableWidget> MenuClass)
{
	return Stack ? Stack->AddWidget(MenuClass) : nullptr;
}

bool UMAMenuHostWidget::HasActive() const
{
	return Stack && Stack->GetActiveWidget() != nullptr;
}

void UMAMenuHostWidget::PopActive()
{
	if (UCommonActivatableWidget* Active = Stack ? Stack->GetActiveWidget() : nullptr)
	{
		Active->DeactivateWidget();
	}
}
