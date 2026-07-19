-- 技能数值配置（UnLua 读取；改完在 PIE 控制台敲 MA.Lua.ReloadAbilityConfig 热重载）
-- 键名一一对应各 GA 的 ConfigKey 与参数名；缺的键 C++ 回退到 UPROPERTY 默认值。
return {
    MeleeAttack = {
        TraceRadius      = 50,
        TraceDistance    = 150,
        MontagePlayRate  = 1.3,   -- 2.0 时挥刀过快失真；1.3 = 爽快与自然的平衡点
        ConeHalfAngleDeg = 35,
        MaxLungeDistanceCm = 350,
        StopDistanceCm   = 120,
        NoTargetDashCm   = 0,   -- 0 = 落空时让原生 1.4-3.4m 步进原样播放
        DamageMultiplier   = 3.0, -- 玩家专用：伤害 = AttackPower × 此系数 × 护甲/格挡减免
        AIDamageMultiplier = 1.0,  -- AI 专用，独立调 —— 改玩家手感不会连带 AI 变强
    },
    Fireball = {
        ExplosionRadius  = 300,
        MontagePlayRate  = 1.0,
        MuzzleSocketName = "hand_r",
        DamageMultiplier = 1.0,
    },
    DashSlash = {
        ConeHalfAngleDeg   = 35,
        MaxLungeDistanceCm = 520,
        StopDistanceCm     = 120,
        NoTargetDashCm     = 250,
        TraceDistance      = 180,
        TraceRadius        = 60,
        DamageMultiplier   = 1.0,
    },
}
