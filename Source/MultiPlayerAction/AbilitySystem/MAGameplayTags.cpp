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

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Attacking, "State.Attacking",
		"ActivationOwnedTags during the melee swing (UGA_MeleeAttack); with State.Casting it gates the AnimInstance upper-body aim twist");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Event_Montage_ComboWindow, "Event.Montage.ComboWindow",
		"GameplayEvent sent from AnimNotify near each combo section's end on AM_MeleeCombo — UGA_MeleeAttack jumps to the next section here if an attack input was buffered");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Event_Damage_Taken, "Event.Damage.Taken",
		"Raised server-side by UMAAttributeSet::PostGameplayEffectExecute when damage lands on a living target — AbilityTriggers entry on UGA_HitReact");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Ability_Reaction_HitReact, "Ability.Reaction.HitReact",
		"Asset tag for UGA_HitReact — ServerInitiated flinch montage triggered by Event.Damage.Taken");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Ability_Defense_Block, "Ability.Defense.Block",
		"Asset tag for UGA_Block — hold-style guard; input release cancels via CancelAbilities");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Blocking, "State.Blocking",
		"ActivationOwnedTags while UGA_Block holds: ExecCalc mitigates damage, GA_HitReact is suppressed, melee activation is blocked");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Armed, "State.Armed",
		"持刀状态（复制 loose tag）：刀系 GA（连招/突进斩/格挡）的 ActivationRequiredTags；拾取武器时服务器授予，AI 出生自带");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Ability_Melee_DashSlash, "Ability.Melee.DashSlash",
		"Asset tag for UGA_DashSlash — Motion-Warped root-motion dash attack (Q / gamepad Y)");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Data_DamageMultiplier, "Data.DamageMultiplier",
		"SetByCaller key: per-ability damage multiplier read from Lua config (default 1.0). Consumed by MADamageExecutionCalculation.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Ability_Cooldown_DashSlash, "Ability.Cooldown.DashSlash",
		"Granted by BP_GE_Cooldown_DashSlash while the dash slash is on cooldown");
}
