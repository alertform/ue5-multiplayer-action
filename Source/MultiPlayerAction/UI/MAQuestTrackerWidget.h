#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Narrative/MAQuestTypes.h"
#include "MAQuestTrackerWidget.generated.h"

class UTextBlock;
class UVerticalBox;

/**
 * 右侧任务条：NativeTick 轮询本地 PlayerState 的复制任务状态（项目 UI 惯例：轮询无事件）。
 * 完成/失败后短暂驻留提示再收起。纯代码构建（WidgetTree），无 BP 资产。
 */
UCLASS()
class MULTIPLAYERACTION_API UMAQuestTrackerWidget : public UUserWidget
{
	GENERATED_BODY()

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
	/** 内容盒 —— 显隐开关打在它身上。根必须保持可见：Collapsed 的根不再 tick，
	 *  一旦收起就永远等不到"任务到达"那一帧（首测踩坑实录）。 */
	UPROPERTY()
	TObjectPtr<UVerticalBox> Stack;

	UPROPERTY()
	TObjectPtr<UTextBlock> TitleText;

	UPROPERTY()
	TObjectPtr<UTextBlock> ProgressText;

	/** 终态（完成/失败）驻留计时；<=0 时收起整条。 */
	float TerminalHoldSeconds = 0.f;
	EMAQuestPhase LastPhase = EMAQuestPhase::None;
};
