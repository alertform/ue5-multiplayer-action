#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "UnLuaInterface.h"
#include "MAEscMenuWidget.generated.h"

class UButton;
class UTextBlock;
class UVerticalBox;

/**
 * ESC 菜单的 C++ 骨架：面板容器 + 按钮构件 API + UnLua 绑定。
 * 菜单有哪些项、点了做什么，全在 lua 模块 Content/Script/UI/EscMenu.lua ——
 * 加/删菜单项改 lua 即可，零重编（与任务日志同一套 C++ 骨架 + lua 逻辑分层）。
 */
UCLASS()
class MULTIPLAYERACTION_API UMAEscMenuWidget : public UUserWidget, public IUnLuaInterface
{
	GENERATED_BODY()

public:
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

private:
	UPROPERTY()
	TObjectPtr<UVerticalBox> ItemsBox;
};
