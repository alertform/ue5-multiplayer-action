#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Templates/SubclassOf.h"
#include "MADialogueComponent.generated.h"

class ACharacter;

/**
 * 挂上即成"可对话 NPC"的数据组件 —— 名字、人设、开场白、交互半径全部 per-NPC。
 * 无任何行为逻辑：玩家侧 AMAPlayerController 按 T 搜寻半径内组件；服务器侧
 * UMADialogueSubsystem 用 Persona 组 system prompt 并代理 Kimi 调用。
 *
 * Persona 只在服务器上使用；关卡摆放的 actor 两端都有本组件实例，
 * 客户端读 NpcName/Greeting/InteractRadius 做本地 UI，无需复制。
 */
UCLASS(ClassGroup = (MADialogue), meta = (BlueprintSpawnableComponent))
class MULTIPLAYERACTION_API UMADialogueComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	/** 对话窗标题 & system prompt 中的 {NpcName}。 */
	UPROPERTY(EditAnywhere, Category = "Dialogue")
	FString NpcName = TEXT("云游剑客");

	/** 人设描述，填进 system prompt 的 {Persona}。只在服务器上参与请求。 */
	UPROPERTY(EditAnywhere, Category = "Dialogue", meta = (MultiLine = "true"))
	FString Persona = TEXT(
		"一位在各处战场游历多年的老剑客，见惯生死，说话简短、冷淡但偶尔透出关切。"
		"熟悉这片竞技场的规矩：拾取武士刀、格挡可减伤七成、翻滚有无敌帧。"
		"对愿意讨教的后辈会给出实战建议。");

	/** 开场白 —— 本地即时显示的固定台词，不消耗 LLM 调用。 */
	UPROPERTY(EditAnywhere, Category = "Dialogue")
	FString Greeting = TEXT("站住，旅人。……看你的持刀姿势，是新来的吧？想问什么就问。");

	/** 可发起对话的距离（厘米）；服务器验证时另加 50% 容差。 */
	UPROPERTY(EditAnywhere, Category = "Dialogue", meta = (ClampMin = "100.0", ClampMax = "2000.0"))
	float InteractRadius = 350.f;

	/** 发布任务时在 NPC 周围刷出的敌人类（如 BP_TargetDummy）；None = 不刷怪只发任务。 */
	UPROPERTY(EditAnywhere, Category = "Quest")
	TSubclassOf<ACharacter> QuestEnemyClass;

protected:
	virtual void BeginPlay() override;

private:
	/** 头顶铭牌（名字 + 交互提示）——运行时挂到 owner 上，纯 C++ 无 BP 资产。 */
	UPROPERTY()
	TObjectPtr<class UWidgetComponent> NameplateComponent;
};
