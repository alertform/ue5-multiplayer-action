#include "Animation/MAAnimInstance.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystem/MAGameplayTags.h"
#include "GameFramework/Pawn.h"

void UMAAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
	Super::NativeUpdateAnimation(DeltaSeconds);

	float TargetYaw = 0.f;

	if (const APawn* PawnOwner = TryGetPawnOwner())
	{
		// ASC lives on the PlayerState for players (character forwards via IAbilitySystemInterface);
		// AI pawns resolve their own ASC the same way. Null ASC (e.g. preview window) -> no twist.
		if (const UAbilitySystemComponent* ASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(
			const_cast<APawn*>(PawnOwner)))
		{
			const bool bAimFacing =
				ASC->HasMatchingGameplayTag(MAGameplayTags::State_Attacking) ||
				ASC->HasMatchingGameplayTag(MAGameplayTags::State_Casting);

			if (bAimFacing)
			{
				// GetBaseAimRotation: controller view for players (replicated to sim proxies via
				// RemoteViewPitch + replicated rotation), actor rotation for AI -> delta 0.
				const float RawDelta = FRotator::NormalizeAxis(
					PawnOwner->GetBaseAimRotation().Yaw - PawnOwner->GetActorRotation().Yaw);
				TargetYaw = FMath::Clamp(RawDelta, -MaxAimYaw, MaxAimYaw);
			}
		}
	}

	CurrentAimYaw = FMath::FInterpTo(CurrentAimYaw, TargetYaw, DeltaSeconds, AimInterpSpeed);
	SpineAimRotation = FRotator(0.f, CurrentAimYaw / FMath::Max(1, NumSpineBones), 0.f);
}
