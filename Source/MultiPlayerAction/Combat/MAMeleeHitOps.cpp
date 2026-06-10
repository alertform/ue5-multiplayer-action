#include "Combat/MAMeleeHitOps.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystem/MAGameplayTags.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerState.h"
#include "Network/MALagCompSubsystem.h"

// Mirror of the subsystem's master toggle, read on the game thread.
static bool CVarMeleeLagCompEnabled()
{
	static IConsoleVariable* Var = IConsoleManager::Get().FindConsoleVariable(TEXT("ma.LagComp.Enabled"));
	return Var ? (Var->GetInt() != 0) : true;
}

int32 MAMeleeHitOps::SweepAndApplyMeleeHit(AActor* Avatar, UAbilitySystemComponent* SourceASC,
	const FGameplayEffectSpecHandle& DamageSpecHandle, float TraceDistance, float TraceRadius)
{
	if (!Avatar || !SourceASC || !DamageSpecHandle.IsValid())
	{
		return 0;
	}

	// Aim along camera yaw, not body facing.
	const APawn* AvatarPawn = Cast<APawn>(Avatar);
	const FRotator AimYaw(0.f,
		AvatarPawn ? AvatarPawn->GetBaseAimRotation().Yaw : Avatar->GetActorRotation().Yaw, 0.f);
	const FVector AimDir = AimYaw.Vector();

	const FVector Start = Avatar->GetActorLocation() + AimDir * 50.f;
	const FVector End = Start + AimDir * TraceDistance;

	TArray<TPair<AActor*, FVector>> TargetsHit;

	UWorld* World = Avatar->GetWorld();
	UMALagCompSubsystem* LagComp = World ? World->GetSubsystem<UMALagCompSubsystem>() : nullptr;
	const bool bUseLagComp = LagComp && CVarMeleeLagCompEnabled();

	if (bUseLagComp)
	{
		// Rewind by the attacker's latency. AI pawns (no PlayerState) report ping 0 -> no rewind.
		float PingMs = 0.f;
		if (AvatarPawn)
		{
			if (const APlayerState* PS = AvatarPawn->GetPlayerState())
			{
				PingMs = PS->GetPingInMilliseconds();
			}
		}
		const float RewindAmount = LagComp->ComputeRewindAmount(PingMs);
		const TArray<FMARewindHit> Hits = LagComp->SweepRewound(Start, End, TraceRadius, RewindAmount, Avatar);
		for (const FMARewindHit& Hit : Hits)
		{
			if (AActor* HitActor = Hit.Actor.Get())
			{
				TargetsHit.Emplace(HitActor, Hit.RewoundCenter);
			}
		}
	}
	else
	{
		// Fallback / lag-comp-disabled: sweep live positions (original behavior).
		FCollisionQueryParams QueryParams;
		QueryParams.AddIgnoredActor(Avatar);
		TArray<FHitResult> HitResults;
		const bool bHit = World->SweepMultiByChannel(
			HitResults, Start, End, FQuat::Identity,
			ECC_Pawn, FCollisionShape::MakeSphere(TraceRadius), QueryParams);
		if (bHit)
		{
			for (const FHitResult& Hit : HitResults)
			{
				if (AActor* HitActor = Hit.GetActor())
				{
					TargetsHit.Emplace(HitActor, Hit.ImpactPoint);
				}
			}
		}
	}

	// Unified application: damage GE + hit cue per unique target.
	TSet<AActor*> Applied;
	for (const TPair<AActor*, FVector>& Entry : TargetsHit)
	{
		AActor* HitActor = Entry.Key;
		if (!HitActor || HitActor == Avatar || Applied.Contains(HitActor))
		{
			continue;
		}
		Applied.Add(HitActor);

		// AI faction never damages itself (see GA_MeleeAttack history); players are FFA.
		const APawn* HitPawn = Cast<APawn>(HitActor);
		if (AvatarPawn && !AvatarPawn->IsPlayerControlled() && HitPawn && !HitPawn->IsPlayerControlled())
		{
			continue;
		}

		UAbilitySystemComponent* TargetASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(HitActor);
		if (!TargetASC)
		{
			continue;
		}

		SourceASC->ApplyGameplayEffectSpecToTarget(*DamageSpecHandle.Data.Get(), TargetASC);

		FGameplayCueParameters CueParams;
		CueParams.Location = Entry.Value;
		CueParams.Normal = (Avatar->GetActorLocation() - Entry.Value).GetSafeNormal();
		CueParams.Instigator = Avatar;
		CueParams.EffectCauser = Avatar;
		CueParams.SourceObject = HitActor; // victim — GCN_MeleeImpact freezes/shakes both parties
		SourceASC->ExecuteGameplayCue(MAGameplayTags::GameplayCue_Melee_Hit, CueParams);
	}
	return Applied.Num();
}
