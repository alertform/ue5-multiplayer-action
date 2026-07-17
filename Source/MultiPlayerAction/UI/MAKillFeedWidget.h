#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "MAKillFeedWidget.generated.h"

class UTextBlock;
class UVerticalBox;

/**
 * Top-right kill feed: one line per kill ("Killer > Victim", "<victim> died" when
 * unattributed), newest at the bottom, each line fades out after a few seconds.
 * Code-built like the other match widgets; lines arrive via AMAGameState::OnKillEvent
 * (multicast RPC -> local delegate), so the feed works identically on every machine.
 */
UCLASS()
class MULTIPLAYERACTION_API UMAKillFeedWidget : public UUserWidget
{
	GENERATED_BODY()

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual void NativeDestruct() override;

private:
	UPROPERTY()
	TObjectPtr<UVerticalBox> FeedBox;

	struct FFeedLine
	{
		UTextBlock* Text = nullptr;
		float ExpireTime = 0.f;
	};
	TArray<FFeedLine> Lines;

	bool bBoundToGameState = false;

	void HandleKill(const FString& KillerName, const FString& VictimName);
	void HandleTaunt(const FString& Text);

	/** 共用的加行逻辑（封顶+过期由 Tick 统一管理）。 */
	void AddLine(const FString& LineString, const FLinearColor& Color);
};
