#pragma once

#include "CoreMinimal.h"
#include "GameplayEffectTypes.h"

class UAbilitySystemComponent;

/**
 * Server-authoritative melee sweep + application, shared by GA_MeleeAttack and GA_DashSlash:
 * camera-yaw sphere sweep (lag-comp rewound when ma.LagComp.Enabled), AI-faction friendly-fire
 * filter, damage GE per unique victim, GameplayCue.Melee.Hit with the victim in SourceObject.
 * Caller owns the authority check and the outgoing spec.
 */
namespace MAMeleeHitOps
{
	/** Returns the number of victims the damage spec was applied to. */
	int32 SweepAndApplyMeleeHit(AActor* Avatar, UAbilitySystemComponent* SourceASC,
		const FGameplayEffectSpecHandle& DamageSpecHandle, float TraceDistance, float TraceRadius);
}
