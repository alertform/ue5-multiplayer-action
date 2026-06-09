#include "Network/MALagCompGeometry.h"

namespace MALagCompGeometry
{
	float SegmentSegmentDistSq(const FVector& A0, const FVector& A1, const FVector& B0, const FVector& B1)
	{
		// Standard clamped closest-point-between-two-segments (Ericson, Real-Time Collision Detection).
		const FVector D1 = A1 - A0; // direction of segment A
		const FVector D2 = B1 - B0; // direction of segment B
		const FVector R = A0 - B0;
		const float A = FVector::DotProduct(D1, D1); // |D1|^2
		const float E = FVector::DotProduct(D2, D2); // |D2|^2
		const float F = FVector::DotProduct(D2, R);

		float S = 0.f, T = 0.f;
		const float Eps = KINDA_SMALL_NUMBER;

		if (A <= Eps && E <= Eps)
		{
			// Both segments are points.
			return FVector::DistSquared(A0, B0);
		}
		if (A <= Eps)
		{
			// First segment is a point.
			T = FMath::Clamp(F / E, 0.f, 1.f);
		}
		else
		{
			const float C = FVector::DotProduct(D1, R);
			if (E <= Eps)
			{
				// Second segment is a point.
				S = FMath::Clamp(-C / A, 0.f, 1.f);
			}
			else
			{
				const float B = FVector::DotProduct(D1, D2);
				const float Denom = A * E - B * B; // always >= 0
				S = (Denom > Eps) ? FMath::Clamp((B * F - C * E) / Denom, 0.f, 1.f) : 0.f;
				T = (B * S + F) / E;
				if (T < 0.f)      { T = 0.f; S = FMath::Clamp(-C / A, 0.f, 1.f); }
				else if (T > 1.f) { T = 1.f; S = FMath::Clamp((B - C) / A, 0.f, 1.f); }
			}
		}

		const FVector Cp1 = A0 + D1 * S;
		const FVector Cp2 = B0 + D2 * T;
		return FVector::DistSquared(Cp1, Cp2);
	}

	bool SweptSphereVsCapsule(const FVector& Start, const FVector& End, float Rs,
		const FVector& Center, float HalfHeight, float Rc)
	{
		// The capsule's inner segment runs vertically from A to B (sphere-centre endpoints).
		const float SegHalf = FMath::Max(0.f, HalfHeight - Rc);
		const FVector A = Center - FVector(0, 0, SegHalf);
		const FVector B = Center + FVector(0, 0, SegHalf);

		const float Sum = Rc + Rs;
		return SegmentSegmentDistSq(Start, End, A, B) <= Sum * Sum;
	}

	FMACapsuleSnapshot InterpolateSnapshot(const FMACapsuleSnapshot& Older, const FMACapsuleSnapshot& Newer, float T)
	{
		const float Span = Newer.Time - Older.Time;
		const float Alpha = (Span > KINDA_SMALL_NUMBER)
			? FMath::Clamp((T - Older.Time) / Span, 0.f, 1.f)
			: 0.f;

		FMACapsuleSnapshot Out;
		Out.Time = FMath::Lerp(Older.Time, Newer.Time, Alpha);
		Out.Center = FMath::Lerp(Older.Center, Newer.Center, Alpha);
		Out.HalfHeight = FMath::Lerp(Older.HalfHeight, Newer.HalfHeight, Alpha);
		Out.Radius = FMath::Lerp(Older.Radius, Newer.Radius, Alpha);
		return Out;
	}
}
