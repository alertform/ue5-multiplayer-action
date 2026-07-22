#pragma once

#include "CommonActivatableWidget.h"
#include "CoreMinimal.h"
#include "UnLuaInterface.h"
#include "MAEscMenuWidget.generated.h"

class UButton;
class UTextBlock;
class UVerticalBox;

/**
 * ESC 菜单：CommonUI Activatable（由 UMAMenuHostWidget 的栈 push/pop）+ UnLua 内容层。
 *  - 激活时 ActionRouter 自动切 Menu 输入模式 + 光标，反激活自动还原（PC 不再手工
 *    SetInputMode）；ESC / 手柄 B 在 NativeOnKeyDown 里反激活（不依赖 CommonInput
 *    的按键数据表配置）。
 *  - 菜单有哪些项、点了做什么，仍全在 lua 模块 Content/Script/UI/EscMenu.lua。
 *  - 期望焦点 = 第一个按钮 → 手柄方向键可在按钮间导航。
 */
UCLASS()
class MULTIPLAYERACTION_API UMAEscMenuWidget : public UCommonActivatableWidget, public IUnLuaInterface
{
	GENERATED_BODY()

public:
	UMAEscMenuWidget(const FObjectInitializer& ObjectInitializer);

	// IUnLuaInterface
	virtual FString GetModuleName_Implementation() const override { return TEXT("UI.EscMenu"); }

	/** lua 构件 API：追加一个菜单按钮并返回（lua 绑 OnClicked）。 */
	UFUNCTION(BlueprintCallable, Category = "EscMenu")
	UButton* AddButton(const FString& Label);

	/** lua 构件 API：清空菜单项。 */
	UFUNCTION(BlueprintCallable, Category = "EscMenu")
	void ClearItems();

	/** 菜单打开时触发；lua 实现（首开建按钮）。 */
	UFUNCTION(BlueprintImplementableEvent, Category = "EscMenu")
	void OnMenuOpened();

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeOnActivated() override;
	virtual UWidget* NativeGetDesiredFocusTarget() const override;
	virtual TOptional<FUIInputConfig> GetDesiredInputConfig() const override;
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;

private:
	UPROPERTY()
	TObjectPtr<UVerticalBox> ItemsBox;

	UPROPERTY()
	TObjectPtr<UButton> FirstButton;
};
