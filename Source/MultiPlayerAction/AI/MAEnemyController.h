#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "MAEnemyController.generated.h"

/**
 * Minimal AI controller for melee enemies. On Possess, starts a periodic timer
 * that finds the nearest player, faces them, and tries to activate the melee
 * ability via tag — sharing the SAME UGA_MeleeAttack class with the player.
 *
 * This is the headline GAS proof point: AI and player drive identical
 * GameplayAbility classes through TryActivateAbilitiesByTag, no duplicate code.
 *
 * Upgrade path: replace timer-driven loop with BehaviorTree + custom BTTask
 * that calls TryActivateAbilitiesByTag (Lyra-style).
 */
UCLASS()
class MULTIPLAYERACTION_API AMAEnemyController : public AAIController
{
	GENERATED_BODY()

public:
	virtual void OnPossess(APawn* InPawn) override;
	virtual void OnUnPossess() override;

protected:
	/** How often (seconds) to evaluate the attack condition */
	UPROPERTY(EditDefaultsOnly, Category = "AI")
	float EvalInterval = 1.5f;

	/** Player must be within this many units to trigger an attack */
	UPROPERTY(EditDefaultsOnly, Category = "AI")
	float AttackRange = 250.f;

	FTimerHandle EvalTimer;

	void EvaluateAndAttack();
};
