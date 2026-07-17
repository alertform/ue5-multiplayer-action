#pragma once

#include "CoreMinimal.h"

/**
 * LLM 战术指挥官返回的一条决策。字段缺失用"不建议"缺省值表达：
 * MaxAttackers=-1（不改名额）、FocusPlayerName 空（不指定集火）、Taunt 空（不喊话）。
 */
struct FMAAdvisorDecision
{
	int32 MaxAttackers = -1;
	FString FocusPlayerName;
	FString Taunt;
	bool bValid = false;
};

/**
 * 解析 LLM 文本输出为决策。容忍代码围栏与前后废话（取首个 '{' 到末个 '}'）；
 * max_attackers 钳制 [0,3]；taunt 截 60 字符。LLM 是建议者不是执行者 —— 一切
 * 越权输出在这里消毒。纯逻辑；测试：MultiPlayerAction.AICombat.AdvisorDecision
 */
namespace MAAdvisorDecision
{
	MULTIPLAYERACTION_API FMAAdvisorDecision Parse(const FString& LLMText);
}
