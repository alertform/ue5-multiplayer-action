#pragma once

#include "CoreMinimal.h"

/** Feedback strengths for one landed hit, derived from post-mitigation damage. */
struct FMAHitFeelParams
{
	float HitStopSeconds = 0.f;    // attacker+victim mesh-freeze duration
	float KnockbackImpulse = 0.f;  // horizontal LaunchCharacter magnitude
};

namespace MAHitFeel
{
	/** Pure damage→feedback curve (no World, no UObject). Unit-tested in MAHitFeelTest.cpp.
	 *  Sized to the default loadout (AttackPower 20, Armor 10, ArmorScale 0.05):
	 *  unblocked 10 dmg → 300 impulse / 0.08 s; blocked (30% through) 3 dmg → 90 / 0.052 s.
	 *  Zero/negative damage → all-zero params (no feedback on fully-mitigated hits). */
	FMAHitFeelParams ComputeHitFeel(float FinalDamage);

	/** ma.HitFeel master toggle (cvar defined once, in MAHitFeel.cpp). Game thread only. */
	bool IsEnabled();

	/** Upward pop added to knockback so ground friction doesn't eat the slide. */
	inline constexpr float KnockbackZBoost = 50.f;
}
