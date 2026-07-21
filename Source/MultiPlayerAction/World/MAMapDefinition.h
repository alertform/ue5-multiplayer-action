#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "MAMapDefinition.generated.h"

class UBoxComponent;
class UTexture2D;

/**
 * 每张地图放一个：标定小地图纹理覆盖的世界范围（水平方形），并携带烘焙好的顶视图纹理。
 * 纹理约定（与烘焙脚本一致）：正交相机 Pitch=-90/Yaw=0 俯拍 —— 屏幕上方 = 世界 +X，
 * 屏幕右方 = 世界 +Y。WorldToMapUV 按此把世界坐标映射到 [0,1] UV。
 *
 * 换大场景（World Partition）时：用 WorldPartitionMiniMapBuilder 出图后，把纹理和
 * 覆盖范围填到本 actor 即可，小地图 UI 零改动。
 */
UCLASS()
class MULTIPLAYERACTION_API AMAMapDefinition : public AActor
{
	GENERATED_BODY()

public:
	AMAMapDefinition();

	/** 世界坐标 → 纹理 UV。约定：U 随 +Y 增，V 随 +X 减（屏幕上方=+X）。 */
	FVector2D WorldToMapUV(const FVector& WorldPos) const;

	UTexture2D* GetMinimapTexture() const { return MinimapTexture; }

	/** 覆盖范围世界边长（方形）。 */
	float GetWorldSpan() const;

protected:
	/** 覆盖范围（取 XY 平面；烘焙脚本会把它设成与正交捕捉同尺寸的正方形）。 */
	UPROPERTY(VisibleAnywhere, Category = "Minimap")
	TObjectPtr<UBoxComponent> Bounds;

	/** 烘焙的顶视图。关卡实例上指定，C++ 不持内容引用。 */
	UPROPERTY(EditAnywhere, Category = "Minimap")
	TObjectPtr<UTexture2D> MinimapTexture;
};
