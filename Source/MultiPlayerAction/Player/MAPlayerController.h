#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "MAPlayerController.generated.h"

class UMAUserWidget;
class UMAMatchStatusWidget;
class UMAScoreboardWidget;
class UMAKillFeedWidget;
class UMADialogueWidget;
class UMADialogueComponent;
class UMATouchControlsWidget;

/**
 * Owns local-player UI. Spawns the HUD widget on BeginPlay and binds it to the
 * owning ASC as soon as the PlayerState is available (server: in BeginPlay,
 * client: in OnRep_PlayerState — whichever happens first). Also owns the match
 * overlay (clock/score/respawn) and the Tab scoreboard — both code-built C++
 * widgets spawned straight from their classes, no BP assets involved.
 */
UCLASS()
class MULTIPLAYERACTION_API AMAPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	AMAPlayerController();

	virtual void BeginPlay() override;
	virtual void OnRep_PlayerState() override;

	/** Dev cheat: apply damage to own Health (bypasses GE pipeline) — 控制台输 DamageSelf 10 */
	UFUNCTION(Exec)
	void DamageSelf(float Amount = 10.f);

	/** Server RPC backing DamageSelf — ensures the authoritative ASC is the one mutated */
	UFUNCTION(Server, Reliable)
	void Server_DamageSelf(float Amount);

	/** Server-only: queue Respawn() to fire after Delay seconds */
	void ScheduleRespawn(float Delay);

	/** Show/hide the Tab scoreboard. While WaitingPostMatch it is pinned visible regardless. */
	void SetScoreboardVisible(bool bVisible);

	/** Local-player reaction to the match ending — called on every machine from
	 *  AMAGameState::HandleMatchHasEnded (replication-driven, no extra RPC). */
	void OnLocalMatchEnded();

	/** Server -> owning client: HUD respawn countdown (server world time when respawn fires). */
	UFUNCTION(Client, Reliable)
	void Client_OnRespawnScheduled(float RespawnEndServerTime);

	// ===== NPC LLM 流式对话（server 代理 Kimi，见 UMADialogueSubsystem）=====

	/** 交互输入（Character 的 IA_Interact Started 路由至此）：搜寻半径内最近 NPC，找到即开窗。 */
	void OnInteractPressed();

	/** J 键：任务日志面板开合（widget 骨架 C++，界面逻辑在 lua UI.QuestJournal）。BlueprintCallable 供 lua 菜单调。 */
	UFUNCTION(BlueprintCallable, Category = "UI")
	void ToggleQuestJournal();

	/** Esc 键：游戏内菜单开合（菜单项在 lua UI.EscMenu）。 */
	UFUNCTION(BlueprintCallable, Category = "UI")
	void ToggleEscMenu();

	/** lua 菜单动作：关菜单继续游戏。 */
	UFUNCTION(BlueprintCallable, Category = "UI")
	void CloseEscMenu();

	/** lua 菜单动作：销毁会话并回主菜单（host 离开=全房解散,客户端断连兜底会送回菜单）。 */
	UFUNCTION(BlueprintCallable, Category = "UI")
	void ReturnToMainMenu();

	/** lua 菜单动作：退出游戏进程。 */
	UFUNCTION(BlueprintCallable, Category = "UI")
	void QuitToDesktop();

	/** 触屏控件用：对话打开期间战斗按钮静默忽略。 */
	bool IsDialogueOpen() const { return bDialogueOpen; }

	/** widget → PC：玩家回车提交一条消息（本地上屏 + 发给 server）。 */
	void SubmitDialogueText(const FString& Text);

	/** widget ✕ / Esc → PC：关窗、还原输入模式、通知 server 销毁会话。 */
	void CloseDialogue();

	UFUNCTION(Server, Reliable)
	void Server_StartDialogue(AActor* NpcActor);

	UFUNCTION(Server, Reliable)
	void Server_SendDialogueMessage(const FString& Text);

	UFUNCTION(Server, Reliable)
	void Server_EndDialogue();

	/** server → owning client：合批后的流式增量。MessageId 用于丢弃打断后迟到的增量。 */
	UFUNCTION(Client, Reliable)
	void Client_DialogueDelta(int32 MessageId, const FString& Text);

	UFUNCTION(Client, Reliable)
	void Client_DialogueCompleted(int32 MessageId);

	UFUNCTION(Client, Reliable)
	void Client_DialogueError(const FString& Message);

protected:
	void Respawn();
	FTimerHandle RespawnTimerHandle;

protected:
	/** HUD widget class (set in BP_MAPlayerController defaults to WBP_HUD) */
	UPROPERTY(EditDefaultsOnly, Category = "UI")
	TSubclassOf<UMAUserWidget> HUDWidgetClass;

	UPROPERTY()
	TObjectPtr<UMAUserWidget> HUDWidget;

	/** Match overlay (clock / K-D / respawn countdown). Defaults to the C++ class. */
	UPROPERTY(EditDefaultsOnly, Category = "UI")
	TSubclassOf<UMAMatchStatusWidget> MatchStatusWidgetClass;

	UPROPERTY()
	TObjectPtr<UMAMatchStatusWidget> MatchStatusWidget;

	/** 右侧叙事任务条（轮询 PlayerState 复制状态）。纯 C++ 类直建，无 BP。 */
	UPROPERTY()
	TObjectPtr<class UMAQuestTrackerWidget> QuestTrackerWidget;

	/** 底部叙事字幕条（任务节拍/世界事件公告）。纯 C++ 类直建，无 BP。 */
	UPROPERTY()
	TObjectPtr<class UMAAnnounceWidget> AnnounceWidget;

	/** 右下角小地图（烘焙纹理滑动窗口 + 图标层）。纯 C++ 类直建，无 BP。 */
	UPROPERTY()
	TObjectPtr<class UMAMinimapWidget> MinimapWidget;

	/** 右下角横排圆形技能槽（取代 WBP_HUD 底部中置条）。纯 C++ 类直建，无 BP。 */
	UPROPERTY()
	TObjectPtr<class UMASkillBarWidget> SkillBarWidget;

	/** 任务导航标记（屏幕空间目标指引：屏内浮标 / 出屏边缘箭头）。纯 C++ 类直建，无 BP。 */
	UPROPERTY()
	TObjectPtr<class UMAObjectiveMarkerWidget> ObjectiveMarkerWidget;

	/** 任务日志面板（C++ 骨架 + lua 逻辑）。 */
	UPROPERTY()
	TObjectPtr<class UMAQuestJournalWidget> QuestJournalWidget;

	/** 菜单宿主（CommonUI Activatable 栈）：ESC 菜单 push 进栈，输入模式切换交给 ActionRouter。 */
	UPROPERTY()
	TObjectPtr<class UMAMenuHostWidget> MenuHostWidget;

	virtual void SetupInputComponent() override;

	/** Tab scoreboard / post-match results panel. Defaults to the C++ class. */
	UPROPERTY(EditDefaultsOnly, Category = "UI")
	TSubclassOf<UMAScoreboardWidget> ScoreboardWidgetClass;

	UPROPERTY()
	TObjectPtr<UMAScoreboardWidget> ScoreboardWidget;

	/** Top-right kill feed. Defaults to the C++ class. */
	UPROPERTY(EditDefaultsOnly, Category = "UI")
	TSubclassOf<UMAKillFeedWidget> KillFeedWidgetClass;

	UPROPERTY()
	TObjectPtr<UMAKillFeedWidget> KillFeedWidget;

	/** 触屏操作层（虚拟摇杆之外的按键动作）。显隐由 ma.TouchControls 决定。 */
	UPROPERTY(EditDefaultsOnly, Category = "UI")
	TSubclassOf<UMATouchControlsWidget> TouchControlsWidgetClass;

	UPROPERTY()
	TObjectPtr<UMATouchControlsWidget> TouchControlsWidget;

private:
	/** Idempotent: creates widget if not yet created, then binds to ASC if PS available. */
	void EnsureHUDInitialized();

	// ===== NPC LLM 对话（客户端侧状态）=====

	UMADialogueComponent* FindNearbyDialogueNpc() const;

	UPROPERTY()
	TObjectPtr<UMADialogueWidget> DialogueWidget;

	/** 当前正在流式上屏的服务器消息号（INDEX_NONE = 无）。 */
	int32 ClientDialogueMessageId = INDEX_NONE;

	bool bDialogueOpen = false;
};
