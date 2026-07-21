#include "Items/MAWeaponPickup.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemInterface.h"
#include "AbilitySystem/MAGameplayTags.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "MultiPlayerActionCharacter.h"

AMAWeaponPickup::AMAWeaponPickup()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;
	SetReplicateMovement(false);   // 位置静态，自旋是本地表现

	Sphere = CreateDefaultSubobject<USphereComponent>(TEXT("Sphere"));
	SetRootComponent(Sphere);
	Sphere->SetSphereRadius(120.f);
	Sphere->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Sphere->SetCollisionResponseToAllChannels(ECR_Ignore);
	Sphere->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	Mesh->SetupAttachment(Sphere);
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

void AMAWeaponPickup::BeginPlay()
{
	Super::BeginPlay();
	Sphere->OnComponentBeginOverlap.AddDynamic(this, &AMAWeaponPickup::OnPickupOverlap);
}

void AMAWeaponPickup::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (Mesh)
	{
		Mesh->AddLocalRotation(FRotator(0.f, SpinDegreesPerSecond * DeltaSeconds, 0.f));
	}
}

void AMAWeaponPickup::OnPickupOverlap(UPrimitiveComponent* /*OverlappedComp*/, AActor* OtherActor,
	UPrimitiveComponent* /*OtherComp*/, int32 /*OtherBodyIndex*/, bool /*bFromSweep*/,
	const FHitResult& /*SweepResult*/)
{
	if (!HasAuthority())
	{
		return;
	}
	AMultiPlayerActionCharacter* Character = Cast<AMultiPlayerActionCharacter>(OtherActor);
	const IAbilitySystemInterface* AsInterface = Cast<IAbilitySystemInterface>(OtherActor);
	UAbilitySystemComponent* ASC = AsInterface ? AsInterface->GetAbilitySystemComponent() : nullptr;
	if (!Character || !ASC || ASC->HasMatchingGameplayTag(MAGameplayTags::State_Armed))
	{
		return;
	}
	// server 本地计数 + 复制容器双写（LocalPredicted 技能在客户端也要过 RequiredTags 检查）
	ASC->AddLooseGameplayTag(MAGameplayTags::State_Armed);
	ASC->AddReplicatedLooseGameplayTag(MAGameplayTags::State_Armed);
	Character->SetArmed(true);
	Destroy();
}
