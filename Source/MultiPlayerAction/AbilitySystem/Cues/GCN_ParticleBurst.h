#pragma once

#include "CoreMinimal.h"
#include "GameplayCueNotify_Static.h"
#include "GCN_ParticleBurst.generated.h"

class UParticleSystem;

/**
 * Data-driven burst cue: spawns a one-shot Cascade particle at the cue's impact location.
 * C++ base so BP children stay pure data (set ParticleTemplate + GameplayCueTag in defaults,
 * no per-cue graph). First user: BP_GCN_FireballExplosion (GameplayCue.Fireball.Explosion).
 */
UCLASS(Abstract, Blueprintable)
class MULTIPLAYERACTION_API UGCN_ParticleBurst : public UGameplayCueNotify_Static
{
	GENERATED_BODY()

public:
	virtual bool OnExecute_Implementation(AActor* MyTarget, const FGameplayCueParameters& Parameters) const override;

protected:
	/** One-shot particle template spawned at Parameters.Location, oriented along Parameters.Normal */
	UPROPERTY(EditDefaultsOnly, Category = "ParticleBurst")
	TObjectPtr<UParticleSystem> ParticleTemplate;
};
