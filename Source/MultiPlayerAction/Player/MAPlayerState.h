#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerState.h"
#include "AbilitySystemInterface.h"
#include "Narrative/MAQuestTypes.h"
#include "MAPlayerState.generated.h"

class UMAAbilitySystemComponent;
class UMAAttributeSet;

UCLASS()
class MULTIPLAYERACTION_API AMAPlayerState : public APlayerState, public IAbilitySystemInterface
{
	GENERATED_BODY()

public:
	AMAPlayerState();

	// IAbilitySystemInterface
	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;

	UMAAttributeSet* GetAttributeSet() const { return AttributeSet; }

	/** Server-only: default abilities + persistent regen GE are granted exactly once per ASC lifetime.
	 *  The ASC survives pawn death/respawn, so the guard must live here, not on the Character. */
	bool HasGrantedStartupAbilities() const { return bStartupAbilitiesGranted; }
	void MarkStartupAbilitiesGranted() { bStartupAbilitiesGranted = true; }

	UFUNCTION(BlueprintPure, Category = "Match")
	int32 GetKills() const { return Kills; }

	UFUNCTION(BlueprintPure, Category = "Match")
	int32 GetDeaths() const { return Deaths; }

	/** Server-only scoreboard mutators — only the GameMode's kill router calls these. */
	void AddKill() { ++Kills; }
	void AddDeath() { ++Deaths; }

	/** 当前叙事任务（复制；HUD 轮询读）。服务器经 Set/GetMutable 写。 */
	const FMAQuestState& GetActiveQuest() const { return ActiveQuest; }
	void SetActiveQuest(const FMAQuestState& InQuest) { ActiveQuest = InQuest; }
	FMAQuestState& GetMutableActiveQuest() { return ActiveQuest; }

	/** 本场已发任务数（server-only 预算账本，不复制）。 */
	int32 GetQuestsIssued() const { return QuestsIssued; }
	void IncrementQuestsIssued() { ++QuestsIssued; }

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:
	// ASC lives on PlayerState for persistence across respawns
	UPROPERTY(VisibleAnywhere, Category = "GAS")
	TObjectPtr<UMAAbilitySystemComponent> AbilitySystemComponent;

	UPROPERTY()
	TObjectPtr<UMAAttributeSet> AttributeSet;

	/** Match-scoped tallies; no reset path needed — the post-match map restart is a full
	 *  (non-seamless) travel that recreates every PlayerState. UI polls, so no OnRep. */
	UPROPERTY(Replicated)
	int32 Kills = 0;

	UPROPERTY(Replicated)
	int32 Deaths = 0;

	/** 叙事任务状态；同 Kills/Deaths，对局重启随 PlayerState 重建自然清零。 */
	UPROPERTY(Replicated)
	FMAQuestState ActiveQuest;

private:
	bool bStartupAbilitiesGranted = false;

	/** 每局 give_quest 预算计数（MAQuestRules::MaxQuestsPerMatch 上限）。 */
	int32 QuestsIssued = 0;
};
