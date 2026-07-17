-- 技能数值配置（UnLua 读取；改完在 PIE 控制台敲 MA.Lua.ReloadAbilityConfig 热重载）
-- 键名一一对应各 GA 的 ConfigKey 与参数名；缺的键 C++ 回退到 UPROPERTY 默认值。
return {
    MeleeAttack = {
        TraceRadius      = 50,
        TraceDistance    = 150,
        MontagePlayRate  = 2.0,
        ConeHalfAngleDeg = 35,
        MaxLungeDistanceCm = 350,
        StopDistanceCm   = 120,
        NoTargetDashCm   = 0,   -- 0 = 落空时让原生 1.4-3.4m 步进原样播放
    },
    Fireball = {
        ExplosionRadius  = 300,
        MontagePlayRate  = 1.0,
        MuzzleSocketName = "hand_r",
    },
    DashSlash = {
        ConeHalfAngleDeg   = 35,
        MaxLungeDistanceCm = 520,
        StopDistanceCm     = 120,
        NoTargetDashCm     = 250,
        TraceDistance      = 180,
        TraceRadius        = 60,
    },
}
