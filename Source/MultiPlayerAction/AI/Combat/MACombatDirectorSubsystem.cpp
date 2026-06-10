#include "AI/Combat/MACombatDirectorSubsystem.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystem/MAGameplayTags.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"

// Single definition point for every ma.AI.* cvar — other files read via FindConsoleVariable.
static TAutoConsoleVariable<int32> CVarMaxAttackers(
	TEXT("ma.AI.MaxAttackers"), 1,
	TEXT("How many enemies may hold an attack token (= swing) simultaneously."));

static TAutoConsoleVariable<float> CVarTokenTTL(
	TEXT("ma.AI.TokenTTL"), 3.0f,
	TEXT("Attack-token lifetime in seconds. Backstop for BT-abort paths that skip the release task."));

static TAutoConsoleVariable<int32> CVarAIDebug(
	TEXT("ma.AI.Debug"), 0,
	TEXT("1 = draw ATK over attack-token holders and BLK over blocking AIs."));

bool UMACombatDirectorSubsystem::ClaimToken(AActor* Holder)
{
	UWorld* W = GetWorld();
	if (!Holder || !W || W->GetNetMode() == NM_Client)
	{
		return false;
	}
	Ledger.SetCapacity(CVarMaxAttackers.GetValueOnGameThread());
	const uint32 Id = Holder->GetUniqueID();
	const bool bGranted = Ledger.Request(Id, W->GetTimeSeconds());
	if (bGranted)
	{
		HolderActors.Add(Id, Holder);
	}
	return bGranted;
}

void UMACombatDirectorSubsystem::ReleaseToken(AActor* Holder)
{
	if (!Holder)
	{
		return;
	}
	const uint32 Id = Holder->GetUniqueID();
	if (Ledger.Release(Id))
	{
		HolderActors.Remove(Id);
	}
}

void UMACombatDirectorSubsystem::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	UWorld* W = GetWorld();
	if (!W || W->GetNetMode() == NM_Client)
	{
		return; // server / standalone only
	}

	const float Now = W->GetTimeSeconds();
	Ledger.SetCapacity(CVarMaxAttackers.GetValueOnGameThread());
	Ledger.ReapExpired(Now, CVarTokenTTL.GetValueOnGameThread());

	const bool bDebug = CVarAIDebug.GetValueOnGameThread() != 0;
	for (auto It = HolderActors.CreateIterator(); It; ++It)
	{
		AActor* Holder = It.Value().Get();

		// Dead or destroyed holders forfeit their token immediately (no TTL wait).
		bool bGone = (Holder == nullptr);
		if (Holder)
		{
			if (const UAbilitySystemComponent* ASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Holder))
			{
				bGone = ASC->HasMatchingGameplayTag(MAGameplayTags::State_Dead);
			}
		}
		if (bGone)
		{
			Ledger.Release(It.Key());
			It.RemoveCurrent();
			continue;
		}

		// Drop map entries whose grant the TTL reaper already removed.
		if (!Ledger.Holds(It.Key()))
		{
			It.RemoveCurrent();
			continue;
		}

		if (bDebug)
		{
			DrawDebugString(W, Holder->GetActorLocation() + FVector(0.f, 0.f, 120.f),
				TEXT("ATK"), nullptr, FColor::Red, 0.f, /*bDrawShadow=*/true);
		}
	}
}

TStatId UMACombatDirectorSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UMACombatDirectorSubsystem, STATGROUP_Tickables);
}

bool UMACombatDirectorSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}
