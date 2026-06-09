#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Network/MALagCompTypes.h"
#include "MALagCompSubsystem.generated.h"

/**
 * Server-side lag compensation. Records each registered character's capsule into a
 * per-actor, time-ascending history buffer every server frame; rewinds targets to the
 * attacker's view at melee-hit time. All rewind math lives in MALagCompGeometry (unit-tested).
 * Ticks/records only on authority (NM_Client never records or queries).
 */
UCLASS()
class MULTIPLAYERACTION_API UMALagCompSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	// UTickableWorldSubsystem
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;

	/** Begin/stop recording a target's capsule history (call server-side). */
	void RegisterTarget(AActor* Target);
	void UnregisterTarget(AActor* Target);

	/** Convert an instigator's round-trip ping (ms) into a capped rewind amount in seconds. */
	float ComputeRewindAmount(float PingMs) const;

	/** Resolve a melee swept-sphere against every registered target rewound by RewindAmountSeconds
	 *  into the past. Returns the targets the rewound sphere intersects. */
	TArray<FMARewindHit> SweepRewound(const FVector& Start, const FVector& End, float SphereRadius,
		float RewindAmountSeconds, const AActor* Ignored);

private:
	struct FTargetHistory
	{
		TWeakObjectPtr<AActor> Actor;
		TArray<FMACapsuleSnapshot> Snapshots; // time-ascending
	};
	TArray<FTargetHistory> Targets;

	void RecordSnapshots(float ServerNow);
	static bool GetCapsule(const AActor* Actor, FVector& OutCenter, float& OutHalfHeight, float& OutRadius);
};
