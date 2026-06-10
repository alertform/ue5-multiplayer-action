#include "AI/Combat/MAAttackTokenLedger.h"

void FMAAttackTokenLedger::SetCapacity(int32 N)
{
	Capacity = FMath::Max(0, N);
}

bool FMAAttackTokenLedger::Request(uint32 HolderId, float Now)
{
	const int32 Idx = IndexOf(HolderId);
	if (Idx != INDEX_NONE)
	{
		Grants[Idx].GrantTime = Now; // refresh — an actively-attacking holder outlives the TTL
		return true;
	}
	if (Grants.Num() >= Capacity)
	{
		return false;
	}
	Grants.Add(FMATokenGrant{ HolderId, Now });
	return true;
}

bool FMAAttackTokenLedger::Release(uint32 HolderId)
{
	const int32 Idx = IndexOf(HolderId);
	if (Idx == INDEX_NONE)
	{
		return false;
	}
	Grants.RemoveAt(Idx);
	return true;
}

void FMAAttackTokenLedger::ReapExpired(float Now, float TtlSeconds)
{
	Grants.RemoveAll([Now, TtlSeconds](const FMATokenGrant& G)
	{
		return Now - G.GrantTime > TtlSeconds;
	});
}

bool FMAAttackTokenLedger::Holds(uint32 HolderId) const
{
	return IndexOf(HolderId) != INDEX_NONE;
}

int32 FMAAttackTokenLedger::NumHolders() const
{
	return Grants.Num();
}

int32 FMAAttackTokenLedger::IndexOf(uint32 HolderId) const
{
	return Grants.IndexOfByPredicate([HolderId](const FMATokenGrant& G)
	{
		return G.HolderId == HolderId;
	});
}
