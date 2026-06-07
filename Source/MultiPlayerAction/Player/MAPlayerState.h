#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerState.h"
#include "AbilitySystemInterface.h"
#include "MAPlayerState.generated.h"

class UMAAbilitySystemComponent;
class UMAAttributeSet;

UCLASS()
class MULTIPLAYERACTION_API AMAPlayerState : public APlayerState, public IAbilitySystemInterface
{
	GENERATED_BODY()

public:
	AMAPlayerState();

	// IAbilitySystemInterface
	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;

	UMAAttributeSet* GetAttributeSet() const { return AttributeSet; }

	/** Server-only: default abilities + persistent regen GE are granted exactly once per ASC lifetime.
	 *  The ASC survives pawn death/respawn, so the guard must live here, not on the Character. */
	bool HasGrantedStartupAbilities() const { return bStartupAbilitiesGranted; }
	void MarkStartupAbilitiesGranted() { bStartupAbilitiesGranted = true; }

	UFUNCTION(BlueprintPure, Category = "Match")
	int32 GetKills() const { return Kills; }

	UFUNCTION(BlueprintPure, Category = "Match")
	int32 GetDeaths() const { return Deaths; }

	/** Server-only scoreboard mutators — only the GameMode's kill router calls these. */
	void AddKill() { ++Kills; }
	void AddDeath() { ++Deaths; }

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:
	// ASC lives on PlayerState for persistence across respawns
	UPROPERTY(VisibleAnywhere, Category = "GAS")
	TObjectPtr<UMAAbilitySystemComponent> AbilitySystemComponent;

	UPROPERTY()
	TObjectPtr<UMAAttributeSet> AttributeSet;

	/** Match-scoped tallies; no reset path needed — the post-match map restart is a full
	 *  (non-seamless) travel that recreates every PlayerState. UI polls, so no OnRep. */
	UPROPERTY(Replicated)
	int32 Kills = 0;

	UPROPERTY(Replicated)
	int32 Deaths = 0;

private:
	bool bStartupAbilitiesGranted = false;
};
