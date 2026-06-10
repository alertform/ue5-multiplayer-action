#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Cues/GCN_ParticleBurst.h"
#include "GCN_MeleeImpact.generated.h"

class USoundBase;
class UCameraShakeBase;

/**
 * Full melee impact package on top of the parent's particle burst: impact sound,
 * hit stop (brief mesh-animation freeze on attacker AND victim) and viewpoint-routed
 * camera shakes (light for the attacker's local view, heavy for the victim's).
 *
 * Executes on every relevant client via the attacker ASC's GameplayCue.Melee.Hit.
 * MyTarget = attacker avatar; the victim arrives in Parameters.SourceObject
 * (replicated — set in UGA_MeleeAttack::PerformHitTrace). Everything beyond the
 * parent particle is gated on ma.HitFeel. Cosmetic only — the freeze is
 * GlobalAnimRateScale on the mesh, never time dilation, so the movement simulation
 * (and its prediction) is untouched.
 */
UCLASS(Abstract, Blueprintable)
class MULTIPLAYERACTION_API UGCN_MeleeImpact : public UGCN_ParticleBurst
{
	GENERATED_BODY()

public:
	UGCN_MeleeImpact();

	virtual bool OnExecute_Implementation(AActor* MyTarget, const FGameplayCueParameters& Parameters) const override;

protected:
	/** One-shot impact sound at the hit location. Optional — unset skips cleanly. */
	UPROPERTY(EditDefaultsOnly, Category = "Impact")
	TObjectPtr<USoundBase> ImpactSound;

	/** Mesh-freeze duration for attacker + victim. Fixed (not damage-scaled): the cue fires
	 *  before the damage GE resolves, so post-mitigation damage isn't knowable here (spec §6). */
	UPROPERTY(EditDefaultsOnly, Category = "Impact", meta = (ClampMin = "0.0", UIMax = "0.2"))
	float HitStopSeconds = 0.07f;

	/** Shake for the local player who dealt the hit. C++ defaults to UMACameraShake_HitLight. */
	UPROPERTY(EditDefaultsOnly, Category = "Impact")
	TSubclassOf<UCameraShakeBase> AttackerShake;

	/** Shake for the local player who took the hit. C++ defaults to UMACameraShake_HitHeavy. */
	UPROPERTY(EditDefaultsOnly, Category = "Impact")
	TSubclassOf<UCameraShakeBase> VictimShake;
};
