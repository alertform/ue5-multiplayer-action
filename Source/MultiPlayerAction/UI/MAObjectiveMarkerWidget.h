#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "MAObjectiveMarkerWidget.generated.h"

class UTextBlock;
class UVerticalBox;

/**
 * 任务导航：屏幕空间目标指引。客户端本地解析"当前目标"——
 *   - 任务进行中 → 最近的存活敌人（剧情模式竞技场里的假人皆为任务敌人）
 *   - 无任务 / 已完成 → 云游剑客 NPC（离场隐身时不指）
 * 目标在屏内 → 头顶浮标 + 名称 + 距离；出屏 / 相机背后 → 钳到屏幕边缘变方向箭头。
 * 纯代码构建，无 BP 资产；数据全来自复制到本端的 actor（无需服务器额外下发）。
 */
UCLASS()
class MULTIPLAYERACTION_API UMAObjectiveMarkerWidget : public UUserWidget
{
	GENERATED_BODY()

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
	/** 重扫目标源（敌人 + NPC），节流。 */
	void RefreshSources();

	/** 解析当前应导航到的目标 actor + 展示名；无有效目标返回 null。 */
	AActor* ResolveObjective(const APawn* Pawn, bool bQuestActive, FString& OutName) const;

	UPROPERTY()
	TObjectPtr<UVerticalBox> Container;

	UPROPERTY()
	TObjectPtr<UTextBlock> IconText;

	UPROPERTY()
	TObjectPtr<UTextBlock> LabelText;

	TArray<TWeakObjectPtr<AActor>> EnemySources;
	TArray<TWeakObjectPtr<AActor>> NpcSources;
	float ScanCooldown = 0.f;
};
