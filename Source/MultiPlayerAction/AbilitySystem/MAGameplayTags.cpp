#include "AbilitySystem/MAGameplayTags.h"

namespace MAGameplayTags
{
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Ability_Melee_Attack, "Ability.Melee.Attack",
		"Activation tag for UGA_MeleeAttack — TryActivateAbilitiesByTag matches against this");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Ability_Cooldown_Melee, "Ability.Cooldown.Melee",
		"Cooldown tag granted by BP_GE_Cooldown_Melee — blocks UGA_MeleeAttack while present");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Event_Montage_Hit, "Event.Montage.Hit",
		"GameplayEvent sent from AnimNotify on AM_MeleeAttack to trigger PerformHitTrace");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(GameplayCue_Melee_Hit, "GameplayCue.Melee.Hit",
		"Hit-impact FX cue executed from UGA_MeleeAttack with FHitResult location/normal — drives BP_GCN_MeleeHit");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Dead, "State.Dead",
		"Loose tag added when Health reaches 0; gate ability activation and dedupe death handling");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Ability_Movement_Sprint, "Ability.Movement.Sprint",
		"Activation + asset tag for UGA_Sprint; GA_MeleeAttack lists this in CancelAbilitiesWithTag to interrupt sprint on attack");
}
