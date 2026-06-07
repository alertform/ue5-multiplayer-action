// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameMode.h"
#include "MultiPlayerActionGameMode.generated.h"

/**
 * Deathmatch loop on the engine MatchState machine (WaitingToStart -> InProgress ->
 * WaitingPostMatch). Win on kill target OR match-clock expiry — both polled by the
 * engine via ReadyToEndMatch. Post-match: verdict + freeze, then RestartGame()
 * reloads the map (fresh PlayerStates = fresh scores; the LAN session survives on
 * the GameInstance).
 */
UCLASS(minimalapi)
class AMultiPlayerActionGameMode : public AGameMode
{
	GENERATED_BODY()

public:
	AMultiPlayerActionGameMode();

	/** Server-only: the AttributeSet reports every death here. KillerActor may be a
	 *  PlayerState (player ASCs are PS-owned), a pawn (dummy ASCs are pawn-owned),
	 *  or null (suicide/cheat). Dummy victims have no PlayerState — kill still credits. */
	void NotifyKill(AActor* KillerActor, AActor* VictimActor);

protected:
	virtual void InitGameState() override;
	virtual void HandleMatchHasStarted() override;
	virtual bool ReadyToEndMatch_Implementation() override;
	virtual void HandleMatchHasEnded() override;

	/** Match clock length, seconds. EditAnywhere: tunable per-level via WorldSettings override. */
	UPROPERTY(EditAnywhere, Category = "Match")
	float MatchDuration = 300.f;

	/** Kills that end the match early. */
	UPROPERTY(EditAnywhere, Category = "Match")
	int32 KillTarget = 10;

	/** Post-match results screen duration before the map restarts. */
	UPROPERTY(EditAnywhere, Category = "Match")
	float PostMatchDuration = 10.f;

private:
	void RestartMatch();
	FTimerHandle RestartTimerHandle;
};
