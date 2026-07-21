#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "MAWeaponPickup.generated.h"

class USphereComponent;
class UStaticMeshComponent;

/**
 * 武器拾取物：玩家走入球形范围（server 判定）即持刀 ——
 *  - ASC 授予复制 loose tag `State.Armed`（刀系 GA 的 ActivationRequiredTags；
 *    挂在 PS 的 ASC 上，跨重生存活）
 *  - Character.SetArmed(true) 显示手中刀模
 *  - 自毁（bReplicates，晚加入客户端也看不到已被拾取的刀）
 * 展示网格按实例在关卡里指定（C++ 零内容引用，项目惯例）；旋转为各端本地纯表现。
 */
UCLASS()
class MULTIPLAYERACTION_API AMAWeaponPickup : public AActor
{
	GENERATED_BODY()

public:
	AMAWeaponPickup();

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	UFUNCTION()
	void OnPickupOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep,
		const FHitResult& SweepResult);

	UPROPERTY(VisibleAnywhere, Category = "Pickup")
	TObjectPtr<USphereComponent> Sphere;

	UPROPERTY(VisibleAnywhere, Category = "Pickup")
	TObjectPtr<UStaticMeshComponent> Mesh;

	/** 展示自旋速度（度/秒，本地表现）。 */
	UPROPERTY(EditAnywhere, Category = "Pickup")
	float SpinDegreesPerSecond = 90.f;
};
