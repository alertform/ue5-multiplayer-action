#include "AbilitySystem/MAGameplayTags.h"

namespace MAGameplayTags
{
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Ability_Melee_Attack, "Ability.Melee.Attack",
		"Activation tag for UGA_MeleeAttack — TryActivateAbilitiesByTag matches against this");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Ability_Cooldown_Melee, "Ability.Cooldown.Melee",
		"Cooldown tag granted by BP_GE_Cooldown_Melee — blocks UGA_MeleeAttack while present");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Event_Montage_Hit, "Event.Montage.Hit",
		"GameplayEvent sent from AnimNotify on AM_MeleeAttack to trigger PerformHitTrace");
}
