#include "Animation/MAAnimInstance.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystem/MAGameplayTags.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "KismetAnimationLibrary.h"

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

			// 8-way strafe inputs. CalculateDirection gives the signed velocity-vs-facing angle;
			// strafe engages only when moving meaningfully off the facing axis (the locked-on
			// sidestep / backpedal). Both velocity and rotation replicate, so sim proxies match.
			const FVector Velocity = CharacterOwner->GetVelocity();
			Direction = UKismetAnimationLibrary::CalculateDirection(Velocity, CharacterOwner->GetActorRotation());

			// Hysteresis on the gate: once strafing, hold it through a lower exit band so the
			// AnimGraph BlendPosesByBool doesn't dither when speed/angle hover at the entry
			// thresholds (the AnimGraph node's blend time is the primary smoother; this mirrors
			// the aim-twist's FInterpTo and keeps the *choice* of blendspace stable too).
			constexpr float ExitBand = 0.7f; // exit thresholds = entry * 0.7
			const float SpeedGate = StrafeSpeedThreshold * (bStrafing ? ExitBand : 1.f);
			const float DirGate = StrafeDirectionThreshold * (bStrafing ? ExitBand : 1.f);
			bStrafing = Velocity.Size2D() > SpeedGate && FMath::Abs(Direction) > DirGate;
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
