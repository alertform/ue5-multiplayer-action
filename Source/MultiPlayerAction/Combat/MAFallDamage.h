#pragma once

#include "CoreMinimal.h"

class ACharacter;

/**
 * 掉落伤害：Landed 时按落地竖直速度结算。
 * - 纯函数 Compute 可单测；数值走 cvar（ma.FallDamage.*）便于 PIE 调参。
 * - 经临时 Instant GE 直写 Damage meta 属性 —— 绕过 ExecCalc（护甲/格挡不减摔伤），
 *   仍走 AttributeSet 的 Damage→Health 路由与死亡处理；instigator=自己 → 摔死按自杀
 *   记账（记死亡不记击杀），与现有击杀归因规则一致。
 */
namespace MAFallDamage
{
	/** 落地速度(uu/s,正值) → 伤害。<=SafeSpeed 为 0，超出部分 × DamagePerUnit。 */
	MULTIPLAYERACTION_API float Compute(float FallSpeed, float SafeSpeed, float DamagePerUnit);

	/** server-only；速度取自 Landed 时刻的竖直速度绝对值。内部读 cvar 并应用 GE。 */
	MULTIPLAYERACTION_API void ApplyFallDamage(ACharacter* Victim, float FallSpeed);
}
