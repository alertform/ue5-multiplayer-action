#pragma once

#include "CoreMinimal.h"
#include "Shakes/DefaultCameraShakeBase.h"
#include "MACameraShakes.generated.h"

/**
 * Hit-feel camera shakes, parameters fixed in C++ (no BP shake assets — same
 * data-in-code philosophy as the native gameplay tags). Both are bSingleInstance
 * so rapid combo hits restart the shake instead of stacking it.
 */

/** Light kick for the attacker's view when a melee hit lands. */
UCLASS()
class MULTIPLAYERACTION_API UMACameraShake_HitLight : public UDefaultCameraShakeBase
{
	GENERATED_BODY()

public:
	UMACameraShake_HitLight(const FObjectInitializer& ObjectInitializer);
};

/** Heavy jolt for the victim's view when taking a melee hit. */
UCLASS()
class MULTIPLAYERACTION_API UMACameraShake_HitHeavy : public UDefaultCameraShakeBase
{
	GENERATED_BODY()

public:
	UMACameraShake_HitHeavy(const FObjectInitializer& ObjectInitializer);
};
