#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "MACombatantInterface.generated.h"

UINTERFACE(BlueprintType, MinimalAPI)
class UMACombatantInterface : public UInterface
{
	GENERATED_BODY()
};

/**
 * Anything that can die in combat. AttributeSet calls HandleDeath when Health crosses 0,
 * letting Character / NPC implement their own death visuals + respawn policy without
 * coupling AttributeSet to specific actor classes.
 */
class MULTIPLAYERACTION_API IMACombatantInterface
{
	GENERATED_BODY()

public:
	/** Called server-side once when Health first reaches 0. Implementer drives ragdoll/respawn. */
	UFUNCTION(BlueprintNativeEvent, Category = "Combat")
	void HandleDeath();
	virtual void HandleDeath_Implementation() {}
};
