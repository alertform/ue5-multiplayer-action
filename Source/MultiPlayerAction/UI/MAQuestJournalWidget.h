#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "UnLuaInterface.h"
#include "MAQuestJournalWidget.generated.h"

class UBorder;
class UTextBlock;
class UVerticalBox;

/**
 * 任务日志面板的 C++ 骨架：只负责布局容器、UnLua 绑定与节流刷新脉冲。
 * 全部界面逻辑（读什么数据、显示什么文案、怎么排行）在 lua 模块
 * Content/Script/UI/QuestJournal.lua —— 改 lua 即改界面行为，不碰 C++/不重编。
 * 分层叙事：C++ = 骨架与权威数据缝；lua = 表现层（业界"UI 走脚本热更"的标准位）。
 */
UCLASS()
class MULTIPLAYERACTION_API UMAQuestJournalWidget : public UUserWidget, public IUnLuaInterface
{
	GENERATED_BODY()

public:
	// IUnLuaInterface —— 绑定的 lua 模块名。
	virtual FString GetModuleName_Implementation() const override { return TEXT("UI.QuestJournal"); }

	/** lua 构件 API：往内容区追加一行文本并返回它（lua 持引用随后 SetText）。 */
	UFUNCTION(BlueprintCallable, Category = "Journal")
	UTextBlock* AddLine(int32 FontSize, FLinearColor Color);

	/** lua 构件 API：清空内容区。 */
	UFUNCTION(BlueprintCallable, Category = "Journal")
	void ClearLines();

	/** 面板显示时触发（C++ 开合调用）；lua 实现刷新体。 */
	UFUNCTION(BlueprintImplementableEvent, Category = "Journal")
	void OnJournalOpened();

	/** 可见期间约 0.25s 一次的刷新脉冲；lua 实现（倒计时/进度实时跳字）。 */
	UFUNCTION(BlueprintImplementableEvent, Category = "Journal")
	void OnJournalRefresh();

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
	UPROPERTY()
	TObjectPtr<UVerticalBox> LinesBox;

	UPROPERTY()
	TObjectPtr<UTextBlock> TitleText;

	float RefreshAccum = 0.f;
};
