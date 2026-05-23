#include "UI/MainMenu/MAMainMenuViewModel.h"
#include "UI/MainMenu/MASessionListEntryVM.h"
#include "Online/MASessionSubsystem.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/KismetSystemLibrary.h"

void UMAMainMenuViewModel::Initialize(UMASessionSubsystem* InSubsystem, APlayerController* InOwningPC)
{
	OwningPC = InOwningPC;
	Subsystem = InSubsystem;
	if (bInitialized || !Subsystem)
	{
		return;
	}

	Subsystem->OnHostSessionComplete.AddDynamic(this, &UMAMainMenuViewModel::HandleHostComplete);
	Subsystem->OnFindSessionsComplete.AddDynamic(this, &UMAMainMenuViewModel::HandleFindComplete);
	Subsystem->OnJoinSessionComplete.AddDynamic(this, &UMAMainMenuViewModel::HandleJoinComplete);
	bInitialized = true;
}

void UMAMainMenuViewModel::Host()
{
	if (!Subsystem) { return; }
	SetIsBusy(true);
	SetStatusText(FText::FromString(TEXT("Creating session...")));
	Subsystem->HostSession(MaxPlayers, TEXT("/Game/Maps/ThirdPersonMap"), PlayerName);
}

void UMAMainMenuViewModel::Refresh()
{
	if (!Subsystem) { return; }
	SetIsBusy(true);
	SetStatusText(FText::FromString(TEXT("Searching...")));
	Subsystem->FindSessions();
}

void UMAMainMenuViewModel::Join(int32 EntryIndex)
{
	if (!Subsystem) { return; }
	SetIsBusy(true);
	SetStatusText(FText::FromString(TEXT("Joining...")));
	Subsystem->SetLocalPlayerName(PlayerName);
	Subsystem->JoinSessionByIndex(EntryIndex);
}

void UMAMainMenuViewModel::Quit()
{
	if (APlayerController* PC = OwningPC.Get())
	{
		UKismetSystemLibrary::QuitGame(PC, PC, EQuitPreference::Quit, false);
	}
}

void UMAMainMenuViewModel::HandleHostComplete(bool bSucceeded)
{
	SetIsBusy(false);
	if (!bSucceeded)
	{
		SetStatusText(FText::FromString(TEXT("Host failed.")));
	}
}

void UMAMainMenuViewModel::HandleFindComplete(bool bSucceeded, const TArray<FMASessionInfo>& InSessions)
{
	SetIsBusy(false);
	Sessions.Reset();

	if (bSucceeded)
	{
		for (int32 i = 0; i < InSessions.Num(); ++i)
		{
			UMASessionListEntryVM* Entry = NewObject<UMASessionListEntryVM>(this);
			Entry->InitFromInfo(InSessions[i], i, this);
			Sessions.Add(Entry);
		}
		SetStatusText(FText::FromString(Sessions.Num() > 0
			? FString::Printf(TEXT("%d session(s) found."), Sessions.Num())
			: TEXT("No sessions found.")));
	}
	else
	{
		SetStatusText(FText::FromString(TEXT("Search failed.")));
	}

	SetHasSessions(Sessions.Num() > 0);
	OnSessionsChanged.Broadcast();
}

void UMAMainMenuViewModel::HandleJoinComplete(bool bSucceeded)
{
	SetIsBusy(false);
	if (!bSucceeded)
	{
		SetStatusText(FText::FromString(TEXT("Join failed.")));
	}
}
