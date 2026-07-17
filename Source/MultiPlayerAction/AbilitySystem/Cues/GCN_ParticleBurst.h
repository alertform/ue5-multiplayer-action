#pragma once

#include "CoreMinimal.h"
#include "GameplayCueNotify_Static.h"
#include "GCN_ParticleBurst.generated.h"

class UParticleSystem;
class UNiagaraSystem;

/**
 * Data-driven burst cue: spawns a one-shot Cascade particle and/or a one-shot Niagara system
 * at the cue's impact location. C++ base so BP children stay pure data (set the templates +
 * GameplayCueTag in defaults, no per-cue graph). Users: BP_GCN_FireballExplosion
 * (GameplayCue.Fireball.Explosion), BP_GCN_MeleeHit (sparks + blood splatter).
 */
UCLASS(Abstract, Blueprintable)
class MULTIPLAYERACTION_API UGCN_ParticleBurst : public UGameplayCueNotify_Static
{
	GENERATED_BODY()

public:
	virtual bool OnExecute_Implementation(AActor* MyTarget, const FGameplayCueParameters& Parameters) const override;

protected:
	/** One-shot Cascade template spawned at Parameters.Location, oriented along Parameters.Normal */
	UPROPERTY(EditDefaultsOnly, Category = "ParticleBurst")
	TObjectPtr<UParticleSystem> ParticleTemplate;

	/** Optional one-shot Niagara system spawned at the same impact point (e.g. blood splatter).
	 *  Fires alongside ParticleTemplate — either or both may be set. */
	UPROPERTY(EditDefaultsOnly, Category = "ParticleBurst")
	TObjectPtr<UNiagaraSystem> NiagaraTemplate;
};
