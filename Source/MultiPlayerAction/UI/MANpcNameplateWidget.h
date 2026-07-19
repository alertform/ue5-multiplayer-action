#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "MANpcNameplateWidget.generated.h"

class UTextBlock;

/**
 * NPC 头顶铭牌：名字常显（与 AI 敌人一眼区分），本地玩家进入交互半径时
 * 追加"按 T 对话"提示行。挂在 screen-space UWidgetComponent 上，纯代码构建。
 */
UCLASS()
class MULTIPLAYERACTION_API UMANpcNameplateWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void Setup(const FString& InNpcName, float InInteractRadius, AActor* InNpcActor);

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
	UPROPERTY()
	TObjectPtr<UTextBlock> NameText;

	UPROPERTY()
	TObjectPtr<UTextBlock> HintText;

	TWeakObjectPtr<AActor> NpcActor;
	float InteractRadiusSq = 0.f;
};
