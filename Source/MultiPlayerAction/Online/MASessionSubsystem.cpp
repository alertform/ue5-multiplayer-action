#include "Online/MASessionSubsystem.h"
#include "OnlineSubsystem.h"
#include "OnlineSubsystemUtils.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"

UMASessionSubsystem::UMASessionSubsystem() = default;

IOnlineSessionPtr UMASessionSubsystem::GetSessionInterface() const
{
	IOnlineSubsystem* OSS = Online::GetSubsystem(GetWorld());
	return OSS ? OSS->GetSessionInterface() : nullptr;
}

// ---------- Host ----------

void UMASessionSubsystem::HostSession(int32 NumPublicConnections, const FString& MapName, const FString& PlayerName)
{
	if (!PlayerName.IsEmpty())
	{
		SetLocalPlayerName(PlayerName);
	}

	// Hosting requires authority over the current world: a client connected to another
	// server cannot ServerTravel (the engine ensures and drops the connection). Hit when
	// PIE 'Listen Server' mode pre-connects every window to one menu world — menu-flow
	// testing belongs in 'Play Standalone'.
	if (const UWorld* World = GetWorld(); World && World->GetNetMode() == NM_Client)
	{
		UE_LOG(LogNet, Warning,
			TEXT("UMASessionSubsystem::HostSession refused: this world is a connected client and cannot ServerTravel."));
		OnHostSessionComplete.Broadcast(false);
		return;
	}

	// In-flight guard: covers the entire async host lifecycle including the stale-destroy
	// detour. Without this, a second HostSession call before the first completes would
	// overwrite CreateSessionHandle/StaleDestroyHandle and could fire double ServerTravel.
	if (bOperationInFlight)
	{
		UE_LOG(LogNet, Warning, TEXT("UMASessionSubsystem::HostSession refused: operation already in flight"));
		OnHostSessionComplete.Broadcast(false);
		return;
	}
	bOperationInFlight = true;

	// Re-entrancy guard: a stale-session destroy is already in flight (rapid double click) —
	// a second request would orphan StaleDestroyHandle and double-fire the chain (gate review).
	if (PendingHostConnections != INDEX_NONE || PendingJoinIndex != INDEX_NONE)
	{
		bOperationInFlight = false;
		OnHostSessionComplete.Broadcast(false);
		return;
	}

	IOnlineSessionPtr Sessions = GetSessionInterface();
	if (!Sessions.IsValid())
	{
		bOperationInFlight = false;
		OnHostSessionComplete.Broadcast(false);
		return;
	}

	PendingTravelURL = MapName + TEXT("?listen") + BuildNameOption();

	// A stale named session (left over from a previous PIE run — the NULL OSS is
	// process-wide) blocks CreateSession. DestroySession is ASYNC: racing CreateSession
	// against it is what made the first Host click fail. Chain create after the destroy.
	if (Sessions->GetNamedSession(SessionName))
	{
		PendingHostConnections = NumPublicConnections;
		PendingJoinIndex = INDEX_NONE;
		StaleDestroyHandle = Sessions->AddOnDestroySessionCompleteDelegate_Handle(
			FOnDestroySessionCompleteDelegate::CreateUObject(this, &UMASessionSubsystem::HandleStaleSessionDestroyed));
		if (!Sessions->DestroySession(SessionName))
		{
			Sessions->ClearOnDestroySessionCompleteDelegate_Handle(StaleDestroyHandle);
			PendingHostConnections = INDEX_NONE;
			bOperationInFlight = false;
			OnHostSessionComplete.Broadcast(false);
		}
		return;
	}

	StartCreateSession(NumPublicConnections);
}

void UMASessionSubsystem::StartCreateSession(int32 NumPublicConnections)
{
	IOnlineSessionPtr Sessions = GetSessionInterface();
	if (!Sessions.IsValid())
	{
		bOperationInFlight = false;
		OnHostSessionComplete.Broadcast(false);
		return;
	}

	FOnlineSessionSettings Settings;
	Settings.NumPublicConnections = FMath::Max(1, NumPublicConnections);
	Settings.bShouldAdvertise = true;
	Settings.bAllowJoinInProgress = true;
	Settings.bIsLANMatch = true;            // NULL OSS → LAN broadcast
	Settings.bUsesPresence = false;          // not using Steam/EOS presence
	Settings.bAllowJoinViaPresence = false;
	Settings.bAllowInvites = false;
	Settings.bUseLobbiesIfAvailable = false; // NULL OSS doesn't have Steam-style lobbies

	CreateSessionHandle = Sessions->AddOnCreateSessionCompleteDelegate_Handle(
		FOnCreateSessionCompleteDelegate::CreateUObject(this, &UMASessionSubsystem::HandleCreateSessionComplete));

	const ULocalPlayer* LP = GetGameInstance()->GetFirstGamePlayer();
	if (!LP || !Sessions->CreateSession(*LP->GetPreferredUniqueNetId(), SessionName, Settings))
	{
		Sessions->ClearOnCreateSessionCompleteDelegate_Handle(CreateSessionHandle);
		bOperationInFlight = false;
		OnHostSessionComplete.Broadcast(false);
	}
}

void UMASessionSubsystem::HandleStaleSessionDestroyed(FName InSessionName, bool bWasSuccessful)
{
	if (IOnlineSessionPtr Sessions = GetSessionInterface())
	{
		Sessions->ClearOnDestroySessionCompleteDelegate_Handle(StaleDestroyHandle);
	}

	const int32 HostConnections = PendingHostConnections;
	const int32 JoinIndex = PendingJoinIndex;
	PendingHostConnections = INDEX_NONE;
	PendingJoinIndex = INDEX_NONE;

	if (!bWasSuccessful)
	{
		bOperationInFlight = false;
		if (HostConnections != INDEX_NONE) { OnHostSessionComplete.Broadcast(false); }
		if (JoinIndex != INDEX_NONE)       { OnJoinSessionComplete.Broadcast(false); }
		return;
	}

	if (HostConnections != INDEX_NONE)
	{
		StartCreateSession(HostConnections);
	}
	else if (JoinIndex != INDEX_NONE)
	{
		StartJoinSession(JoinIndex);
	}
}

void UMASessionSubsystem::HandleCreateSessionComplete(FName InSessionName, bool bWasSuccessful)
{
	if (IOnlineSessionPtr Sessions = GetSessionInterface())
	{
		Sessions->ClearOnCreateSessionCompleteDelegate_Handle(CreateSessionHandle);
	}

	bOperationInFlight = false;
	OnHostSessionComplete.Broadcast(bWasSuccessful);

	if (bWasSuccessful && !PendingTravelURL.IsEmpty())
	{
		if (UWorld* World = GetWorld())
		{
			World->ServerTravel(PendingTravelURL);
		}
	}
}

// ---------- Find ----------

void UMASessionSubsystem::FindSessions(int32 MaxSearchResults)
{
	IOnlineSessionPtr Sessions = GetSessionInterface();
	if (!Sessions.IsValid())
	{
		OnFindSessionsComplete.Broadcast(false, {});
		return;
	}

	SearchSettings = MakeShared<FOnlineSessionSearch>();
	SearchSettings->MaxSearchResults = FMath::Max(1, MaxSearchResults);
	SearchSettings->bIsLanQuery = true;

	FindSessionsHandle = Sessions->AddOnFindSessionsCompleteDelegate_Handle(
		FOnFindSessionsCompleteDelegate::CreateUObject(this, &UMASessionSubsystem::HandleFindSessionsComplete));

	const ULocalPlayer* LP = GetGameInstance()->GetFirstGamePlayer();
	if (!LP || !Sessions->FindSessions(*LP->GetPreferredUniqueNetId(), SearchSettings.ToSharedRef()))
	{
		Sessions->ClearOnFindSessionsCompleteDelegate_Handle(FindSessionsHandle);
		OnFindSessionsComplete.Broadcast(false, {});
	}
}

void UMASessionSubsystem::HandleFindSessionsComplete(bool bWasSuccessful)
{
	if (IOnlineSessionPtr Sessions = GetSessionInterface())
	{
		Sessions->ClearOnFindSessionsCompleteDelegate_Handle(FindSessionsHandle);
	}

	TArray<FMASessionInfo> Snapshots;
	if (bWasSuccessful && SearchSettings.IsValid())
	{
		for (const FOnlineSessionSearchResult& R : SearchSettings->SearchResults)
		{
			FMASessionInfo Info;
			Info.OwningUserName = R.Session.OwningUserName;
			Info.PingMs = R.PingInMs;
			Info.NumOpenConnections = R.Session.NumOpenPublicConnections;
			Info.NumPublicConnections = R.Session.SessionSettings.NumPublicConnections;
			Snapshots.Add(Info);
		}
	}

	OnFindSessionsComplete.Broadcast(bWasSuccessful, Snapshots);
}

// ---------- Join ----------

void UMASessionSubsystem::JoinSessionByIndex(int32 SessionIndex)
{
	// In-flight guard: covers the entire async join lifecycle including the stale-destroy
	// detour. Without this, a second JoinSession call before the first completes would
	// overwrite JoinSessionHandle/StaleDestroyHandle and could double-fire ClientTravel.
	if (bOperationInFlight)
	{
		UE_LOG(LogNet, Warning, TEXT("UMASessionSubsystem::JoinSessionByIndex refused: operation already in flight"));
		OnJoinSessionComplete.Broadcast(false);
		return;
	}
	bOperationInFlight = true;

	// Re-entrancy guard — see HostSession.
	if (PendingHostConnections != INDEX_NONE || PendingJoinIndex != INDEX_NONE)
	{
		bOperationInFlight = false;
		OnJoinSessionComplete.Broadcast(false);
		return;
	}

	IOnlineSessionPtr Sessions = GetSessionInterface();
	if (!Sessions.IsValid() || !SearchSettings.IsValid() ||
		!SearchSettings->SearchResults.IsValidIndex(SessionIndex))
	{
		bOperationInFlight = false;
		OnJoinSessionComplete.Broadcast(false);
		return;
	}

	// Same stale-session hazard as HostSession: a leftover named session makes
	// JoinSession bail immediately with AlreadyInSession. Destroy, then join.
	// (NOTE: only ever a leftover from THIS instance's previous run — with PIE's
	// "Run Under One Process" enabled all instances share one NULL OSS and the
	// host's live session would be hit here; multiplayer PIE testing requires
	// that setting OFF.)
	if (Sessions->GetNamedSession(SessionName))
	{
		PendingJoinIndex = SessionIndex;
		PendingHostConnections = INDEX_NONE;
		StaleDestroyHandle = Sessions->AddOnDestroySessionCompleteDelegate_Handle(
			FOnDestroySessionCompleteDelegate::CreateUObject(this, &UMASessionSubsystem::HandleStaleSessionDestroyed));
		if (!Sessions->DestroySession(SessionName))
		{
			Sessions->ClearOnDestroySessionCompleteDelegate_Handle(StaleDestroyHandle);
			PendingJoinIndex = INDEX_NONE;
			bOperationInFlight = false;
			OnJoinSessionComplete.Broadcast(false);
		}
		return;
	}

	StartJoinSession(SessionIndex);
}

void UMASessionSubsystem::StartJoinSession(int32 SessionIndex)
{
	IOnlineSessionPtr Sessions = GetSessionInterface();
	if (!Sessions.IsValid() || !SearchSettings.IsValid() ||
		!SearchSettings->SearchResults.IsValidIndex(SessionIndex))
	{
		bOperationInFlight = false;
		OnJoinSessionComplete.Broadcast(false);
		return;
	}

	JoinSessionHandle = Sessions->AddOnJoinSessionCompleteDelegate_Handle(
		FOnJoinSessionCompleteDelegate::CreateUObject(this, &UMASessionSubsystem::HandleJoinSessionComplete));

	const ULocalPlayer* LP = GetGameInstance()->GetFirstGamePlayer();
	if (!LP || !Sessions->JoinSession(*LP->GetPreferredUniqueNetId(), SessionName,
		SearchSettings->SearchResults[SessionIndex]))
	{
		Sessions->ClearOnJoinSessionCompleteDelegate_Handle(JoinSessionHandle);
		bOperationInFlight = false;
		OnJoinSessionComplete.Broadcast(false);
	}
}

void UMASessionSubsystem::HandleJoinSessionComplete(FName InSessionName, EOnJoinSessionCompleteResult::Type Result)
{
	IOnlineSessionPtr Sessions = GetSessionInterface();
	if (Sessions.IsValid())
	{
		Sessions->ClearOnJoinSessionCompleteDelegate_Handle(JoinSessionHandle);
	}

	bOperationInFlight = false;
	const bool bSuccess = (Result == EOnJoinSessionCompleteResult::Success);
	OnJoinSessionComplete.Broadcast(bSuccess);

	if (bSuccess && Sessions.IsValid())
	{
		FString TravelURL;
		if (Sessions->GetResolvedConnectString(SessionName, TravelURL))
		{
			TravelURL += BuildNameOption();
			if (APlayerController* PC = GetGameInstance()->GetFirstLocalPlayerController())
			{
				PC->ClientTravel(TravelURL, TRAVEL_Absolute);
			}
		}
	}
}

// ---------- Destroy ----------

void UMASessionSubsystem::DestroyCurrentSession()
{
	IOnlineSessionPtr Sessions = GetSessionInterface();
	if (!Sessions.IsValid() || !Sessions->GetNamedSession(SessionName))
	{
		OnDestroySessionComplete.Broadcast(true); // already gone
		return;
	}

	DestroySessionHandle = Sessions->AddOnDestroySessionCompleteDelegate_Handle(
		FOnDestroySessionCompleteDelegate::CreateUObject(this, &UMASessionSubsystem::HandleDestroySessionComplete));

	if (!Sessions->DestroySession(SessionName))
	{
		Sessions->ClearOnDestroySessionCompleteDelegate_Handle(DestroySessionHandle);
		OnDestroySessionComplete.Broadcast(false);
	}
}

void UMASessionSubsystem::HandleDestroySessionComplete(FName InSessionName, bool bWasSuccessful)
{
	if (IOnlineSessionPtr Sessions = GetSessionInterface())
	{
		Sessions->ClearOnDestroySessionCompleteDelegate_Handle(DestroySessionHandle);
	}
	OnDestroySessionComplete.Broadcast(bWasSuccessful);
}

void UMASessionSubsystem::SetLocalPlayerName(const FString& InPlayerName)
{
	DesiredPlayerName = SanitizePlayerName(InPlayerName);
}

FString UMASessionSubsystem::SanitizePlayerName(const FString& In)
{
	FString Out = In.TrimStartAndEnd();
	Out.ReplaceInline(TEXT(" "), TEXT("_"));
	Out.ReplaceInline(TEXT("?"), TEXT(""));
	Out.ReplaceInline(TEXT(":"), TEXT(""));
	Out.ReplaceInline(TEXT("#"), TEXT(""));
	Out.ReplaceInline(TEXT("="), TEXT(""));
	Out.ReplaceInline(TEXT("&"), TEXT(""));
	return Out.Left(32);
}

FString UMASessionSubsystem::BuildNameOption() const
{
	return DesiredPlayerName.IsEmpty()
		? FString()
		: FString::Printf(TEXT("?Name=%s"), *DesiredPlayerName);
}
