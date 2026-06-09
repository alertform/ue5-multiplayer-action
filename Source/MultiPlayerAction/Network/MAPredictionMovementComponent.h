#pragma once

#include "CoreMinimal.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Network/MACorrectionTracker.h"
#include "MAPredictionMovementComponent.generated.h"

/**
 * Instrumented CharacterMovementComponent. Captures server reconciliation corrections on the
 * autonomous proxy into FMACorrectionTracker, and (behind ma.PredDebug) shows an on-screen readout
 * plus red(predicted)/green(server) ghost capsules. Observability only — movement is unchanged.
 */
UCLASS()
class MULTIPLAYERACTION_API UMAPredictionMovementComponent : public UCharacterMovementComponent
{
	GENERATED_BODY()

public:
	virtual void TickComponent(float DeltaTime, enum ELevelTick TickType,
		FActorComponentTickFunction* ThisTickFunction) override;

protected:
	virtual void OnClientCorrectionReceived(class FNetworkPredictionData_Client_Character& ClientData, float TimeStamp,
		FVector NewLocation, FVector NewVelocity, UPrimitiveComponent* NewBase, FName NewBaseBoneName,
		bool bHasBase, bool bBaseRelativePosition, uint8 ServerMovementMode, FVector ServerGravityDirection) override;

private:
	FMACorrectionTracker CorrectionTracker;
};
