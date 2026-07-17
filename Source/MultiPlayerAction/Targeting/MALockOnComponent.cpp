#include "Targeting/MALockOnComponent.h"
#include "UI/MALockOnReticleWidget.h"
#include "AbilitySystem/MACombatantInterface.h"
#include "AbilitySystem/MAGameplayTags.h"
#include "AbilitySystemInterface.h"
#include "AbilitySystemComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "MultiPlayerActionCharacter.h"
#include "GameFramework/PlayerController.h"
#include "Blueprint/UserWidget.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"

// File-unique names — adaptive unity build merges .cpps (C2084 lesson).
static const float GLockOnReticleZOrder = 5.f;

UMALockOnComponent::UMALockOnComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	// Camera/UI work — no need to tick before the game is live.
	PrimaryComponentTick.bStartWithTickEnabled = true;
	ReticleWidgetClass = UMALockOnReticleWidget::StaticClass();
}

void UMALockOnComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// Restore orientation flags + tear down the reticle if we die/leave while locked.
	if (IsLocked())
	{
		ClearLock();
	}
	if (Reticle)
	{
		Reticle->RemoveFromParent();
		Reticle = nullptr;
	}
	Super::EndPlay(EndPlayReason);
}

void UMALockOnComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!OwnerIsLocallyControlled())
	{
		return;
	}

	if (CurrentTarget.IsValid())
	{
		if (!IsTargetStillValid())
		{
			ClearLock();
		}
		else
		{
			UpdateCameraToTarget(DeltaTime);
		}
	}
}

void UMALockOnComponent::ToggleLock()
{
	if (!OwnerIsLocallyControlled())
	{
		return;
	}

	if (IsLocked())
	{
		ClearLock();
	}
	else if (AActor* Best = FindBestTarget())
	{
		SetLock(Best);
	}
}

void UMALockOnComponent::SwitchTarget(float DirSign)
{
	if (!IsLocked())
	{
		return;
	}
	if (AActor* Next = FindAdjacentTarget(DirSign))
	{
		AdoptTarget(Next);
	}
}

bool UMALockOnComponent::HandleLookInput(const FVector2D& AxisValue)
{
	if (!IsLocked())
	{
		return false; // free-look as usual
	}

	const float X = AxisValue.X;
	const float AbsX = FMath::Abs(X);
	const float Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;

	if (AbsX <= SwitchReleaseThreshold)
	{
		// 归中区：鼠标增量帧间抖动会频繁掠过这里，所以重新武装要求
		// 连续驻留 SwitchRearmDwell —— 单帧的抖动不再武装切换。
		if (NeutralSince < 0.f)
		{
			NeutralSince = Now;
		}
		if (!bSwitchArmed && (Now - NeutralSince) >= SwitchRearmDwell)
		{
			bSwitchArmed = true;
		}
	}
	else
	{
		NeutralSince = -1.f;
		if (bSwitchArmed && AbsX >= SwitchInputThreshold && (Now - LastSwitchTime) > SwitchCooldown)
		{
			SwitchTarget(X > 0.f ? 1.f : -1.f);
			bSwitchArmed = false;
			LastSwitchTime = Now;
		}
	}

	return true; // consumed: camera stays glued to the target
}

// ---------------------------------------------------------------------------

ACharacter* UMALockOnComponent::GetOwnerCharacter() const
{
	return Cast<ACharacter>(GetOwner());
}

bool UMALockOnComponent::OwnerIsLocallyControlled() const
{
	const ACharacter* C = GetOwnerCharacter();
	return C && C->IsLocallyControlled();
}

void UMALockOnComponent::SetLock(AActor* NewTarget)
{
	if (!NewTarget)
	{
		return;
	}

	// Enter strafe orientation: body faces control rotation (which we aim at the target),
	// movement no longer turns the character toward its velocity. Save to restore on release.
	if (ACharacter* C = GetOwnerCharacter())
	{
		if (UCharacterMovementComponent* Move = C->GetCharacterMovement())
		{
			bSavedOrientToMovement = Move->bOrientRotationToMovement;
			Move->bOrientRotationToMovement = false;
		}
		bSavedUseControllerYaw = C->bUseControllerRotationYaw;
		C->bUseControllerRotationYaw = true;

		// Tell the locomotion layer to switch to the 8-way strafe set (replicates to proxies).
		if (AMultiPlayerActionCharacter* MAC = Cast<AMultiPlayerActionCharacter>(C))
		{
			MAC->SetStrafeMode(true);
		}
	}

	AdoptTarget(NewTarget);
}

void UMALockOnComponent::AdoptTarget(AActor* NewTarget)
{
	CurrentTarget = NewTarget;
	bSwitchArmed = false; // require a re-center before the next switch
	NeutralSince = -1.f;  // 驻留计时随之作废 —— 新目标从零开始积累归中时间
	EnsureReticle();
	if (Reticle)
	{
		Reticle->SetTarget(NewTarget);
	}
}

void UMALockOnComponent::ClearLock()
{
	CurrentTarget = nullptr;

	if (ACharacter* C = GetOwnerCharacter())
	{
		if (UCharacterMovementComponent* Move = C->GetCharacterMovement())
		{
			Move->bOrientRotationToMovement = bSavedOrientToMovement;
		}
		C->bUseControllerRotationYaw = bSavedUseControllerYaw;

		// Back to orient-to-movement free-run locomotion.
		if (AMultiPlayerActionCharacter* MAC = Cast<AMultiPlayerActionCharacter>(C))
		{
			MAC->SetStrafeMode(false);
		}
	}

	if (Reticle)
	{
		Reticle->SetTarget(nullptr);
	}
}

bool UMALockOnComponent::IsTargetStillValid() const
{
	AActor* T = CurrentTarget.Get();
	if (!T || !IsActorAlive(T))
	{
		return false;
	}
	const ACharacter* C = GetOwnerCharacter();
	if (!C)
	{
		return false;
	}
	const float BreakDist = MaxLockDistance + BreakDistancePadding;
	return FVector::DistSquared(C->GetActorLocation(), T->GetActorLocation()) <= FMath::Square(BreakDist);
}

AActor* UMALockOnComponent::FindBestTarget() const
{
	const ACharacter* C = GetOwnerCharacter();
	const APlayerController* PC = C ? Cast<APlayerController>(C->GetController()) : nullptr;
	if (!PC)
	{
		return nullptr;
	}

	FVector ViewLoc;
	FRotator ViewRot;
	PC->GetPlayerViewPoint(ViewLoc, ViewRot);
	const FVector Fwd = ViewRot.Vector();
	const float CosLimit = FMath::Cos(FMath::DegreesToRadians(AcquireHalfAngleDeg));

	TArray<AActor*> Candidates;
	GatherCandidates(Candidates);

	AActor* Best = nullptr;
	float BestScore = TNumericLimits<float>::Max();
	for (AActor* A : Candidates)
	{
		const FVector ToT = GetTargetFocusLocation(A) - ViewLoc;
		const float Dist = ToT.Size();
		if (Dist <= KINDA_SMALL_NUMBER)
		{
			continue;
		}
		const float Cos = FVector::DotProduct(Fwd, ToT / Dist);
		if (Cos < CosLimit)
		{
			continue; // outside acquisition cone
		}
		// Prefer the most centered, then the nearest.
		const float Score = (1.f - Cos) * 1000.f + Dist * 0.05f;
		if (Score < BestScore)
		{
			BestScore = Score;
			Best = A;
		}
	}
	return Best;
}

AActor* UMALockOnComponent::FindAdjacentTarget(float DirSign) const
{
	const ACharacter* C = GetOwnerCharacter();
	const APlayerController* PC = C ? Cast<APlayerController>(C->GetController()) : nullptr;
	AActor* Cur = CurrentTarget.Get();
	if (!PC || !Cur)
	{
		return nullptr;
	}

	FVector ViewLoc;
	FRotator ViewRot;
	PC->GetPlayerViewPoint(ViewLoc, ViewRot);
	const FVector Right = FRotationMatrix(ViewRot).GetUnitAxis(EAxis::Y);

	const FVector CurDir = (GetTargetFocusLocation(Cur) - ViewLoc).GetSafeNormal();
	const float CurRightDot = FVector::DotProduct(Right, CurDir);

	TArray<AActor*> Candidates;
	GatherCandidates(Candidates);

	AActor* Best = nullptr;
	float BestDelta = TNumericLimits<float>::Max();
	for (AActor* A : Candidates)
	{
		if (A == Cur)
		{
			continue;
		}
		const FVector Dir = (GetTargetFocusLocation(A) - ViewLoc).GetSafeNormal();
		// Positive => A sits further toward the requested side than the current target.
		const float Delta = (FVector::DotProduct(Right, Dir) - CurRightDot) * DirSign;
		if (Delta <= KINDA_SMALL_NUMBER)
		{
			continue;
		}
		if (Delta < BestDelta)
		{
			BestDelta = Delta;
			Best = A;
		}
	}
	return Best;
}

void UMALockOnComponent::GatherCandidates(TArray<AActor*>& Out) const
{
	Out.Reset();
	const ACharacter* C = GetOwnerCharacter();
	if (!C)
	{
		return;
	}

	TArray<AActor*> All;
	UGameplayStatics::GetAllActorsWithInterface(this, UMACombatantInterface::StaticClass(), All);

	const FVector Origin = C->GetActorLocation();
	const float MaxDistSq = FMath::Square(MaxLockDistance);
	for (AActor* A : All)
	{
		if (!IsActorLockable(A))
		{
			continue;
		}
		if (FVector::DistSquared(Origin, A->GetActorLocation()) > MaxDistSq)
		{
			continue;
		}
		Out.Add(A);
	}
}

bool UMALockOnComponent::IsActorLockable(const AActor* Actor) const
{
	if (!Actor || Actor == GetOwner())
	{
		return false;
	}
	if (!Actor->Implements<UMACombatantInterface>())
	{
		return false;
	}
	return IsActorAlive(Actor);
}

bool UMALockOnComponent::IsActorAlive(const AActor* Actor)
{
	if (const IAbilitySystemInterface* ASI = Cast<const IAbilitySystemInterface>(Actor))
	{
		if (UAbilitySystemComponent* ASC = const_cast<IAbilitySystemInterface*>(ASI)->GetAbilitySystemComponent())
		{
			return !ASC->HasMatchingGameplayTag(MAGameplayTags::State_Dead);
		}
	}
	return true; // no ASC to consult — not death-trackable, treat as alive
}

FVector UMALockOnComponent::GetTargetFocusLocation(const AActor* Actor) const
{
	return Actor ? Actor->GetActorLocation() + FVector(0.f, 0.f, TargetVerticalOffset) : FVector::ZeroVector;
}

void UMALockOnComponent::UpdateCameraToTarget(float DeltaTime)
{
	ACharacter* C = GetOwnerCharacter();
	AController* Ctrl = C ? C->GetController() : nullptr;
	AActor* T = CurrentTarget.Get();
	if (!Ctrl || !T)
	{
		return;
	}

	// Aim the control rotation from the pawn at the target; the spring arm (pawn-control-rotation)
	// then frames the target ahead with the camera behind.
	const FVector Eye = C->GetActorLocation();
	FRotator Desired = (GetTargetFocusLocation(T) - Eye).Rotation();
	Desired.Pitch = FMath::Clamp(Desired.Pitch + LockedPitchBiasDeg, MinPitchDeg, MaxPitchDeg);
	Desired.Roll = 0.f;

	const FRotator New = FMath::RInterpTo(Ctrl->GetControlRotation(), Desired, DeltaTime, CameraInterpSpeed);
	Ctrl->SetControlRotation(New);
}

void UMALockOnComponent::EnsureReticle()
{
	if (Reticle)
	{
		return;
	}
	const ACharacter* C = GetOwnerCharacter();
	APlayerController* PC = C ? Cast<APlayerController>(C->GetController()) : nullptr;
	if (!PC || !PC->IsLocalController() || !ReticleWidgetClass)
	{
		return;
	}
	Reticle = CreateWidget<UMALockOnReticleWidget>(PC, ReticleWidgetClass);
	if (Reticle)
	{
		Reticle->AddToViewport(static_cast<int32>(GLockOnReticleZOrder));
	}
}
