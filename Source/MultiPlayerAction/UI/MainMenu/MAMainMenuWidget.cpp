#include "UI/MainMenu/MAMainMenuWidget.h"
#include "UI/MainMenu/MAMainMenuViewModel.h"
#include "UI/MainMenu/MASessionListEntryVM.h"
#include "Online/MASessionSubsystem.h"
#include "Components/Button.h"
#include "Components/ListView.h"
#include "MVVMSubsystem.h"
#include "View/MVVMView.h"
#include "GameFramework/PlayerController.h"
#include "Engine/GameInstance.h"

void UMAMainMenuWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	ViewModel = NewObject<UMAMainMenuViewModel>(this);
	ViewModel->OnSessionsChanged.AddDynamic(this, &UMAMainMenuWidget::HandleSessionsChanged);

	if (!bClickHandlersBound)
	{
		if (HostButton)    { HostButton->OnClicked.AddDynamic(this, &UMAMainMenuWidget::HandleHostClicked); }
		if (RefreshButton) { RefreshButton->OnClicked.AddDynamic(this, &UMAMainMenuWidget::HandleRefreshClicked); }
		if (QuitButton)    { QuitButton->OnClicked.AddDynamic(this, &UMAMainMenuWidget::HandleQuitClicked); }
		bClickHandlersBound = true;
	}
}

void UMAMainMenuWidget::NativeConstruct()
{
	Super::NativeConstruct();

	APlayerController* PC = GetOwningPlayer();
	if (UGameInstance* GI = GetGameInstance())
	{
		ViewModel->Initialize(GI->GetSubsystem<UMASessionSubsystem>(), PC);
	}

	if (UMVVMView* View = UMVVMSubsystem::GetViewFromUserWidget(this))
	{
		View->SetViewModelByClass(ViewModel);
	}

	if (PC)
	{
		FInputModeUIOnly Mode;
		PC->SetInputMode(Mode);
		PC->SetShowMouseCursor(true);
	}

	HandleSessionsChanged();
}

void UMAMainMenuWidget::HandleHostClicked()    { if (ViewModel) { ViewModel->Host(); } }
void UMAMainMenuWidget::HandleRefreshClicked() { if (ViewModel) { ViewModel->Refresh(); } }
void UMAMainMenuWidget::HandleQuitClicked()    { if (ViewModel) { ViewModel->Quit(); } }

void UMAMainMenuWidget::HandleSessionsChanged()
{
	if (!SessionListView || !ViewModel)
	{
		return;
	}

	TArray<UObject*> Items;
	Items.Reserve(ViewModel->GetSessions().Num());
	for (const TObjectPtr<UMASessionListEntryVM>& Entry : ViewModel->GetSessions())
	{
		Items.Add(Entry);
	}
	SessionListView->SetListItems(Items);
}
