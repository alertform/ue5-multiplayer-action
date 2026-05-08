#pragma once

#include "CoreMinimal.h"
#include "GameplayEffectExecutionCalculation.h"
#include "MADamageExecutionCalculation.generated.h"

/**
 * Damage formula:
 *   FinalDamage = SourceAttackPower * max(0, 1 - TargetArmor * ArmorScale)
 *
 * Output: Damage meta-attribute += FinalDamage on the target. AS PostGameplayEffectExecute
 * routes Damage → Health (Lyra ULyraDamageExecution pattern).
 *
 * Reference in BP_GE_Damage's `Executions` array (replaces the old Health Modifier).
 */
UCLASS()
class MULTIPLAYERACTION_API UMADamageExecutionCalculation : public UGameplayEffectExecutionCalculation
{
	GENERATED_BODY()

public:
	UMADamageExecutionCalculation();

	virtual void Execute_Implementation(
		const FGameplayEffectCustomExecutionParameters& ExecutionParams,
		FGameplayEffectCustomExecutionOutput& OutExecutionOutput) const override;

protected:
	/** Each Armor point reduces incoming damage by this fraction. 0.05 → 5% per point, capped at 1.0 (no negative damage). */
	UPROPERTY(EditDefaultsOnly, Category = "Damage", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float ArmorScale = 0.05f;
};
