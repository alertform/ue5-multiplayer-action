#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameState.h"
#include "MAGameState.generated.h"

/** Fired locally on every machine when a kill replicates in (see Multicast_OnKill).
 *  Killer may be empty (unattributed death — cheats/suicide). */
DECLARE_MULTICAST_DELEGATE_TwoParams(FMAOnKillEvent, const FString& /*KillerName*/, const FString& /*VictimName*/);

/** 敌方阵营喊话（LLM 战术指挥官的 taunt）在每台机器上的本地分发。 */
DECLARE_MULTICAST_DELEGATE_OneParam(FMAOnTauntEvent, const FString& /*Text*/);

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

	/** Server-only entry (GameMode kill router): pushes one kill line to every machine.
	 *  Transient event — a feed misses nothing by not being state-replicated to late joiners. */
	UFUNCTION(NetMulticast, Reliable)
	void Multicast_OnKill(const FString& KillerName, const FString& VictimName);

	/** Local-side fan-out for UI (kill feed, KILLED BY). Bind with AddUObject + RemoveAll. */
	FMAOnKillEvent OnKillEvent;

	/** Server-only entry（MATacticalAdvisorSubsystem）：敌方喊话推到每台机器。 */
	UFUNCTION(NetMulticast, Reliable)
	void Multicast_OnTaunt(const FString& Text);

	FMAOnTauntEvent OnTauntEvent;

protected:
	/** Runs on the server AND on every client when MatchState hits WaitingPostMatch —
	 *  the one replication-driven hook where local-player UI reacts to the match ending. */
	virtual void HandleMatchHasEnded() override;
};
