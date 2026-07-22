#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "MAMenuHostWidget.generated.h"

class UCommonActivatableWidget;
class UCommonActivatableWidgetStack;

/**
 * 常驻菜单宿主：全屏透明容器 + CommonUI Activatable 栈。
 * 菜单（ESC 等）push 进栈，激活时 CommonUI ActionRouter 自动切 Menu 输入模式 +
 * 光标，出栈自动还原游戏输入 —— 取代 PC 里手工 SetInputMode 的开关舞。
 * 纯代码构建，无 BP / 样式资产。
 */
UCLASS()
class MULTIPLAYERACTION_API UMAMenuHostWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** push 一个菜单（栈内新建实例并激活）。 */
	UCommonActivatableWidget* Push(TSubclassOf<UCommonActivatableWidget> MenuClass);

	/** 栈顶是否有激活中的菜单。 */
	bool HasActive() const;

	/** 关掉栈顶菜单（无则无操作）。 */
	void PopActive();

protected:
	virtual void NativeOnInitialized() override;

private:
	UPROPERTY()
	TObjectPtr<UCommonActivatableWidgetStack> Stack;
};
