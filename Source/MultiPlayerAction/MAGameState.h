#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameState.h"
#include "MAGameState.generated.h"

/**
 * Match-scoped replicated state for the deathmatch loop. The engine MatchState machine
 * (AGameMode/AGameState) already drives the phases and replicates them — this class only
 * adds the data clients need to RENDER the loop: countdown clock, kill target, verdict.
 *
 * All times are server world seconds (GetServerWorldTimeSeconds): the engine keeps that
 * clock synced on clients, so countdowns render everywhere without per-second RPCs.
 */
UCLASS()
class MULTIPLAYERACTION_API AMAGameState : public AGameState
{
	GENERATED_BODY()

public:
	/** Server world time at which the match clock expires; negative until the match starts. */
	UPROPERTY(BlueprintReadOnly, Replicated, Category = "Match")
	float MatchEndServerTime = -1.f;

	/** First player to reach this many kills ends the match early. Pushed from GameMode config. */
	UPROPERTY(BlueprintReadOnly, Replicated, Category = "Match")
	int32 KillTarget = 10;

	/** Winner's display name; empty = draw. Set server-side in GameMode::HandleMatchHasEnded
	 *  BEFORE the state flips (same frame, same actor — arrives in one bunch with MatchState). */
	UPROPERTY(BlueprintReadOnly, Replicated, Category = "Match")
	FString WinnerName;

	/** Server world time at which the post-match screen ends and the map restarts. */
	UPROPERTY(BlueprintReadOnly, Replicated, Category = "Match")
	float RestartServerTime = -1.f;

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:
	/** Runs on the server AND on every client when MatchState hits WaitingPostMatch —
	 *  the one replication-driven hook where local-player UI reacts to the match ending. */
	virtual void HandleMatchHasEnded() override;
};
