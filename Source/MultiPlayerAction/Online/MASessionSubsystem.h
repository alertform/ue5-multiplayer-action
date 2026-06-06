#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Interfaces/OnlineSessionInterface.h"
#include "OnlineSessionSettings.h"
#include "MASessionSubsystem.generated.h"

/** BP-friendly snapshot of a session search result for lobby UI display. */
USTRUCT(BlueprintType)
struct FMASessionInfo
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Session") FString OwningUserName;
	UPROPERTY(BlueprintReadOnly, Category = "Session") int32 PingMs = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Session") int32 NumOpenConnections = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Session") int32 NumPublicConnections = 0;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FMAOnSessionEvent, bool, bSucceeded);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FMAOnSessionSearchComplete, bool, bSucceeded, const TArray<FMASessionInfo>&, Sessions);

/**
 * GameInstance subsystem wrapping IOnlineSessionPtr for LAN multiplayer.
 *
 * NULL OSS (configured in DefaultEngine.ini) makes sessions discoverable on the
 * same network without requiring Steam/EOS app credentials.
 *
 * Flow:
 *   Host:   HostSession(MaxPlayers, "/Game/.../GameMap?listen") → CreateSession
 *           → on success: GetWorld()->ServerTravel(URL) into the gameplay map
 *   Client: FindSessions() → OnFindSessionsComplete delivers FMASessionInfo array
 *           → JoinSessionByIndex(i) → on success: ClientTravel via resolved URL
 *
 * BP-callable, BP-bindable. Designed to be driven by a main-menu UMG widget.
 */
UCLASS()
class MULTIPLAYERACTION_API UMASessionSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	UMASessionSubsystem();

	UFUNCTION(BlueprintCallable, Category = "Session")
	void HostSession(int32 NumPublicConnections, const FString& MapName, const FString& PlayerName = TEXT(""));

	/** Store the local player's desired display name; carried into travel as ?Name=. */
	UFUNCTION(BlueprintCallable, Category = "Session")
	void SetLocalPlayerName(const FString& InPlayerName);

	UFUNCTION(BlueprintCallable, Category = "Session")
	void FindSessions(int32 MaxSearchResults = 20);

	UFUNCTION(BlueprintCallable, Category = "Session")
	void JoinSessionByIndex(int32 SessionIndex);

	UFUNCTION(BlueprintCallable, Category = "Session")
	void DestroyCurrentSession();

	UPROPERTY(BlueprintAssignable, Category = "Session")
	FMAOnSessionEvent OnHostSessionComplete;

	UPROPERTY(BlueprintAssignable, Category = "Session")
	FMAOnSessionSearchComplete OnFindSessionsComplete;

	UPROPERTY(BlueprintAssignable, Category = "Session")
	FMAOnSessionEvent OnJoinSessionComplete;

	UPROPERTY(BlueprintAssignable, Category = "Session")
	FMAOnSessionEvent OnDestroySessionComplete;

private:
	FName SessionName = NAME_GameSession;
	FString PendingTravelURL;
	FString DesiredPlayerName;
	TSharedPtr<FOnlineSessionSearch> SearchSettings;

	FDelegateHandle CreateSessionHandle;
	FDelegateHandle FindSessionsHandle;
	FDelegateHandle JoinSessionHandle;
	FDelegateHandle DestroySessionHandle;
	FDelegateHandle StaleDestroyHandle;

	/** Covers the whole async lifecycle of host/join, incl. the stale-destroy detour.
	 *  Set at the start of HostSession/JoinSessionByIndex (after param validation),
	 *  cleared at every terminal exit (success, failure, sync early-out). */
	bool bOperationInFlight = false;

	/** Host/Join request parked while an async destroy of a stale session is in flight. */
	int32 PendingHostConnections = INDEX_NONE;
	int32 PendingJoinIndex = INDEX_NONE;

	void HandleCreateSessionComplete(FName InSessionName, bool bWasSuccessful);
	void HandleFindSessionsComplete(bool bWasSuccessful);
	void HandleJoinSessionComplete(FName InSessionName, EOnJoinSessionCompleteResult::Type Result);
	void HandleDestroySessionComplete(FName InSessionName, bool bWasSuccessful);
	void HandleStaleSessionDestroyed(FName InSessionName, bool bWasSuccessful);

	void StartCreateSession(int32 NumPublicConnections);
	void StartJoinSession(int32 SessionIndex);

	IOnlineSessionPtr GetSessionInterface() const;
	FString BuildNameOption() const;
	static FString SanitizePlayerName(const FString& In);
};
