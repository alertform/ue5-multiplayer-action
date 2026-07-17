#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "MALLMSettings.generated.h"

/**
 * LLM NPC 对话的端点/采样/流式配置 —— Project Settings → Game → LLM Dialogue (Kimi)。
 * 落盘 Config/DefaultGame.ini。默认对准 Moonshot(Kimi)，但任何 OpenAI 兼容端点
 * （DeepSeek 等）只需改 BaseUrl/Model/ApiKeyEnvVar 即可切换。
 *
 * API key 永远不进配置文件 —— 只从 server 机器的环境变量读取。
 */
UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "LLM Dialogue (Kimi)"))
class MULTIPLAYERACTION_API UMALLMSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	/** OpenAI 兼容 API 根地址（不带末尾斜杠；请求打到 <BaseUrl>/chat/completions）。 */
	UPROPERTY(EditAnywhere, Config, Category = "Endpoint")
	FString BaseUrl = TEXT("https://api.moonshot.cn/v1");

	/** 模型 ID。kimi-latest 恒指向最新 Kimi；可换 kimi-k2 系列或其他兼容端点的模型。 */
	UPROPERTY(EditAnywhere, Config, Category = "Endpoint")
	FString Model = TEXT("kimi-latest");

	/** 存放 API key 的环境变量名（server 机器上设置，绝不写进任何文件）。 */
	UPROPERTY(EditAnywhere, Config, Category = "Endpoint")
	FString ApiKeyEnvVar = TEXT("MOONSHOT_API_KEY");

	/** Kimi K2 官方推荐 0.6。 */
	UPROPERTY(EditAnywhere, Config, Category = "Sampling", meta = (ClampMin = "0.0", ClampMax = "2.0"))
	float Temperature = 0.6f;

	/** 单次回复 token 上限（NPC 台词短，无需大值）。 */
	UPROPERTY(EditAnywhere, Config, Category = "Sampling", meta = (ClampMin = "16", ClampMax = "8192"))
	int32 MaxTokens = 512;

	/** 整个请求的总超时（流式生成可能较长）。 */
	UPROPERTY(EditAnywhere, Config, Category = "Transport", meta = (ClampMin = "10.0"))
	float TotalTimeoutSeconds = 120.f;

	/** 无数据活动超时（卡流判定）。 */
	UPROPERTY(EditAnywhere, Config, Category = "Transport", meta = (ClampMin = "5.0"))
	float ActivityTimeoutSeconds = 30.f;

	/** server→client 增量合批间隔：攒 SSE delta 到该间隔发一次 RPC，防 RPC 风暴。 */
	UPROPERTY(EditAnywhere, Config, Category = "Streaming", meta = (ClampMin = "0.03", ClampMax = "1.0"))
	float DeltaFlushIntervalSeconds = 0.1f;

	/** 会话历史上限（system 之外的消息条数，超出丢最旧的，控 token 成本）。 */
	UPROPERTY(EditAnywhere, Config, Category = "Dialogue", meta = (ClampMin = "2", ClampMax = "100"))
	int32 MaxHistoryMessages = 24;

	/** system prompt 模板；{NpcName} / {Persona} 会被替换为组件上的值。 */
	UPROPERTY(EditAnywhere, Config, Category = "Dialogue", meta = (MultiLine = "true"))
	FString SystemPromptTemplate = TEXT(
		"你是一款多人动作游戏中的NPC「{NpcName}」。你的人设：{Persona}\n"
		"规则：始终用简体中文、口语化地回复，单次不超过80字；"
		"不要使用markdown、列表或表情符号；不要跳出角色；不要提及你是AI或模型。");

	static const UMALLMSettings* Get() { return GetDefault<UMALLMSettings>(); }
};
