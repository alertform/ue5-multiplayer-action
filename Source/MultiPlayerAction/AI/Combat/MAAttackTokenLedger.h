#pragma once

#include "CoreMinimal.h"

/** One granted attack slot. */
struct FMATokenGrant
{
	uint32 HolderId = 0;
	float GrantTime = 0.f;
};

/**
 * Pure attack-token bookkeeping (no UObject, no World — unit-tested in MAAttackTokenLedgerTest.cpp).
 * Holders are opaque ids (the subsystem maps AActor::GetUniqueID()). Time is always passed in.
 */
class FMAAttackTokenLedger
{
public:
	/** Shrinking below the current holder count keeps existing holders and denies new claims. */
	void SetCapacity(int32 N);

	/** Grants if a slot is free. A current holder re-requesting refreshes its GrantTime and returns true. */
	bool Request(uint32 HolderId, float Now);

	/** Returns true if the holder had a grant. */
	bool Release(uint32 HolderId);

	/** Drops grants older than TtlSeconds — the backstop for BT-abort paths that skip the release task. */
	void ReapExpired(float Now, float TtlSeconds);

	bool Holds(uint32 HolderId) const;
	int32 NumHolders() const;

private:
	int32 Capacity = 1;
	TArray<FMATokenGrant> Grants;

	int32 IndexOf(uint32 HolderId) const;
};
