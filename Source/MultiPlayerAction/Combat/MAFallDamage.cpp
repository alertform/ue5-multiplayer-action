#include "Combat/MAFallDamage.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemInterface.h"
#include "AbilitySystem/MAAttributeSet.h"
#include "GameFramework/Character.h"
#include "GameplayEffect.h"

static TAutoConsoleVariable<int32> CVarFallDamageEnabled(
	TEXT("ma.FallDamage.Enabled"), 1, TEXT("Enable fall damage on landing."));
static TAutoConsoleVariable<float> CVarFallDamageSafeSpeed(
	TEXT("ma.FallDamage.SafeSpeed"), 1400.f,
	TEXT("Landing speed (uu/s) below which no fall damage is taken. Full jump lands ~900."));
static TAutoConsoleVariable<float> CVarFallDamagePerUnit(
	TEXT("ma.FallDamage.DamagePerUnit"), 0.05f,
	TEXT("Damage per uu/s above the safe landing speed (0.05: lethal at ~3400)."));

namespace MAFallDamage
{

float Compute(float FallSpeed, float SafeSpeed, float DamagePerUnit)
{
	if (FallSpeed <= SafeSpeed || DamagePerUnit <= 0.f)
	{
		return 0.f;
	}
	return (FallSpeed - SafeSpeed) * DamagePerUnit;
}

void ApplyFallDamage(ACharacter* Victim, float FallSpeed)
{
	if (!Victim || !Victim->HasAuthority() || !CVarFallDamageEnabled.GetValueOnGameThread())
	{
		return;
	}
	const float Damage = Compute(FallSpeed,
		CVarFallDamageSafeSpeed.GetValueOnGameThread(),
		CVarFallDamagePerUnit.GetValueOnGameThread());
	if (Damage <= 0.f)
	{
		return;
	}
	const IAbilitySystemInterface* AsInterface = Cast<IAbilitySystemInterface>(Victim);
	UAbilitySystemComponent* ASC = AsInterface ? AsInterface->GetAbilitySystemComponent() : nullptr;
	if (!ASC)
	{
		return;
	}

	// 临时 Instant GE 直写 Damage meta（叙事奖励 GE 同款构造方式）。
	UGameplayEffect* GE = NewObject<UGameplayEffect>(GetTransientPackage(), TEXT("GE_FallDamage"));
	GE->DurationPolicy = EGameplayEffectDurationType::Instant;
	FGameplayModifierInfo Mod;
	Mod.Attribute = UMAAttributeSet::GetDamageAttribute();
	Mod.ModifierOp = EGameplayModOp::Additive;
	Mod.ModifierMagnitude = FGameplayEffectModifierMagnitude(FScalableFloat(Damage));
	GE->Modifiers.Add(Mod);

	FGameplayEffectContextHandle Ctx = ASC->MakeEffectContext();
	Ctx.AddInstigator(Victim, Victim);   // 自伤：摔死走自杀归因
	ASC->ApplyGameplayEffectToSelf(GE, 1.f, Ctx);
}

}   // namespace MAFallDamage
