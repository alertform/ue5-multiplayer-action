#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "MAMainMenuWidget.generated.h"

class UMAMainMenuViewModel;
class UButton;
class UListView;

/**
 * C++ base for WBP_MainMenu. Creates + injects UMAMainMenuViewModel, wires the action
 * buttons (MVVM command binding is unreliable, so do it here), pushes the session list
 * into the ListView, and sets UI input mode. WBP must name widgets exactly:
 * HostButton, RefreshButton, QuitButton, SessionListView.
 */
UCLASS(Abstract)
class MULTIPLAYERACTION_API UMAMainMenuWidget : public UUserWidget
{
	GENERATED_BODY()

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UButton> HostButton;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UButton> RefreshButton;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UButton> QuitButton;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UListView> SessionListView;

	UPROPERTY(BlueprintReadOnly, Category = "Menu") TObjectPtr<UMAMainMenuViewModel> ViewModel;

private:
	UFUNCTION() void HandleHostClicked();
	UFUNCTION() void HandleRefreshClicked();
	UFUNCTION() void HandleQuitClicked();
	UFUNCTION() void HandleSessionsChanged();

	bool bClickHandlersBound = false;
};
