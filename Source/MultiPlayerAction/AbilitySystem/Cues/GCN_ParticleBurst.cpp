#include "AbilitySystem/Cues/GCN_ParticleBurst.h"
#include "Kismet/GameplayStatics.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "Particles/ParticleSystem.h"

bool UGCN_ParticleBurst::OnExecute_Implementation(AActor* MyTarget, const FGameplayCueParameters& Parameters) const
{
	if (!ParticleTemplate && !NiagaraTemplate)
	{
		return false;
	}

	// Cue executes on every relevant client. EffectCauser (the projectile) may already be
	// destroyed when the cue lands — resolve the world from MyTarget / Instigator instead.
	UWorld* World = MyTarget ? MyTarget->GetWorld() : nullptr;
	if (!World && Parameters.GetInstigator())
	{
		World = Parameters.GetInstigator()->GetWorld();
	}
	if (!World)
	{
		return false;
	}

	const FRotator Rotation = Parameters.Normal.IsNearlyZero() ? FRotator::ZeroRotator : Parameters.Normal.Rotation();
	if (ParticleTemplate)
	{
		UGameplayStatics::SpawnEmitterAtLocation(World, ParticleTemplate, Parameters.Location, Rotation);
	}
	if (NiagaraTemplate)
	{
		UNiagaraFunctionLibrary::SpawnSystemAtLocation(World, NiagaraTemplate, Parameters.Location, Rotation);
	}
	return true;
}
