#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "BehaviorTree/BehaviorTreeTypes.h"
#include "GameplayTagContainer.h"
#include "MAAIDefenseComponent.generated.h"

class UAbilitySystemComponent;
class UBlackboardComponent;

/**
 * Reactive guard for AI pawns, fully event-driven (no tick):
 *   Blackboard TargetActor observer -> target-ASC State.Attacking tag listener ->
 *   ShouldReactToAttack (pure, unit-tested) -> human-like delay -> TryActivate GA_Block ->
 *   timed CancelAbilities (the AI mirror of the player's input release).
 * Lives on AMAEnemyController; the controller calls BindToBlackboard after RunBehaviorTree
 * and UnbindAll on unpossess (deterministic — no BeginPlay races with the BT).
 */
UCLASS()
class MULTIPLAYERACTION_API UMAAIDefenseComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	/** Call after RunBehaviorTree: registers the TargetActor key observer and binds the current target. */
	void BindToBlackboard();

	/** Unbinds the BB observer, the target ASC delegate and all timers. Safe to call repeatedly. */
	void UnbindAll();

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** Blackboard key holding the current target (matches BTService_UpdateTargetInfo's key). */
	UPROPERTY(EditDefaultsOnly, Category = "Defense")
	FName TargetActorKeyName = TEXT("TargetActor");

	/** Only react to swings starting within this range. */
	UPROPERTY(EditDefaultsOnly, Category = "Defense")
	float ReactRangeCm = 350.f;

	/** Probability of reacting to an observed swing start. */
	UPROPERTY(EditDefaultsOnly, Category = "Defense", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float ReactChance = 0.6f;

	/** Minimum spacing between reactions. */
	UPROPERTY(EditDefaultsOnly, Category = "Defense")
	float ReactionCooldownSeconds = 3.f;

	/** Beat between seeing the wind-up and raising the guard. */
	UPROPERTY(EditDefaultsOnly, Category = "Defense")
	float ReactionDelaySeconds = 0.2f;

	/** How long the guard is held before the timed release. */
	UPROPERTY(EditDefaultsOnly, Category = "Defense")
	float BlockHoldSeconds = 1.2f;

private:
	EBlackboardNotificationResult OnTargetKeyChanged(const UBlackboardComponent& BB, FBlackboard::FKey Key);
	void BindTargetASC(AActor* NewTarget);
	void OnTargetAttackingChanged(const FGameplayTag Tag, int32 NewCount);
	void StartBlock();
	void StopBlock();

	APawn* GetControlledPawn() const;
	UAbilitySystemComponent* GetOwnASC() const;

	TWeakObjectPtr<UAbilitySystemComponent> TargetASC;
	FDelegateHandle TargetTagHandle;
	float LastReactionTime = -1000.f;
	FTimerHandle ReactionDelayTimer;
	FTimerHandle BlockHoldTimer;
};
