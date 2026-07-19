#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "MADialogueWidget.generated.h"

class UTextBlock;
class UScrollBox;
class UEditableTextBox;
class UButton;

/**
 * NPC 对话窗 —— code-built（同 MAScoreboardWidget，无 BP 资产）。
 * 底部居中面板：标题(NPC名)+✕、滚动历史区、状态行、输入框。
 *
 * 流式显示：AppendNpcDelta 把增量追加进"当前 NPC 行"的同一个 TextBlock；
 * FinishNpcLine 封行。网络语义（MessageId 去重、RPC）全在 PlayerController，
 * 本类只管展示。回车提交 → PC->SubmitDialogueText；Esc/✕ → PC->CloseDialogue。
 */
UCLASS()
class MULTIPLAYERACTION_API UMADialogueWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** 重置为与某 NPC 的新对话：清历史区、设标题、显示开场白。 */
	void OpenFor(const FString& NpcName, const FString& Greeting);

	void AppendPlayerLine(const FString& Text);

	/** 发送后、首个增量到达前的等待状态。 */
	void SetWaitingStatus();

	/** 追加一段 NPC 流式增量（当前无进行中 NPC 行则自动起新行）。 */
	void AppendNpcDelta(const FString& Delta);

	/** 封掉当前 NPC 流式行（完成或被打断）。 */
	void FinishNpcLine();

	void ShowError(const FString& Message);

	/** 键盘焦点交给输入框。 */
	void FocusInput();

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
	UPROPERTY()
	TObjectPtr<UTextBlock> TitleText;

	UPROPERTY()
	TObjectPtr<UScrollBox> HistoryBox;

	UPROPERTY()
	TObjectPtr<UTextBlock> StatusText;

	UPROPERTY()
	TObjectPtr<UEditableTextBox> InputBox;

	UPROPERTY()
	TObjectPtr<UButton> CloseButton;

	/** 一键接任务：等价于替玩家说一句求任务话术，走同一条 LLM+校验管线。
	 *  已有进行中任务或等回复时置灰。 */
	UPROPERTY()
	TObjectPtr<UButton> QuestButton;

	UPROPERTY()
	TObjectPtr<UTextBlock> QuestButtonText;

	/** 正在流式追加的 NPC 行；nullptr = 无进行中回复。 */
	UPROPERTY()
	TObjectPtr<UTextBlock> CurrentNpcLine;

	/** CurrentNpcLine 的累计文本（TextBlock 无 append 接口）。 */
	FString CurrentNpcText;

	/** 等待首个增量期间做省略号动画 —— 思考型模型可能十几秒无输出，静态提示会被当成卡死。 */
	bool bWaitingForReply = false;
	float WaitingAnimAccum = 0.f;

	UFUNCTION()
	void OnInputCommitted(const FText& Text, ETextCommit::Type CommitMethod);

	UFUNCTION()
	void OnCloseClicked();

	UFUNCTION()
	void OnQuestClicked();

	UTextBlock* MakeLine(const FLinearColor& Color, int32 FontSize);
	void ScrollToEnd();
};
