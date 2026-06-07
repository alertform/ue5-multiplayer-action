#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "MAMatchStatusWidget.generated.h"

class UTextBlock;

/**
 * Top-center match readout: countdown clock, own "K x/target  D y" line, and a centered
 * respawn countdown while dead. Entirely code-built (WidgetTree in NativeOnInitialized) —
 * the PlayerController spawns it straight from this class, no BP asset involved.
 *
 * All data is read from replicated AMAGameState / own AMAPlayerState in NativeTick;
 * countdowns derive from the engine-synced server clock, so there is no event plumbing.
 */
UCLASS()
class MULTIPLAYERACTION_API UMAMatchStatusWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** Server world time when the local player's respawn fires; pushed via client RPC. */
	void SetRespawnEndServerTime(float InTime) { RespawnEndServerTime = InTime; }

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual void NativeDestruct() override;

private:
	UPROPERTY()
	TObjectPtr<UTextBlock> ClockText;

	UPROPERTY()
	TObjectPtr<UTextBlock> ScoreText;

	UPROPERTY()
	TObjectPtr<UTextBlock> RespawnText;

	/** "KILLED BY <name>" line above the respawn countdown; fed by the GameState kill event. */
	UPROPERTY()
	TObjectPtr<UTextBlock> KilledByText;

	float RespawnEndServerTime = -1.f;
	FString LastKillerName;
	bool bBoundToKillEvent = false;

	void HandleKill(const FString& KillerName, const FString& VictimName);

	/** Shadowed bold text in the HUD palette. */
	UTextBlock* MakeText(int32 FontSize, const FLinearColor& Color);
};
