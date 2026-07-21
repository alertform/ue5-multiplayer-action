#include "World/MAMapDefinition.h"

#include "Components/BoxComponent.h"

AMAMapDefinition::AMAMapDefinition()
{
	PrimaryActorTick.bCanEverTick = false;
	SetHidden(true);

	Bounds = CreateDefaultSubobject<UBoxComponent>(TEXT("Bounds"));
	SetRootComponent(Bounds);
	Bounds->SetBoxExtent(FVector(2000.f, 2000.f, 500.f));
	Bounds->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Bounds->SetGenerateOverlapEvents(false);
}

FVector2D AMAMapDefinition::WorldToMapUV(const FVector& WorldPos) const
{
	const FVector Center = GetActorLocation();
	const FVector Extent = Bounds->GetScaledBoxExtent();
	if (Extent.X <= 0.f || Extent.Y <= 0.f)
	{
		return FVector2D(0.5f, 0.5f);
	}
	// 屏幕右 = +Y → U；屏幕上 = +X → V 向下递减。
	const float U = (WorldPos.Y - (Center.Y - Extent.Y)) / (2.f * Extent.Y);
	const float V = ((Center.X + Extent.X) - WorldPos.X) / (2.f * Extent.X);
	return FVector2D(U, V);
}

float AMAMapDefinition::GetWorldSpan() const
{
	const FVector Extent = Bounds->GetScaledBoxExtent();
	return 2.f * FMath::Max(Extent.X, Extent.Y);
}
