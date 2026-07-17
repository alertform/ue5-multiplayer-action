#include "Combat/MAProjectile.h"
#include "Components/SphereComponent.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystem/MAGameplayTags.h"
#include "Engine/OverlapResult.h"
#include "DrawDebugHelpers.h"

static TAutoConsoleVariable<int32> CVarFireballDebugExplosion(
	TEXT("ma.Fireball.DebugExplosion"), 0,
	TEXT("1 = draw the AoE damage sphere on each explosion (server viewport, 2s). The VFX size is fixed; this shows the actual gameplay radius (Lua-tunable)."));

AMAProjectile::AMAProjectile()
{
	PrimaryActorTick.bCanEverTick = false;

	// Server spawns; clients receive a replicated proxy with movement replication.
	bReplicates = true;
	SetReplicatingMovement(true);

	// Safety net: a projectile that never hits anything despawns silently (no explosion) — spec'd YAGNI.
	InitialLifeSpan = 5.f;

	CollisionComponent = CreateDefaultSubobject<USphereComponent>(TEXT("Collision"));
	CollisionComponent->InitSphereRadius(15.f);
	CollisionComponent->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	CollisionComponent->SetCollisionObjectType(ECC_WorldDynamic);
	// Detonate ONLY on pawns and world geometry. Ignore-all first so the bolt is itself an
	// ECC_WorldDynamic object that does NOT block other bolts — block-all made two crossing
	// fireballs explode mid-air on each other (review finding).
	CollisionComponent->SetCollisionResponseToAllChannels(ECR_Ignore);
	CollisionComponent->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);
	CollisionComponent->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Block);
	// No bNotifyRigidBodyCollision needed: ProjectileMovement moves via sweeps, and a blocking sweep
	// always broadcasts OnComponentHit through UPrimitiveComponent/AActor::DispatchBlockingHit —
	// that flag only gates physics-simulation contact events, which QueryOnly never generates.
	RootComponent = CollisionComponent;

	ProjectileMovement = CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("ProjectileMovement"));
	ProjectileMovement->UpdatedComponent = CollisionComponent;
	ProjectileMovement->InitialSpeed = 2000.f;
	ProjectileMovement->MaxSpeed = 2000.f;
	ProjectileMovement->ProjectileGravityScale = 0.f; // straight-line bolt
	ProjectileMovement->bRotationFollowsVelocity = true;
}

void AMAProjectile::InitProjectile(const FGameplayEffectSpecHandle& InDamageSpec, float InExplosionRadius)
{
	DamageSpec = InDamageSpec;
	ExplosionRadius = InExplosionRadius;
}

void AMAProjectile::BeginPlay()
{
	Super::BeginPlay();

	// Never collide with the caster. Runs on server AND client proxies — APawn::Instigator replicates,
	// so the client-side simulated projectile also sweeps past its caster instead of jittering on them.
	if (APawn* InstigatorPawn = GetInstigator())
	{
		CollisionComponent->IgnoreActorWhenMoving(InstigatorPawn, true);
	}

	// Authoritative impact handling only — clients just render the replicated flight.
	if (HasAuthority())
	{
		CollisionComponent->OnComponentHit.AddDynamic(this, &AMAProjectile::OnProjectileHit);
	}
}

void AMAProjectile::OnProjectileHit(UPrimitiveComponent* HitComponent, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit)
{
	Explode(Hit.ImpactPoint, Hit.ImpactNormal);
}

void AMAProjectile::Explode(const FVector& Location, const FVector& Normal)
{
	if (bExploded || !HasAuthority())
	{
		return;
	}
	bExploded = true;

	// Source ASC comes from the spec's effect context (set by MakeOutgoingGameplayEffectSpec in the GA).
	// PlayerState-owned ASCs survive pawn death; SourceASC is null only if the caster fully left the game.
	UAbilitySystemComponent* SourceASC = nullptr;
	if (DamageSpec.IsValid())
	{
		SourceASC = DamageSpec.Data->GetContext().GetInstigatorAbilitySystemComponent();
	}

	// AoE: every pawn inside ExplosionRadius takes the (already snapshotted) damage spec.
	FCollisionQueryParams QueryParams;
	QueryParams.AddIgnoredActor(this);
	if (AActor* InstigatorActor = GetInstigator())
	{
		QueryParams.AddIgnoredActor(InstigatorActor); // caster never damages themselves — spec'd
	}

	TArray<FOverlapResult> Overlaps;
	GetWorld()->OverlapMultiByObjectType(
		Overlaps, Location, FQuat::Identity,
		FCollisionObjectQueryParams(ECC_Pawn),
		FCollisionShape::MakeSphere(ExplosionRadius), QueryParams);

#if ENABLE_DRAW_DEBUG
	// 特效大小固定，肉眼看不出 Lua 调过的判定半径——开关打开时把真实 AoE 画出来。
	if (CVarFireballDebugExplosion.GetValueOnGameThread() != 0)
	{
		DrawDebugSphere(GetWorld(), Location, ExplosionRadius, 24, FColor::Orange, false, 2.f);
	}
#endif

	// A pawn can report multiple components — dedupe actors before applying damage.
	TSet<AActor*> DamagedActors;
	for (const FOverlapResult& Overlap : Overlaps)
	{
		AActor* HitActor = Overlap.GetActor();
		if (!HitActor || DamagedActors.Contains(HitActor))
		{
			continue;
		}
		DamagedActors.Add(HitActor);

		UAbilitySystemComponent* TargetASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(HitActor);
		if (!TargetASC || !SourceASC || !DamageSpec.IsValid())
		{
			continue;
		}
		// Apply through the source ASC so instigator/context route correctly (same as melee).
		SourceASC->ApplyGameplayEffectSpecToTarget(*DamageSpec.Data.Get(), TargetASC);
	}

	// Explosion cue replicates through the cue system — fires even on a whiff (no ASC in radius).
	if (SourceASC)
	{
		FGameplayCueParameters CueParams;
		CueParams.Location = Location;
		CueParams.Normal = Normal;
		CueParams.Instigator = GetInstigator();
		CueParams.EffectCauser = this;
		SourceASC->ExecuteGameplayCue(MAGameplayTags::GameplayCue_Fireball_Explosion, CueParams);
	}

	Destroy();
}
