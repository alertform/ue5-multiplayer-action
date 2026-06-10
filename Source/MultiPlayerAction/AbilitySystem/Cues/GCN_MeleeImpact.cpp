#include "AbilitySystem/Cues/GCN_MeleeImpact.h"

#include "Combat/MACameraShakes.h"
#include "Combat/MAHitFeel.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"

// The cue notify is a stateless CDO, so per-mesh restore timers live in a file-static
// registry: re-triggering while frozen RESETS the restore (freeze extends), instead of
// the first hit's timer un-freezing a later hit early. Stale (destroyed-mesh) entries
// are swept opportunistically on each apply.
static TMap<TWeakObjectPtr<USkeletalMeshComponent>, FTimerHandle> GHitStopRestoreTimers;

static void ApplyHitStop(UWorld* World, USkeletalMeshComponent* Mesh, float Seconds)
{
	if (!World || !Mesh || Seconds <= 0.f)
	{
		return;
	}

	if (FTimerHandle* Existing = GHitStopRestoreTimers.Find(Mesh))
	{
		World->GetTimerManager().ClearTimer(*Existing);
	}
	for (auto It = GHitStopRestoreTimers.CreateIterator(); It; ++It)
	{
		if (!It.Key().IsValid())
		{
			It.RemoveCurrent();
		}
	}

	Mesh->GlobalAnimRateScale = 0.f;

	FTimerHandle Handle;
	World->GetTimerManager().SetTimer(Handle,
		FTimerDelegate::CreateWeakLambda(Mesh, [Mesh]()
		{
			Mesh->GlobalAnimRateScale = 1.f;
			GHitStopRestoreTimers.Remove(Mesh);
		}),
		Seconds, false);
	GHitStopRestoreTimers.Add(Mesh, Handle);
}

UGCN_MeleeImpact::UGCN_MeleeImpact()
{
	AttackerShake = UMACameraShake_HitLight::StaticClass();
	VictimShake = UMACameraShake_HitHeavy::StaticClass();
}

bool UGCN_MeleeImpact::OnExecute_Implementation(AActor* MyTarget, const FGameplayCueParameters& Parameters) const
{
	// Parent particle burst stays independent of the ma.HitFeel toggle (pre-existing baseline).
	const bool bParticle = Super::OnExecute_Implementation(MyTarget, Parameters);

	if (!MAHitFeel::IsEnabled())
	{
		return bParticle;
	}

	UWorld* World = MyTarget ? MyTarget->GetWorld() : nullptr;
	if (!World && Parameters.GetInstigator())
	{
		World = Parameters.GetInstigator()->GetWorld();
	}
	if (!World)
	{
		return bParticle;
	}

	if (ImpactSound)
	{
		UGameplayStatics::PlaySoundAtLocation(World, ImpactSound, Parameters.Location);
	}

	// Hit stop: both parties freeze. Victim travels in SourceObject (replicated cue param).
	AActor* Attacker = MyTarget;
	const AActor* Victim = Cast<const AActor>(Parameters.SourceObject.Get());
	ApplyHitStop(World, Attacker ? Attacker->FindComponentByClass<USkeletalMeshComponent>() : nullptr, HitStopSeconds);
	ApplyHitStop(World, Victim ? Victim->FindComponentByClass<USkeletalMeshComponent>() : nullptr, HitStopSeconds);

	// Camera shake routed by the LOCAL viewpoint: each machine shakes only its own
	// player's camera (listen-server host and every client evaluate independently).
	for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
	{
		APlayerController* PC = It->Get();
		if (!PC || !PC->IsLocalController())
		{
			continue;
		}
		const APawn* ViewPawn = PC->GetPawn();
		if (ViewPawn == Attacker && AttackerShake)
		{
			PC->ClientStartCameraShake(AttackerShake);
		}
		else if (ViewPawn == Victim && VictimShake)
		{
			PC->ClientStartCameraShake(VictimShake);
		}
	}

	return true;
}
