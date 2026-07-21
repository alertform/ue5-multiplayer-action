#pragma once

#include "NativeGameplayTags.h"

/**
 * Native gameplay tags for the MultiPlayerAction module.
 * Use these typed references everywhere — string-based RequestGameplayTag is forbidden once a tag has a native counterpart.
 * Compile-time errors here mean the tag identifier is wrong; gone are the days of silent runtime failures.
 */
namespace MAGameplayTags
{
	// Ability identification tags — used by TryActivateAbilitiesByTag and as AbilityTags on GAs.
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_Melee_Attack);

	// Cooldown tags — granted by Cooldown GE, blocks re-activation while present on the ASC.
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_Cooldown_Melee);

	// Gameplay event tags — sent from AnimNotify -> WaitGameplayEvent task.
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Event_Montage_Hit);

	// GameplayCue tags — engine routes "GameplayCue.*" tags to GameplayCueManager for FX.
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(GameplayCue_Melee_Hit);

	// State tags — applied as loose tags during runtime; gate ability activation, drive UI/AI states.
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Dead);

	// Sprint ability identification — used by GA_MeleeAttack's CancelAbilitiesWithTag to interrupt sprint on attack.
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_Movement_Sprint);

	// Dodge ability + dodge invincibility frame state tag + dodge cooldown.
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_Movement_Dodge);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Dodging);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_Cooldown_Dodge);

	// Fireball ranged ability family — activation tag, cooldown, montage release event, explosion cue.
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_Ranged_Fireball);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_Cooldown_Fireball);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Event_Montage_SpawnProjectile);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(GameplayCue_Fireball_Explosion);

	// Rooted-cast state — owned while a cast roots the character; dodge/melee/sprint block on it
	// (LaunchCharacter is swallowed under MOVE_None; montage interrupts waste committed cost+CD).
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Casting);

	// Melee swing state — owned for the swing's duration; drives the AnimInstance upper-body
	// aim twist (spine chain toward camera yaw) together with State.Casting.
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Attacking);

	// Combo decision point — sent from AnimNotify near each combo section's end; the ability
	// consumes it to decide chain-vs-finish at a deterministic montage frame (client & server agree).
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Event_Montage_ComboWindow);

	// Raised server-side by the AttributeSet when damage lands on a survivor — triggers GA_HitReact.
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Event_Damage_Taken);

	// Asset tag for GA_HitReact (ServerInitiated flinch ability, GameplayEvent-triggered).
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_Reaction_HitReact);

	// Block ability (hold) — asset tag for activation/cancel routing from input.
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_Defense_Block);

	// Owned while blocking: gates damage mitigation in the ExecCalc, suppresses the normal
	// hit react (GA_Block plays Block_Hit instead), and blocks starting an attack.
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Blocking);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Armed);

	// Dash slash — Motion-Warped root-motion katana dash. Asset tag + cooldown.
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_Melee_DashSlash);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_Cooldown_DashSlash);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Data_DamageMultiplier);
}
