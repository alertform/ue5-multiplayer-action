#pragma once

#include "CoreMinimal.h"

/** One server-frame snapshot of a target's vertical capsule (plain C++ — no reflection needed). */
struct FMACapsuleSnapshot
{
	float   Time = 0.f;                       // server world time, seconds
	FVector Center = FVector::ZeroVector;     // capsule component world location
	float   HalfHeight = 0.f;                 // scaled capsule half height
	float   Radius = 0.f;                     // scaled capsule radius
};

/** A target whose rewound capsule the swept sphere intersected. */
struct FMARewindHit
{
	TWeakObjectPtr<AActor> Actor;
	FVector RewoundCenter = FVector::ZeroVector;
};
