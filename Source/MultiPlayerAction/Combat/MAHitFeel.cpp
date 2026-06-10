#include "Combat/MAHitFeel.h"
#include "HAL/IConsoleManager.h"

static TAutoConsoleVariable<int32> CVarHitFeel(
	TEXT("ma.HitFeel"), 1,
	TEXT("1 = melee hit-feel feedback (hit stop, camera shake, impact sound, knockback). 0 = baseline (particle cue only)."));

bool MAHitFeel::IsEnabled()
{
	return CVarHitFeel.GetValueOnGameThread() != 0;
}

FMAHitFeelParams MAHitFeel::ComputeHitFeel(float FinalDamage)
{
	if (FinalDamage <= 0.f)
	{
		return FMAHitFeelParams{};
	}
	FMAHitFeelParams Params;
	Params.HitStopSeconds = FMath::Clamp(0.04f + FinalDamage * 0.004f, 0.04f, 0.12f);
	Params.KnockbackImpulse = FMath::Clamp(FinalDamage * 30.f, 60.f, 600.f);
	return Params;
}
