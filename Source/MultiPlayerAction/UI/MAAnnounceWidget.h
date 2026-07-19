#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "MAAnnounceWidget.generated.h"

class UTextBlock;

/**
 * 底部居中叙事字幕条：消费 AMAGameState::OnAnnounceEvent（任务节拍/世界事件）。
 * 新公告替换旧行并重置驻留计时（4.5s 后淡出收起）。纯代码构建，根常显只切内容
 * （Collapsed 根不 tick 的教训）。
 */
UCLASS()
class MULTIPLAYERACTION_API UMAAnnounceWidget : public UUserWidget
{
	GENERATED_BODY()

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual void NativeDestruct() override;

private:
	UPROPERTY()
	TObjectPtr<UTextBlock> LineText;

	void HandleAnnounce(const FString& Text);

	float HoldSeconds = 0.f;
	bool bBoundToGameState = false;
};
