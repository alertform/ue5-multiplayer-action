#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "MAPlayerController.generated.h"

class UMAUserWidget;
class UMAMatchStatusWidget;
class UMAScoreboardWidget;
class UMAKillFeedWidget;

/**
 * Owns local-player UI. Spawns the HUD widget on BeginPlay and binds it to the
 * owning ASC as soon as the PlayerState is available (server: in BeginPlay,
 * client: in OnRep_PlayerState — whichever happens first). Also owns the match
 * overlay (clock/score/respawn) and the Tab scoreboard — both code-built C++
 * widgets spawned straight from their classes, no BP assets involved.
 */
UCLASS()
class MULTIPLAYERACTION_API AMAPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	AMAPlayerController();

	virtual void BeginPlay() override;
	virtual void OnRep_PlayerState() override;

	/** Dev cheat: apply damage to own Health (bypasses GE pipeline) — 控制台输 DamageSelf 10 */
	UFUNCTION(Exec)
	void DamageSelf(float Amount = 10.f);

	/** Server RPC backing DamageSelf — ensures the authoritative ASC is the one mutated */
	UFUNCTION(Server, Reliable)
	void Server_DamageSelf(float Amount);

	/** Server-only: queue Respawn() to fire after Delay seconds */
	void ScheduleRespawn(float Delay);

	/** Show/hide the Tab scoreboard. While WaitingPostMatch it is pinned visible regardless. */
	void SetScoreboardVisible(bool bVisible);

	/** Local-player reaction to the match ending — called on every machine from
	 *  AMAGameState::HandleMatchHasEnded (replication-driven, no extra RPC). */
	void OnLocalMatchEnded();

	/** Server -> owning client: HUD respawn countdown (server world time when respawn fires). */
	UFUNCTION(Client, Reliable)
	void Client_OnRespawnScheduled(float RespawnEndServerTime);

protected:
	void Respawn();
	FTimerHandle RespawnTimerHandle;

protected:
	/** HUD widget class (set in BP_MAPlayerController defaults to WBP_HUD) */
	UPROPERTY(EditDefaultsOnly, Category = "UI")
	TSubclassOf<UMAUserWidget> HUDWidgetClass;

	UPROPERTY()
	TObjectPtr<UMAUserWidget> HUDWidget;

	/** Match overlay (clock / K-D / respawn countdown). Defaults to the C++ class. */
	UPROPERTY(EditDefaultsOnly, Category = "UI")
	TSubclassOf<UMAMatchStatusWidget> MatchStatusWidgetClass;

	UPROPERTY()
	TObjectPtr<UMAMatchStatusWidget> MatchStatusWidget;

	/** Tab scoreboard / post-match results panel. Defaults to the C++ class. */
	UPROPERTY(EditDefaultsOnly, Category = "UI")
	TSubclassOf<UMAScoreboardWidget> ScoreboardWidgetClass;

	UPROPERTY()
	TObjectPtr<UMAScoreboardWidget> ScoreboardWidget;

	/** Top-right kill feed. Defaults to the C++ class. */
	UPROPERTY(EditDefaultsOnly, Category = "UI")
	TSubclassOf<UMAKillFeedWidget> KillFeedWidgetClass;

	UPROPERTY()
	TObjectPtr<UMAKillFeedWidget> KillFeedWidget;

private:
	/** Idempotent: creates widget if not yet created, then binds to ASC if PS available. */
	void EnsureHUDInitialized();
};
