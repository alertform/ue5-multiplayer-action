#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "MATouchControlsWidget.generated.h"

class UButton;
class UCanvasPanel;
class UTextBlock;

/**
 * 触屏操作层（code-built，无 BP 资产）：右下技能簇（攻击/闪避/突进/火球）+
 * 左下长按键（格挡/疾跑）+ 底部中央交互键。移动/视角由引擎默认虚拟摇杆负责
 * （DefaultTouchInterface → 喂现有手柄轴映射），本控件只补按键动作。
 *
 * 按钮直调 Character 的 Touch* 桥接方法（与键鼠/手柄同一套 handler）；对话
 * 打开时战斗按钮静默忽略（对话豁免期不该出招）。SafeZone 包裹防刘海遮挡。
 *
 * 显隐由 AMAPlayerController 依 ma.TouchControls 决定：-1 = 仅触屏平台（默认），
 * 0 = 强制关，1 = 强制开（PC 上调试用鼠标点按验证）。
 */
UCLASS()
class MULTIPLAYERACTION_API UMATouchControlsWidget : public UUserWidget
{
	GENERATED_BODY()

protected:
	virtual void NativeOnInitialized() override;

private:
	UFUNCTION() void OnAttackClicked();
	UFUNCTION() void OnDodgeClicked();
	UFUNCTION() void OnFireballClicked();
	UFUNCTION() void OnDashSlashClicked();
	UFUNCTION() void OnJumpPressed();
	UFUNCTION() void OnJumpReleased();
	UFUNCTION() void OnBlockPressed();
	UFUNCTION() void OnBlockReleased();
	UFUNCTION() void OnSprintPressed();
	UFUNCTION() void OnSprintReleased();
	UFUNCTION() void OnInteractClicked();

	/** 对话打开时战斗输入静默忽略。 */
	bool CombatInputAllowed() const;

	class AMultiPlayerActionCharacter* GetMACharacter() const;

	/** 圆形半透明按钮 + 文字标签，锚定右下（AnchorX/Y ∈ [0,1]，offset 为相对锚点像素）。 */
	UButton* MakeButton(UCanvasPanel* Canvas, const FString& Label, float Size,
		const FVector2D& Anchor, const FVector2D& Offset);
};
