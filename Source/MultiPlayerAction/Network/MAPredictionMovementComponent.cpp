#include "Network/MAPredictionMovementComponent.h"

#include "GameFramework/Character.h"
#include "GameFramework/PlayerState.h"
#include "Components/CapsuleComponent.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "DrawDebugHelpers.h"

static TAutoConsoleVariable<int32> CVarPredDebug(
	TEXT("ma.PredDebug"), 0,
	TEXT("1 = show the local player's prediction-correction readout + red/green ghost capsules on each server correction."));

static TAutoConsoleVariable<float> CVarPredDebugGhostSeconds(
	TEXT("ma.PredDebug.GhostSeconds"), 2.0f,
	TEXT("How long the predicted(red)/server(green) ghost capsules persist, seconds."));

void UMAPredictionMovementComponent::OnClientCorrectionReceived(FNetworkPredictionData_Client_Character& ClientData,
	float TimeStamp, FVector NewLocation, FVector NewVelocity, UPrimitiveComponent* NewBase, FName NewBaseBoneName,
	bool bHasBase, bool bBaseRelativePosition, uint8 ServerMovementMode, FVector ServerGravityDirection)
{
	// Capture the client's pre-correction predicted location BEFORE Super snaps it to NewLocation.
	const FVector PredictedLocation = UpdatedComponent ? UpdatedComponent->GetComponentLocation() : NewLocation;

	Super::OnClientCorrectionReceived(ClientData, TimeStamp, NewLocation, NewVelocity, NewBase, NewBaseBoneName,
		bHasBase, bBaseRelativePosition, ServerMovementMode, ServerGravityDirection);

	const float ErrorCm = FVector::Dist(PredictedLocation, NewLocation);
	const float Now = GetWorld() ? GetWorld()->GetTimeSeconds() : TimeStamp;
	CorrectionTracker.Record(Now, ErrorCm);

	if (CVarPredDebug.GetValueOnGameThread() != 0 && GetWorld())
	{
		const float GhostSeconds = CVarPredDebugGhostSeconds.GetValueOnGameThread();
		const UCapsuleComponent* Capsule = CharacterOwner ? CharacterOwner->GetCapsuleComponent() : nullptr;
		const float HalfHeight = Capsule ? Capsule->GetScaledCapsuleHalfHeight() : 88.f;
		const float Radius = Capsule ? Capsule->GetScaledCapsuleRadius() : 34.f;
		DrawDebugCapsule(GetWorld(), PredictedLocation, HalfHeight, Radius, FQuat::Identity, FColor::Red, false, GhostSeconds);
		DrawDebugCapsule(GetWorld(), NewLocation, HalfHeight, Radius, FQuat::Identity, FColor::Green, false, GhostSeconds);
	}
}

void UMAPredictionMovementComponent::TickComponent(float DeltaTime, ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (CVarPredDebug.GetValueOnGameThread() == 0 || !GEngine)
	{
		return;
	}
	if (!CharacterOwner || !CharacterOwner->IsLocallyControlled())
	{
		return; // local player's own HUD only
	}

	const float Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;
	const FMACorrectionStats S = CorrectionTracker.Stats(Now);

	float PingMs = 0.f;
	if (const APlayerState* PS = CharacterOwner->GetPlayerState())
	{
		PingMs = PS->GetPingInMilliseconds();
	}

	const FString Readout = FString::Printf(
		TEXT("[Prediction] Ping: %.0fms | Corrections: %d (%d/s) | Last: %.1fcm | Max: %.1fcm"),
		PingMs, S.TotalCorrections, S.CorrectionsPerSec, S.LastErrorCm, S.MaxErrorCm);

	// Fixed key (refreshes in place each tick); cyan so it stands out from the lag-comp draws.
	// TimeToDisplay must exceed one frame, or the engine expires the message before it renders.
	GEngine->AddOnScreenDebugMessage((uint64)0x4D41505244, FMath::Max(2.f * DeltaTime, 0.1f), FColor::Cyan, Readout);
}
