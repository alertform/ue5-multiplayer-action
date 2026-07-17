#include "Animation/MAAnimInstance.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystem/MAGameplayTags.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "KismetAnimationLibrary.h"
#include "MultiPlayerActionCharacter.h"

void UMAAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
	Super::NativeUpdateAnimation(DeltaSeconds);

	// Full-body montages (DefaultSlot: dash slash, the katana combo set) own the whole pose. They
	// disable BOTH the aim twist (would wrench the torso out of the authored swing) AND the foot-IK
	// ground lock (would glue the swing's authored foot lifts to the floor and slide the planted
	// feet as root motion carries the body forward).
	bFullBodyMontageActive = false;
	if (const UAnimMontage* ActiveMontage = GetCurrentActiveMontage())
	{
		bFullBodyMontageActive = ActiveMontage->IsValidSlot(TEXT("DefaultSlot"));
	}

	float TargetYaw = 0.f;
	bool bFalling = false;

	if (const APawn* PawnOwner = TryGetPawnOwner())
	{
		if (const ACharacter* CharacterOwner = Cast<ACharacter>(PawnOwner))
		{
			if (const UCharacterMovementComponent* Movement = CharacterOwner->GetCharacterMovement())
			{
				bFalling = Movement->IsFalling();
			}

			// 8-way strafe inputs. CalculateDirection gives the signed velocity-vs-facing angle,
			// which drives the strafe blendspace's X axis (0 ahead, ±90 sidestep, ±180 backpedal).
			const FVector Velocity = CharacterOwner->GetVelocity();
			Direction = UKismetAnimationLibrary::CalculateDirection(Velocity, CharacterOwner->GetActorRotation());

			// The strafe set is gated by the replicated lock-on flag, not by geometry: while locked
			// on the body faces the target, so EVERY movement direction (including straight forward)
			// belongs in the 8-way strafe set, with Direction picking the compass clip. The flag
			// replicates (COND_SkipOwner), so simulated proxies strafe in sync. A speed gate with a
			// lower exit band (hysteresis) keeps a locked-on but near-stationary character on the
			// idle/free-run pose instead of dithering into a zero-speed strafe.
			// Players enter the strafe set via the replicated lock-on flag. AI pawns (not
			// AMultiPlayerActionCharacter) face their target through ControlRotation while
			// circling/approaching, so every moving direction belongs in the 8-way set too —
			// without this they slide sideways playing the forward run. Focus-less pathing is
			// unaffected: control rotation follows the path, Direction≈0 -> forward clip.
			const AMultiPlayerActionCharacter* MACharacter = Cast<AMultiPlayerActionCharacter>(PawnOwner);
			const bool bStrafeMode = MACharacter ? MACharacter->IsStrafeMode() : true;
			constexpr float ExitBand = 0.7f; // exit threshold = entry * 0.7
			const float SpeedGate = StrafeSpeedThreshold * (bStrafing ? ExitBand : 1.f);
			bStrafing = bStrafeMode && Velocity.Size2D() > SpeedGate;
		}

		// ASC lives on the PlayerState for players (character forwards via IAbilitySystemInterface);
		// AI pawns resolve their own ASC the same way. Null ASC (e.g. preview window) -> no twist.
		if (const UAbilitySystemComponent* ASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(
			const_cast<APawn*>(PawnOwner)))
		{
			// The twist exists for UPPER-body swings whose legs follow orient-to-movement.
			const bool bAimFacing = !bFullBodyMontageActive &&
				(ASC->HasMatchingGameplayTag(MAGameplayTags::State_Attacking) ||
				 ASC->HasMatchingGameplayTag(MAGameplayTags::State_Casting));

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

	// Foot IK ground-traces only while grounded AND not in a full-body montage.
	bShouldDoFootIKTrace = !bFalling && !bFullBodyMontageActive;

	CurrentAimYaw = FMath::FInterpTo(CurrentAimYaw, TargetYaw, DeltaSeconds, AimInterpSpeed);
	SpineAimRotation = FRotator(0.f, CurrentAimYaw / FMath::Max(1, NumSpineBones), 0.f);
}
