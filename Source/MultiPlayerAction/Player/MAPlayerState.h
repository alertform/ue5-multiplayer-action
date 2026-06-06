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

protected:
	// ASC lives on PlayerState for persistence across respawns
	UPROPERTY(VisibleAnywhere, Category = "GAS")
	TObjectPtr<UMAAbilitySystemComponent> AbilitySystemComponent;

	UPROPERTY()
	TObjectPtr<UMAAttributeSet> AttributeSet;

private:
	bool bStartupAbilitiesGranted = false;
};
