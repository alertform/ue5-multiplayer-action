#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "MAMinimapWidget.generated.h"

class AMAMapDefinition;
class UBorder;
class UCanvasPanel;
class UImage;
class USizeBox;
class UTextBlock;

/**
 * 右下角窗口小地图：烘焙纹理上的滑动窗口（UV 平移 + 以玩家为轴旋转，玩家朝向恒朝上），
 * 图标层（敌人红 / 对话 NPC 青，NPC 出窗时钳到边缘作方向指示）。
 * 纯代码构建，无 BP / 材质资产；地图纹理与世界范围来自关卡里的 AMAMapDefinition。
 * 窗口世界跨度 cvar：ma.Minimap.ViewSpan。
 */
UCLASS()
class MULTIPLAYERACTION_API UMAMinimapWidget : public UUserWidget
{
	GENERATED_BODY()

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
	void RefreshIconSources();
	UImage* AcquireIcon(int32 Index, const FLinearColor& Color);

	/** 内容盒（根保持可见，Collapsed 根不 tick —— 项目既有教训）。 */
	UPROPERTY()
	TObjectPtr<USizeBox> Frame;

	UPROPERTY()
	TObjectPtr<UBorder> EdgeBorder;

	UPROPERTY()
	TObjectPtr<UCanvasPanel> ClipPanel;

	UPROPERTY()
	TObjectPtr<UImage> MapImage;

	UPROPERTY()
	TObjectPtr<UCanvasPanel> IconCanvas;

	UPROPERTY()
	TObjectPtr<UTextBlock> PlayerMarker;

	/** 图标池（按帧复用，多余的收起）。 */
	UPROPERTY()
	TArray<TObjectPtr<UImage>> IconPool;

	UPROPERTY()
	TWeakObjectPtr<AMAMapDefinition> MapDef;

	/** 材质驱动模式（MapDef 挂了 MinimapMaterial 时启用）：UV 窗口/旋转/圆形遮罩全在
	 *  材质里做，widget 只喂 MID 参数（CenterU/V、ViewScale、Angle）。空则回退方形裁剪。 */
	UPROPERTY()
	TObjectPtr<class UMaterialInstanceDynamic> MapMID;

	/** 图标数据源（节流扫描）：敌人 pawn、对话 NPC、武器拾取物。 */
	TArray<TWeakObjectPtr<AActor>> EnemySources;
	TArray<TWeakObjectPtr<AActor>> NpcSources;
	TArray<TWeakObjectPtr<AActor>> PickupSources;
	float SourceScanCooldown = 0.f;
	bool bMapDefSearched = false;
};
