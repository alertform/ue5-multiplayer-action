#include "UI/MainMenu/MAMainMenuViewModel.h"
#include "UI/MainMenu/MASessionListEntryVM.h"
#include "Online/MASessionSubsystem.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/KismetSystemLibrary.h"

namespace
{
	/** Gameplay map the host travels into (and clients follow via session join). */
	const TCHAR* const GameplayMapPath = TEXT("/Game/External/LAKETOWN/MAPS/LAKETOWN");

	/** Fixed lobby size — the menu no longer exposes a player-count field. */
	const int32 DefaultMaxPlayers = 4;
}

void UMAMainMenuViewModel::Initialize(UMASessionSubsystem* InSubsystem, APlayerController* InOwningPC)
{
	OwningPC = InOwningPC; // refresh the owning PC on every construct
	if (bInitialized || !InSubsystem)
	{
		return;
	}

	Subsystem = InSubsystem;
	Subsystem->OnHostSessionComplete.AddDynamic(this, &UMAMainMenuViewModel::HandleHostComplete);
	Subsystem->OnFindSessionsComplete.AddDynamic(this, &UMAMainMenuViewModel::HandleFindComplete);
	Subsystem->OnJoinSessionComplete.AddDynamic(this, &UMAMainMenuViewModel::HandleJoinComplete);
	bInitialized = true;
}

void UMAMainMenuViewModel::Deinitialize()
{
	if (Subsystem)
	{
		Subsystem->OnHostSessionComplete.RemoveDynamic(this, &UMAMainMenuViewModel::HandleHostComplete);
		Subsystem->OnFindSessionsComplete.RemoveDynamic(this, &UMAMainMenuViewModel::HandleFindComplete);
		Subsystem->OnJoinSessionComplete.RemoveDynamic(this, &UMAMainMenuViewModel::HandleJoinComplete);
	}
	bInitialized = false;
}

void UMAMainMenuViewModel::Host()
{
	if (!Subsystem) { return; }
	SetIsBusy(true);
	SetStatusText(FText::FromString(TEXT("Creating session...")));
	// Empty name => the engine assigns default player names (Player 0/1/...).
	Subsystem->HostSession(DefaultMaxPlayers, GameplayMapPath, FString());
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
