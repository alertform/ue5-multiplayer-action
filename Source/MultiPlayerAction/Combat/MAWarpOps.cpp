#include "Combat/MAWarpOps.h"

#include "Abilities/GameplayAbility.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystem/MACombatantInterface.h"
#include "AbilitySystem/MAGameplayTags.h"
#include "Combat/MAAttackLunge.h"
#include "EngineUtils.h"
#include "GameFramework/Character.h"
#include "MotionWarpingComponent.h"

void MAWarpOps::SetupWarpTargetForAbility(const UGameplayAbility* Ability, FName WarpTargetName,
	const FMALungeParams& Params, float NoTargetDashCm)
{
	ACharacter* Avatar = Ability ? Cast<ACharacter>(Ability->GetAvatarActorFromActorInfo()) : nullptr;
	UMotionWarpingComponent* Warp = Avatar ? Avatar->FindComponentByClass<UMotionWarpingComponent>() : nullptr;
	if (!Avatar || !Warp)
	{
		return;
	}

	// Candidates: living combatants, not self; AI attackers never snap onto fellow AI
	// (same faction rule as the damage path).
	const APawn* AvatarPawn = Cast<APawn>(Avatar);
	const bool bAttackerIsAI = AvatarPawn && !AvatarPawn->IsPlayerControlled();
	TArray<FVector> Locations;
	for (TActorIterator<APawn> It(Avatar->GetWorld()); It; ++It)
	{
		APawn* Candidate = *It;
		if (Candidate == Avatar || !Candidate->Implements<UMACombatantInterface>())
		{
			continue;
		}
		if (bAttackerIsAI && !Candidate->IsPlayerControlled())
		{
			continue;
		}
		if (const UAbilitySystemComponent* ASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Candidate))
		{
			if (ASC->HasMatchingGameplayTag(MAGameplayTags::State_Dead))
			{
				continue;
			}
		}
		Locations.Add(Candidate->GetActorLocation());
	}

	const float AimYaw = AvatarPawn ? AvatarPawn->GetBaseAimRotation().Yaw : Avatar->GetActorRotation().Yaw;
	const FVector MyPos = Avatar->GetActorLocation();
	const int32 Pick = MAAttackLunge::PickLungeTarget(MyPos, AimYaw, Locations, Params);

	if (Pick != INDEX_NONE)
	{
		const FVector WarpPoint = MAAttackLunge::ComputeWarpPoint(MyPos, Locations[Pick], Params.StopDistanceCm);
		FRotator FaceRot(0.f, AimYaw, 0.f);
		FVector To = Locations[Pick] - MyPos;
		To.Z = 0.f;
		if (To.Normalize())
		{
			FaceRot = To.Rotation();
		}
		Warp->AddOrUpdateWarpTargetFromLocationAndRotation(WarpTargetName, WarpPoint, FaceRot);
	}
	else if (NoTargetDashCm > 0.f)
	{
		// A whiff is a short hop, not the full authored dash.
		const FVector Fwd(FMath::Cos(FMath::DegreesToRadians(AimYaw)), FMath::Sin(FMath::DegreesToRadians(AimYaw)), 0.f);
		const FRotator FaceRot(0.f, AimYaw, 0.f);
		Warp->AddOrUpdateWarpTargetFromLocationAndRotation(WarpTargetName, MyPos + Fwd * NoTargetDashCm, FaceRot);
	}
	else
	{
		// No target and no forced forward warp: clear any stale target so the SkewWarp window
		// passes through and the authored root motion (the swing's step-in) plays unmodified.
		Warp->RemoveWarpTarget(WarpTargetName);
	}
}

void MAWarpOps::ClearWarpTarget(const UGameplayAbility* Ability, FName WarpTargetName)
{
	ACharacter* Avatar = Ability ? Cast<ACharacter>(Ability->GetAvatarActorFromActorInfo()) : nullptr;
	if (UMotionWarpingComponent* Warp = Avatar ? Avatar->FindComponentByClass<UMotionWarpingComponent>() : nullptr)
	{
		Warp->RemoveWarpTarget(WarpTargetName);
	}
}
