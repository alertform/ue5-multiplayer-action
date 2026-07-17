#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "MAGameplayAbilityBase.generated.h"

/**
 * Base class for all gameplay abilities in MultiPlayerAction.
 * Provides helper accessors for ASC, Avatar, and AttributeSet.
 */
UCLASS(Abstract)
class MULTIPLAYERACTION_API UMAGameplayAbilityBase : public UGameplayAbility
{
	GENERATED_BODY()

public:
	UMAGameplayAbilityBase();

	/** Input tag used to bind this ability to Enhanced Input */
	UPROPERTY(EditDefaultsOnly, Category = "Input")
	FGameplayTag InputTag;

	/** Lua 配置表里的技能键名（空 = 不走 Lua，纯用 UPROPERTY 默认） */
	UPROPERTY(EditDefaultsOnly, Category = "Config")
	FName ConfigKey;

protected:
	/** Get the avatar actor cast to our character class */
	class AMultiPlayerActionCharacter* GetMACharacter(const FGameplayAbilityActorInfo* ActorInfo) const;

	/** Snap the avatar to the camera's yaw so the action fires where the player is looking.
	 *  Runs on predicting client + server. NOTE: with bOrientRotationToMovement the CMC
	 *  rotates the body back toward the move direction one tick later — for mobile actions
	 *  use BeginAimFacing/EndAimFacing instead so the snap holds for the action's duration. */
	void SnapToAimYaw(const FGameplayAbilityActorInfo* ActorInfo) const;

	/** SnapToAimYaw + disable orient-to-movement so the snapped yaw holds while moving.
	 *  Pair with EndAimFacing in EndAbility. Currently unused — the upper-body aim twist in
	 *  UMAAnimInstance replaced whole-body facing; kept for future full-body-committed actions. */
	void BeginAimFacing(const FGameplayAbilityActorInfo* ActorInfo) const;

	/** Re-enable orient-to-movement disabled by BeginAimFacing. Safe on paths where
	 *  BeginAimFacing never ran (the flag is simply set back to the character default). */
	void EndAimFacing(const FGameplayAbilityActorInfo* ActorInfo) const;

	/** SnapToAimYaw + root (MOVE_None) for the action's duration; pair with EndRootedAction
	 *  in EndAbility. Currently unused (upper-body layering replaced rooting) — kept for
	 *  future committed/channelled actions. */
	void BeginRootedAction(const FGameplayAbilityActorInfo* ActorInfo) const;

	/** Restore MOVE_Walking, but only if still MOVE_None — death ragdoll etc. must not be stomped. */
	void EndRootedAction(const FGameplayAbilityActorInfo* ActorInfo) const;

	/** 从 Lua 配置读 float，缺配置/缺键 → Default。仅读 C++ 缓存，不碰 lua_State。 */
	float ReadConfigFloat(FName Param, float Default) const;
	/** 从 Lua 配置读 FName，缺配置/缺键 → Default。 */
	FName ReadConfigName(FName Param, FName Default) const;
};
