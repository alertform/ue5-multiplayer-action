#include "Network/MALagCompSubsystem.h"

#include "Network/MALagCompGeometry.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/Character.h"
#include "Engine/World.h"
#include "DrawDebugHelpers.h"

static TAutoConsoleVariable<int32> CVarLagCompEnabled(
	TEXT("ma.LagComp.Enabled"), 1,
	TEXT("1 = melee hits rewind targets to the attacker's view; 0 = trace against live positions (old behavior)."));

static TAutoConsoleVariable<int32> CVarLagCompDebug(
	TEXT("ma.LagComp.Debug"), 0,
	TEXT("1 = draw current (red) vs rewound (green) capsule + swept sphere (blue) on each query."));

static TAutoConsoleVariable<float> CVarLagCompMaxHistory(
	TEXT("ma.LagComp.MaxHistorySeconds"), 1.0f,
	TEXT("Seconds of capsule history retained per target."));

static TAutoConsoleVariable<float> CVarLagCompMaxRewind(
	TEXT("ma.LagComp.MaxRewindSeconds"), 0.3f,
	TEXT("Favor-the-shooter cap: server never rewinds further than this many seconds."));

static TAutoConsoleVariable<float> CVarLagCompInterpDelay(
	TEXT("ma.LagComp.InterpolationDelay"), 0.1f,
	TEXT("Client-side proxy interpolation delay added to half-RTT when computing the rewind amount."));

static TAutoConsoleVariable<float> CVarLagCompForcedRewindMs(
	TEXT("ma.LagComp.ForcedRewindMs"), -1.0f,
	TEXT("-1 = derive rewind from ping; >=0 = force this many ms of rewind (debugging without real latency)."));

void UMALagCompSubsystem::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	const UWorld* W = GetWorld();
	if (!W || W->GetNetMode() == NM_Client)
	{
		return; // server / standalone only
	}
	RecordSnapshots(W->GetTimeSeconds());
}

TStatId UMALagCompSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UMALagCompSubsystem, STATGROUP_Tickables);
}

bool UMALagCompSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

void UMALagCompSubsystem::RegisterTarget(AActor* Target)
{
	if (!Target)
	{
		return;
	}
	for (const FTargetHistory& T : Targets)
	{
		if (T.Actor.Get() == Target)
		{
			return; // already registered
		}
	}
	FTargetHistory Entry;
	Entry.Actor = Target;
	Targets.Add(MoveTemp(Entry));
}

void UMALagCompSubsystem::UnregisterTarget(AActor* Target)
{
	Targets.RemoveAll([Target](const FTargetHistory& T)
	{
		return T.Actor.Get() == Target || !T.Actor.IsValid();
	});
}

bool UMALagCompSubsystem::GetCapsule(const AActor* Actor, FVector& OutCenter, float& OutHalfHeight, float& OutRadius)
{
	const ACharacter* Char = Cast<ACharacter>(Actor);
	const UCapsuleComponent* Capsule = Char ? Char->GetCapsuleComponent() : nullptr;
	if (!Capsule)
	{
		return false;
	}
	OutCenter = Capsule->GetComponentLocation();
	OutHalfHeight = Capsule->GetScaledCapsuleHalfHeight();
	OutRadius = Capsule->GetScaledCapsuleRadius();
	return true;
}

void UMALagCompSubsystem::RecordSnapshots(float ServerNow)
{
	const float MaxHistory = CVarLagCompMaxHistory.GetValueOnGameThread();
	const float Cutoff = ServerNow - MaxHistory;

	for (int32 i = Targets.Num() - 1; i >= 0; --i)
	{
		FTargetHistory& T = Targets[i];
		const AActor* Actor = T.Actor.Get();
		if (!Actor)
		{
			Targets.RemoveAt(i);
			continue;
		}

		FMACapsuleSnapshot Snap;
		if (!GetCapsule(Actor, Snap.Center, Snap.HalfHeight, Snap.Radius))
		{
			continue;
		}
		Snap.Time = ServerNow;
		T.Snapshots.Add(Snap);

		// Trim snapshots older than the history window (buffer is time-ascending).
		int32 Trim = 0;
		while (Trim < T.Snapshots.Num() && T.Snapshots[Trim].Time < Cutoff)
		{
			++Trim;
		}
		if (Trim > 0)
		{
			T.Snapshots.RemoveAt(0, Trim, EAllowShrinking::No);
		}
	}
}

float UMALagCompSubsystem::ComputeRewindAmount(float PingMs) const
{
	const float Forced = CVarLagCompForcedRewindMs.GetValueOnGameThread();
	float Amount;
	if (Forced >= 0.f)
	{
		Amount = Forced / 1000.f;
	}
	else
	{
		const float HalfRTT = (PingMs * 0.5f) / 1000.f;
		Amount = HalfRTT + CVarLagCompInterpDelay.GetValueOnGameThread();
	}
	return FMath::Min(Amount, CVarLagCompMaxRewind.GetValueOnGameThread());
}

TArray<FMARewindHit> UMALagCompSubsystem::SweepRewound(const FVector& Start, const FVector& End,
	float SphereRadius, float RewindAmountSeconds, const AActor* Ignored)
{
	TArray<FMARewindHit> Hits;

	const UWorld* W = GetWorld();
	if (!W)
	{
		return Hits;
	}
	const float RewindTime = W->GetTimeSeconds() - FMath::Max(0.f, RewindAmountSeconds);
	const bool bDebug = CVarLagCompDebug.GetValueOnGameThread() != 0;

	if (bDebug)
	{
		DrawDebugLine(W, Start, End, FColor::Blue, false, 2.f, 0, 2.f);
		DrawDebugSphere(W, Start, SphereRadius, 12, FColor::Blue, false, 2.f);
		DrawDebugSphere(W, End, SphereRadius, 12, FColor::Blue, false, 2.f);
	}

	for (const FTargetHistory& T : Targets)
	{
		AActor* Actor = T.Actor.Get();
		if (!Actor || Actor == Ignored)
		{
			continue;
		}

		FVector RewoundCenter;
		const bool bHit = MALagCompGeometry::ResolveRewoundHit(
			T.Snapshots, RewindTime, Start, End, SphereRadius, RewoundCenter);

		if (bDebug && T.Snapshots.Num() > 0)
		{
			const FMACapsuleSnapshot& Latest = T.Snapshots.Last();
			DrawDebugCapsule(W, Latest.Center, Latest.HalfHeight, Latest.Radius,
				FQuat::Identity, FColor::Red, false, 2.f);
			FMACapsuleSnapshot Sampled;
			if (MALagCompGeometry::SampleHistory(T.Snapshots, RewindTime, Sampled))
			{
				DrawDebugCapsule(W, Sampled.Center, Sampled.HalfHeight, Sampled.Radius,
					FQuat::Identity, FColor::Green, false, 2.f);
			}
		}

		if (bHit)
		{
			FMARewindHit Hit;
			Hit.Actor = Actor;
			Hit.RewoundCenter = RewoundCenter;
			Hits.Add(Hit);
		}
	}
	return Hits;
}
