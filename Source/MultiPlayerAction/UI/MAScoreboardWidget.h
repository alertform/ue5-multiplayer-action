#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "MAScoreboardWidget.generated.h"

class UTextBlock;
class UVerticalBox;
class UHorizontalBox;

/**
 * Hold-Tab scoreboard AND post-match results panel — one widget, two modes, decided by
 * the replicated MatchState each refresh (WaitingPostMatch shows the verdict + restart
 * countdown instead of the static title). Code-built like MAMatchStatusWidget.
 *
 * Rows poll GameState->PlayerArray on a short throttle; replicated PlayerState data at
 * this player count needs no event plumbing. Row widgets are pooled — grown on demand,
 * surplus collapsed — so a refresh never reconstructs the tree.
 */
UCLASS()
class MULTIPLAYERACTION_API UMAScoreboardWidget : public UUserWidget
{
	GENERATED_BODY()

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
	UPROPERTY()
	TObjectPtr<UTextBlock> TitleText;

	UPROPERTY()
	TObjectPtr<UTextBlock> SubtitleText;

	UPROPERTY()
	TObjectPtr<UVerticalBox> RowsBox;

	/** One pooled scoreboard line. Raw pointers are GC-safe: every widget here is parented
	 *  into RowsBox, which the UPROPERTY widget tree keeps alive. */
	struct FScoreRow
	{
		UHorizontalBox* Box = nullptr;
		UTextBlock* Rank = nullptr;
		UTextBlock* Name = nullptr;
		UTextBlock* Kills = nullptr;
		UTextBlock* Deaths = nullptr;
		UTextBlock* Ping = nullptr;
	};
	TArray<FScoreRow> Rows;

	/** Starts above the threshold so the first visible tick refreshes immediately. */
	float RefreshAccumulator = 1.f;

	void RefreshBoard();
	FScoreRow& EnsureRow(int32 Index);

	/** One row of 4 column-aligned cells (name fills, numbers fixed-width). */
	UHorizontalBox* MakeRow(FScoreRow& OutRow, int32 FontSize);
	UTextBlock* MakeCell(UHorizontalBox* Box, float FixedWidth, int32 FontSize);
};
