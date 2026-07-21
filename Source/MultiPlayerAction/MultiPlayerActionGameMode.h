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

	/** 随机出生点：洗牌全部 PlayerStart 取第一个附近无活人的（引擎默认总选第一个）；
	 *  编辑器"从此处游玩"（PlayerStartPIE）仍然优先。 */
	virtual AActor* ChoosePlayerStart_Implementation(AController* Player) override;

	/** Match clock length, seconds; <=0 = 不限时（剧情模式默认）。EditAnywhere: per-level tunable. */
	UPROPERTY(EditAnywhere, Category = "Match")
	float MatchDuration = 0.f;

	/** Kills that end the match early; <=0 = 不以杀数终结（剧情模式默认）。 */
	UPROPERTY(EditAnywhere, Category = "Match")
	int32 KillTarget = 0;

	/** Post-match results screen duration before the map restarts. */
	UPROPERTY(EditAnywhere, Category = "Match")
	float PostMatchDuration = 10.f;

private:
	void RestartMatch();
	FTimerHandle RestartTimerHandle;
};
