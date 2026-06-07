#pragma once

#include "CoreMinimal.h"

/**
 * GAS input IDs assigned to ability specs at grant time (GiveDefaultAbilities).
 *
 * Only abilities that need the ASC's input-press plumbing get an ID. The melee combo uses it:
 * AbilityLocalInputPressed(Attack) activates the spec when idle, and while the ability is active
 * it raises the GAS generic replicated InputPressed event (client -> ServerSetReplicatedEvent),
 * which the ability's WaitInputPress task consumes as the combo input buffer on BOTH ends.
 * Tag-only activation (TryActivateAbilitiesByTag) has no such replicated re-press channel.
 */
enum class EMAAbilityInputID : int32
{
	None = -1,
	Attack = 1,
};
