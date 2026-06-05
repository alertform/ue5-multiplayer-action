#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameplayEffectTypes.h"
#include "MAProjectile.generated.h"

class USphereComponent;
class UProjectileMovementComponent;

/**
 * Server-authoritative replicated projectile.
 * Spawned ONLY on the server (by UGA_Fireball); replicates to all clients with movement.
 * On hit: AoE overlap -> apply the injected damage spec to every ASC in radius -> explosion GameplayCue -> Destroy.
 * Visuals (trail particles, mesh) live on BP children — keep this class rendering-free.
 */
UCLASS(Abstract)
class MULTIPLAYERACTION_API AMAProjectile : public AActor
{
	GENERATED_BODY()

public:
	AMAProjectile();

	/**
	 * Server-only injection during deferred spawn (call between SpawnActorDeferred and FinishSpawning).
	 * The damage spec already snapshotted source AttackPower at cast time — the fireball hits with
	 * cast-time stats even if the caster dies mid-flight (PlayerState-owned ASC survives death).
	 */
	void InitProjectile(const FGameplayEffectSpecHandle& InDamageSpec, float InExplosionRadius);

protected:
	virtual void BeginPlay() override;

	/** Collision root — query-only sphere, blocks pawns + world, generates hit events */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Projectile")
	TObjectPtr<USphereComponent> CollisionComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Projectile")
	TObjectPtr<UProjectileMovementComponent> ProjectileMovement;

	UFUNCTION()
	void OnProjectileHit(UPrimitiveComponent* HitComponent, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit);

private:
	/** AoE damage + explosion cue + Destroy. Server only; runs at most once. */
	void Explode(const FVector& Location, const FVector& Normal);

	/** Injected by the ability — exists only on the server, never replicated. */
	FGameplayEffectSpecHandle DamageSpec;

	float ExplosionRadius = 300.f;

	bool bExploded = false;
};
