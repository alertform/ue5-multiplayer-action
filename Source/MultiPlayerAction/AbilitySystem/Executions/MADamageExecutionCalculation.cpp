#include "AbilitySystem/Executions/MADamageExecutionCalculation.h"
#include "AbilitySystem/MAAttributeSet.h"
#include "AbilitySystem/MAGameplayTags.h"
#include "AbilitySystemComponent.h"

/**
 * Static struct of attribute capture defs — created once, used in ctor + Execute.
 * Source's AttackPower is snapshotted at GE application time (so buffs at apply
 * count, but post-apply changes don't retroactively scale damage).
 * Target's Armor is read live (current target armor at execution).
 */
struct FMADamageStatics
{
	DECLARE_ATTRIBUTE_CAPTUREDEF(AttackPower);
	DECLARE_ATTRIBUTE_CAPTUREDEF(Armor);

	FMADamageStatics()
	{
		DEFINE_ATTRIBUTE_CAPTUREDEF(UMAAttributeSet, AttackPower, Source, true);
		DEFINE_ATTRIBUTE_CAPTUREDEF(UMAAttributeSet, Armor, Target, false);
	}
};

static const FMADamageStatics& DamageStatics()
{
	static FMADamageStatics Statics;
	return Statics;
}

UMADamageExecutionCalculation::UMADamageExecutionCalculation()
{
	RelevantAttributesToCapture.Add(DamageStatics().AttackPowerDef);
	RelevantAttributesToCapture.Add(DamageStatics().ArmorDef);
}

void UMADamageExecutionCalculation::Execute_Implementation(
	const FGameplayEffectCustomExecutionParameters& ExecutionParams,
	FGameplayEffectCustomExecutionOutput& OutExecutionOutput) const
{
	const FGameplayEffectSpec& Spec = ExecutionParams.GetOwningSpec();
	FAggregatorEvaluateParameters EvalParams;
	EvalParams.SourceTags = Spec.CapturedSourceTags.GetAggregatedTags();
	EvalParams.TargetTags = Spec.CapturedTargetTags.GetAggregatedTags();

	float AttackPower = 0.f;
	float Armor = 0.f;
	ExecutionParams.AttemptCalculateCapturedAttributeMagnitude(DamageStatics().AttackPowerDef, EvalParams, AttackPower);
	ExecutionParams.AttemptCalculateCapturedAttributeMagnitude(DamageStatics().ArmorDef, EvalParams, Armor);

	// Mitigation factor in [0, 1]: each Armor point reduces damage by ArmorScale fraction
	const float Mitigation = FMath::Clamp(1.f - Armor * ArmorScale, 0.f, 1.f);

	// 每技能伤害系数（GA 从 lua 读出经 SetByCaller 传入；老 spec/未设置 = 1.0）。
	// 负数视为配置错误，钳到 0 防止 lua 填负值把伤害变治疗。
	const float AbilityMultiplier = FMath::Max(0.f, Spec.GetSetByCallerMagnitude(
		MAGameplayTags::Data_DamageMultiplier, /*WarnIfNotFound*/ false, /*DefaultIfNotFound*/ 1.f));

	float FinalDamage = FMath::Max(0.f, AttackPower * AbilityMultiplier * Mitigation);

	// Guard: a blocking target absorbs BlockMitigation of what's left (tags captured from
	// the target ASC at application — State.Blocking is GA_Block's ActivationOwnedTags).
	if (EvalParams.TargetTags && EvalParams.TargetTags->HasTag(MAGameplayTags::State_Blocking))
	{
		FinalDamage *= FMath::Clamp(1.f - BlockMitigation, 0.f, 1.f);
	}

	if (FinalDamage > 0.f)
	{
		OutExecutionOutput.AddOutputModifier(
			FGameplayModifierEvaluatedData(
				UMAAttributeSet::GetDamageAttribute(),
				EGameplayModOp::Additive,
				FinalDamage));
	}
}
