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

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Ability_Movement_Dodge, "Ability.Movement.Dodge",
		"Asset tag for UGA_Dodge");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Dodging, "State.Dodging",
		"ActivationOwnedTags during dodge — BP_GE_Damage ApplicationTagRequirements.IgnoreTags includes this to grant i-frame");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Ability_Ranged_Fireball, "Ability.Ranged.Fireball",
		"Activation + asset tag for UGA_Fireball — TryActivateAbilitiesByTag matches against this");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Ability_Cooldown_Fireball, "Ability.Cooldown.Fireball",
		"Cooldown tag granted by BP_GE_Cooldown_Fireball — blocks UGA_Fireball while present");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Ability_Cooldown_Dodge, "Ability.Cooldown.Dodge",
		"Cooldown tag granted by BP_GE_Cooldown_Dodge — blocks UGA_Dodge while present (2s; ends infinite i-frame rolls)");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Event_Montage_SpawnProjectile, "Event.Montage.SpawnProjectile",
		"GameplayEvent sent from AnimNotify on AM_FireballCast's release frame — server spawns the projectile here");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(GameplayCue_Fireball_Explosion, "GameplayCue.Fireball.Explosion",
		"Explosion FX cue executed from AMAProjectile::Explode with impact location — drives BP_GCN_FireballExplosion");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Casting, "State.Casting",
		"ActivationOwnedTags during a rooted cast (UGA_Fireball); dodge/melee/sprint list this in ActivationBlockedTags");
}
