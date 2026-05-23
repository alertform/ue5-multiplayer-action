#include "UI/MainMenu/MASessionRowWidget.h"
#include "UI/MainMenu/MASessionListEntryVM.h"
#include "MVVMSubsystem.h"
#include "View/MVVMView.h"

void UMASessionRowWidget::NativeOnListItemObjectSet(UObject* ListItemObject)
{
	if (UMASessionListEntryVM* EntryVM = Cast<UMASessionListEntryVM>(ListItemObject))
	{
		if (UMVVMView* View = UMVVMSubsystem::GetViewFromUserWidget(this))
		{
			View->SetViewModelByClass(EntryVM);
		}
	}
}
