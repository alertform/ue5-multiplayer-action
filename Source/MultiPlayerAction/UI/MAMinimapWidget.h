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
	UImage* AcquirePathDot(int32 Index);
	/** 高度提示 ▲/▼：目标与玩家 Z 差超阈值时在图标上方显示，返回复用的 TextBlock。 */
	class UTextBlock* AcquireChevron(int32 Index, float DeltaZ, const FLinearColor& Color);

	/** 客户端解析当前任务目标世界坐标（任务进行→最近存活敌人，否则→可见 NPC）。 */
	bool ResolveObjectiveLocation(const APawn* Pawn, FVector& OutLoc) const;

	/** 内容盒（根保持可见，Collapsed 根不 tick —— 项目既有教训）。 */
	UPROPERTY()
	TObjectPtr<USizeBox> Frame;

	UPROPERTY()
	TObjectPtr<UBorder> EdgeBorder;

	UPROPERTY()
	TObjectPtr<UCanvasPanel> ClipPanel;

	UPROPERTY()
	TObjectPtr<UImage> MapImage;

	/** 任务路径面包屑层（在地图之上、图标之下）。 */
	UPROPERTY()
	TObjectPtr<UCanvasPanel> PathCanvas;

	UPROPERTY()
	TObjectPtr<UCanvasPanel> IconCanvas;

	UPROPERTY()
	TObjectPtr<UTextBlock> PlayerMarker;

	/** 图标池（按帧复用，多余的收起）。 */
	UPROPERTY()
	TArray<TObjectPtr<UImage>> IconPool;

	/** 路径面包屑点池。 */
	UPROPERTY()
	TArray<TObjectPtr<UImage>> PathDots;

	/** 高度提示 ▲/▼ 池（跟随敌人/NPC 图标）。 */
	UPROPERTY()
	TArray<TObjectPtr<class UTextBlock>> ChevronPool;

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

	/** 缓存的任务路径世界点（节流寻路，每帧重投影到窗口）。 */
	TArray<FVector> CachedPathPoints;
	float PathRecomputeCooldown = 0.f;
};
