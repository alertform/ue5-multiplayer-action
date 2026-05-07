#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "MAPlayerController.generated.h"

class UMAUserWidget;

/**
 * Owns local-player UI. Spawns the HUD widget on BeginPlay and binds it to the
 * owning ASC as soon as the PlayerState is available (server: in BeginPlay,
 * client: in OnRep_PlayerState — whichever happens first).
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

protected:
	/** HUD widget class (set in BP_MAPlayerController defaults to WBP_HUD) */
	UPROPERTY(EditDefaultsOnly, Category = "UI")
	TSubclassOf<UMAUserWidget> HUDWidgetClass;

	UPROPERTY()
	TObjectPtr<UMAUserWidget> HUDWidget;

private:
	/** Idempotent: creates widget if not yet created, then binds to ASC if PS available. */
	void EnsureHUDInitialized();
};
