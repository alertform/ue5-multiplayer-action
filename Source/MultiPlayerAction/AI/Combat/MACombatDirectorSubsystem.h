#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "AI/Combat/MAAttackTokenLedger.h"
#include "MACombatDirectorSubsystem.generated.h"

/**
 * Server-side group-combat coordinator: a pool of attack tokens (capacity ma.AI.MaxAttackers)
 * gates which enemies may swing at any moment; the rest circle. Claims come from
 * BTTask_ClaimAttackToken, releases from BTTask_ReleaseAttackToken; the Tick reaps
 * TTL-expired grants (BT-abort leak backstop) and dead/destroyed holders.
 * All bookkeeping lives in FMAAttackTokenLedger (unit-tested). Defines the ma.AI.* cvars.
 */
UCLASS()
class MULTIPLAYERACTION_API UMACombatDirectorSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	// UTickableWorldSubsystem
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;

	/** Claim an attack slot (server-side). A current holder re-claiming refreshes its TTL. */
	bool ClaimToken(AActor* Holder);

	/** Release the holder's slot. No-op if not held. */
	void ReleaseToken(AActor* Holder);

private:
	FMAAttackTokenLedger Ledger;
	TMap<uint32, TWeakObjectPtr<AActor>> HolderActors; // id -> actor, for liveness reaping + debug draw
};
