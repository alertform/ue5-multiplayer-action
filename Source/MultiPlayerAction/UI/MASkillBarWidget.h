#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "GameplayTagContainer.h"
#include "MASkillBarWidget.generated.h"

class UAbilitySystemComponent;
class UImage;
class UTextBlock;

/**
 * 右下角横排 4 圆形技能槽（近战/翻滚/火球/格挡；冲刺属移动修饰键不占槽）。
 * 纯代码构建：圆形 = RoundedBox 笔刷（零贴图），激活 = 琥珀描边 + 底色提亮，
 * 冷却 = 压暗遮罩 + 剩余秒数。数据源与旧 keycap 槽一致（冷却 GE 查询 + 状态 tag）。
 * 取代 WBP_HUD 里底部中置的 SkillBarCard（该容器已 Collapsed）。
 */
UCLASS()
class MULTIPLAYERACTION_API UMASkillBarWidget : public UUserWidget
{
	GENERATED_BODY()

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
	struct FSlotRuntime
	{
		FGameplayTag CooldownTag;
		FGameplayTag ActiveTag;
		UImage* Circle = nullptr;
		UImage* CooldownDim = nullptr;
		UTextBlock* CooldownText = nullptr;
		bool bActiveVisual = false;
		bool bCoolingVisual = false;
	};

	void ApplyCircleStyle(FSlotRuntime& S, bool bActive);

	TArray<FSlotRuntime> Slots;
	TWeakObjectPtr<UAbilitySystemComponent> ASC;
};
