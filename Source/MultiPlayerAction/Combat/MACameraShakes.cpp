#include "Combat/MACameraShakes.h"
#include "Shakes/PerlinNoiseCameraShakePattern.h"

// UDefaultCameraShakeBase's ctor creates a UPerlinNoiseCameraShakePattern default subobject
// named "RootShakePattern"; the subclass ctors fetch and parameterize it.
// FPerlinNoiseShaker has a user-defined ctor (not an aggregate) — set members individually.

UMACameraShake_HitLight::UMACameraShake_HitLight(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bSingleInstance = true;

	if (UPerlinNoiseCameraShakePattern* P = Cast<UPerlinNoiseCameraShakePattern>(GetRootShakePattern()))
	{
		P->Duration = 0.2f;
		P->BlendInTime = 0.02f;
		P->BlendOutTime = 0.1f;
		P->Pitch.Amplitude = 0.6f;  P->Pitch.Frequency = 18.f;
		P->Yaw.Amplitude   = 0.4f;  P->Yaw.Frequency   = 18.f;
		P->X.Amplitude     = 2.5f;  P->X.Frequency     = 20.f; // slight push-in along view
	}
}

UMACameraShake_HitHeavy::UMACameraShake_HitHeavy(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bSingleInstance = true;

	if (UPerlinNoiseCameraShakePattern* P = Cast<UPerlinNoiseCameraShakePattern>(GetRootShakePattern()))
	{
		P->Duration = 0.35f;
		P->BlendInTime = 0.02f;
		P->BlendOutTime = 0.18f;
		P->Pitch.Amplitude = 1.5f;  P->Pitch.Frequency = 15.f;
		P->Yaw.Amplitude   = 1.0f;  P->Yaw.Frequency   = 15.f;
		P->Roll.Amplitude  = 0.6f;  P->Roll.Frequency  = 12.f;
		P->X.Amplitude     = 6.f;   P->X.Frequency     = 18.f;
		P->Z.Amplitude     = 3.f;   P->Z.Frequency     = 15.f;
	}
}
