# ue5-multiplayer-action

[English](README.md) | **简体中文**

基于 Unreal Engine 5.5 的剧情向多人动作 Demo —— 一个由 **LLM** 扮演任务发布者的武士刀战斗沙盒。一名游荡的剑客 NPC 通过**流式 LLM 对话**（Kimi k2.6，OpenAI 兼容 SSE）与玩家交谈，并通过**工具调用协议**驱动游戏 —— 发布讨伐任务、触发突袭、赐予祝福、记录好感 —— 每一次工具调用在触及玩法之前，都要先通过**服务器权威的校验流水线**。底层是一整套 **Gameplay Ability System**、一套**武士刀战斗组件**（4 段根运动连招，每一刀带 Motion Warping、锁定、格挡/招架、受击硬直、编排死亡动画、Motion Warping 突进斩）、**服务器端延迟补偿**及预测修正指标与打击感处理、由 **BehaviorTree** 驱动的 AI —— 与玩家共用同一批 `GameplayAbility` C++ 类，由服务器攻击令牌调度器和 **LLM 战术顾问**协调 —— 以及**三层 Lua 集成**（C++ 战斗核心 / 热重载 Lua 配置 / 经 UnLua 编写的 Lua UI）。以 **Win64 Shipping pak 包**发布（约 495 MB）。配有 22 个无头 UE Automation 测试。个人作品集项目。

> 本项目与 **UnrealAgentMCP** 一同开发，且大部分内容是**用它**做出来的 —— 那是一个自研的编辑器内 MCP 服务器（77 个工具），让 AI agent 能在运行中的编辑器里直接编写 Blueprint、UMG、AnimGraph、BehaviorTree、Montage、Niagara 系统、材质图（含 HLSL Custom 节点）、IK 重定向以及关卡内容。本项目内容侧的绝大部分工作都是通过它在 agent 侧完成的。该插件在独立的**私有**仓库中开发（可按需提供演示）；构建或运行本项目并不需要它。

---

## 架构

```
                                 SERVER (authority)
                                 ─────────────────
   AMAPlayerState                                          AMATargetDummy (NPC)
     ├─ UMAAbilitySystemComponent  (Mixed replication)       ├─ UMAAbilitySystemComponent  (Minimal replication)
     └─ UMAAttributeSet                                      └─ UMAAttributeSet
            ↑                                                       ↑
     [InitAbilityActorInfo(PS, Pawn)]                        [InitAbilityActorInfo(this, this)]
            ↑                                                       ↑
   AMultiPlayerActionCharacter                              AMATargetDummy
     (cached ASC ptr; no ASC ownership)                       (Pawn = Owner = Avatar)
                                                                    ↑
   AMAPlayerController                                       AMAEnemyController : AAIController
     ├─ Owns all local UI (HUD/dialogue/journal/menus)          └─ RunBehaviorTree(BT_Enemy)
     └─ DamageSelf (Server RPC) for testing                          ├─ Service: BTService_UpdateTargetInfo
                                                                     └─ BTTask_TryActivateAbilityByTag
                                                                            ↓
                                                                     Same UGA_MeleeAttack C++ class as player
```

### 叙事流程（LLM 工具调用，服务器校验）

```
玩家在剑客 NPC 附近按 T（头顶名牌提示可交互）
     ↓
MADialogueComponent（NPC 侧） ←→ MADialogueSubsystem（服务器 —— 持有历史与协议）
     ↓ system prompt 每条消息重建一次：实时任务状态 + 好感态度
       + 战场情报（仅当好感 ≥ +3）+ 输出纪律规则
     ↓
MALLMStreamingClient → Moonshot kimi-k2.6（OpenAI 兼容 SSE，关闭 thinking
     → 首 token 约 1 秒；密钥在主机侧从 MOONSHOT_API_KEY 读取，绝不打包，
     绝不下发给客户端）
     ← 内容增量 → 服务器端回声清洗 → MADialogueWidget 里的流式字幕
     ← tool_call 增量（首帧带 id/name，其后是参数片段）
            ↓ FMAToolCallAggregator（按 index 稀疏重组）
     ↓
MANarrativeSubsystem::ExecuteToolCall —— 服务器校验流水线：
     白名单 → 参数钳制（击杀数 1–10 · 时限 0|60–600 秒 · 突袭数量 1–6 · 祝福时长 30–180 秒）
     → 每局预算（2 次突袭 + 2 次祝福）+ 60 秒共享冷却 + 赛末封锁
     → 好感门槛（≤ −3 时除道歉外全部禁用；祝福需要 ≥ +3）
     ↓ 通过后：
        give_quest     → 挂起，直到对话窗口关闭
                          → 发布者离场（消失特效）→ 2.5 秒 → 敌人环形刷出，
                            刷出瞬间打上截止时间戳 → 追踪器/日志更新
        trigger_raid   → 额外敌人波次，90 秒后自动消散
        grant_blessing → 限时的速度/攻击倍率 GE —— 运行时构造的临时
                          UGameplayEffect，C++ 里不引用任何内容资产
        adjust_favor   → 对 PlayerState.NpcFavor 施加 ±2 钳制后的增量
     ↓
每个节拍 → AMAGameState::Multicast_OnNarrativeAnnounce（底部居中字幕）
           + Multicast_OnNarrativeFX（按位置播放 Niagara：消失 / 出现 / 刷怪）
     ↓
任务终结（完成 / 超时）→ 清理敌人 → 3 秒节拍
     → 发布者归来（出现特效）+ 奖励（运行时治疗 GE）→ 日志反映结果
```

LLM 从不直接改动状态 —— 它只发出*提案*，由服务器端的纯逻辑规则核心（`MAQuestRules`、`MAWorldEventRules`、`MAFavorRules`）来接受、钳制或拒绝。被拒绝的调用同样会产出一句得体的台词；对话窗口关闭不再取消进行中的请求（工具调用照样落地，迟到的文本由客户端按过期消息 id 丢弃）。

### 伤害流程

```
玩家输入（LMB）                                AI BT tick
     ↓                                         ↓
Enhanced Input → ASC.AbilityLocalInputPressed(InputID)    ASC.TryActivateAbilitiesByTag
     │   spec 空闲 → 激活；spec 激活中 → 复制的 InputPressed = 连招缓冲
                                ↓
                    UGA_MeleeAttack::ActivateAbility（LocalPredicted）
                                ↓
                    CommitAbility → BP_GE_StaminaCost（-10 体力）+ BP_GE_Cooldown_Melee（1 秒）
                                ↓
                    PlayMontageAndWait + WaitGameplayEvent（AnimNotify Event.Montage.Hit）
                    + WaitGameplayEvent（Event.Montage.ComboWindow）+ WaitInputPress（重新武装）
                    │     窗口开启 → 消费排队的按键 → MontageJumpToSection(Combo2/3)
                                ↓
                    服务器：PerformHitTrace → SphereSweep ECC_Pawn（按相机 yaw 瞄准，
                    ma.LagComp.Enabled 时做延迟补偿回溯）
                                ↓
                    施加 BP_GE_Damage（MADamageExecutionCalculation），带 SetByCaller
                    Data.DamageMultiplier —— 由 Lua 调参，玩家/AI 取值分离
                                ↓
                    Source.AttackPower（快照）× (1 - Target.Armor × 0.05)
                            × 0.3（目标处于 State.Blocking 时）× Data.DamageMultiplier
                                ↓
                    在目标 ASC 上输出 Damage 元属性
                                ↓
                    目标 AS::PostGameplayEffectExecute 把 Damage 路由到 Health
                       ├─ 存活：Event.Damage.Taken → GA_HitReact（ServerInitiated 硬直）
                       └─ Health<=0：AS::CheckDeath(ASC, instigator)
                                ↓
                    State.Dead + CancelAbilities + Execute_HandleDeath + GameMode::NotifyKill
                       ├─ 死斗计分（启用时）
                       └─ Narrative->NotifyKill(KillerPS) → 任务进度 / 完成判定
                                ↓
                    Multicast_PlayDeath：编排的死亡动画（单节点播放）→ 动画结束时
                    交接给 ragdoll · AI 额外：停止大脑 + 清空黑板
                                ↓
                    ScheduleRespawn(3 秒) + 客户端重生倒计时 → UnPossess +
                    GameMode.RestartPlayer → 在 PlayerStart 生成全新 pawn
```

### 对局循环（死斗 —— 保留，默认关闭）

剧情模式出厂设置为 `MatchDuration = 0` / `KillTarget = 0`，这会屏蔽计时和按分数结算的结束条件；在 GameMode 上把两者都设为 > 0，即可恢复下面完整的死斗流程。

```
AGameMode MatchState 状态机：WaitingToStart → InProgress → WaitingPostMatch
     ↓ HandleMatchHasStarted：GameState.MatchEndServerTime = 现在 + MatchDuration
       （只复制一次 —— 每个客户端都基于 GetServerWorldTimeSeconds 渲染计时）
每次击杀：CheckDeath(ASC, 效果上下文中的 instigator) → GameMode::NotifyKill
     ├─ PlayerState.Kills/Deaths（复制；自杀只算死亡，绝不算击杀）
     └─ GameState.Multicast_OnKill → 击杀信息行 + 受害者 HUD 上的 "KILLED BY"
ReadyToEndMatch（引擎 Tick 轮询）：计时结束 或 达到击杀目标
     ↓ HandleMatchHasEnded：结算结果在状态翻转**之前**写入 —— 同一个 actor、同一帧、
       与 MatchState 同一个复制包，因此任何客户端都不会渲染出缺少胜者的结算界面
     ↓ 战斗冻结 → 固定展示的计分板结算（胜者行为金色）→ 10 秒后 RestartGame()
```

### 火球流程（远程 AoE —— 预测施法，权威投射物）

```
玩家输入（Q）→ ASC.TryActivateAbilitiesByTag(Ability.Ranged.Fireball)
     ↓
UGA_Fireball::ActivateAbility（LocalPredicted —— 施法在拥有端客户端**立即**开始）
     ↓
CommitAbility（体力 -20 + 3 秒冷却）· 可移动施法 —— Montage 走 UpperBody 插槽，
     双腿继续跑动；躯干朝相机做瞄准扭转（UMAAnimInstance，State.Casting）
     ↓
PlayMontageAndWait(AM_FireballCastUB) + WaitGameplayEvent(Event.Montage.SpawnProjectile)
     ↓ （释放帧的 AnimNotify —— 前摇正好掩盖投射物的复制延迟）
仅服务器：对伤害 spec 做快照（此刻捕获来源的 AttackPower）
     ↓
SpawnActorDeferred<AMAProjectile>（复制）→ 命中时 SphereOverlap → 把快照 spec
应用到半径内每个 ASC（与近战同一套 ExecCalc）→ GameplayCue 爆裂特效 → Destroy
```

### HUD 绑定（Lyra 风格）

```
PlayerController.BeginPlay
     ↓ CreateWidget<UMAUserWidget>(WBP_HUD) + AddToViewport
PlayerController.OnRep_PlayerState
     ↓ HUDWidget->InitFromASC(PS->GetAbilitySystemComponent())
                  ↓
UMAUserWidget::InitFromASC 绑定属性变化委托
                  ↓ BlueprintImplementableEvent OnHealthChanged / OnStaminaChanged / ...
                  ↓ WBP_HUD（BP 子类）覆写事件 → ProgressBar.SetPercent(...)
```

Character / PlayerState **不持有任何面向 HUD 的 API**。NPC 通过 `UWidgetComponent` 复用同一套控件基类来做浮动血条。Lua 驱动的控件则把这条缝反过来切：C++ 在 PlayerState 上暴露 `BlueprintPure` 读取器（`GetActiveQuestState` / `GetNpcFavor` / `GetActiveBuffLines`）和骨架控件 API（`AddLine`/`AddButton`），而玩家看到的一切由 Lua 模块决定。

---

## 亮点

| 领域 | 内容 |
|---|---|
| **LLM 叙事引擎** | NPC 通过 `MALLMStreamingClient` 说话（HTTP 上的 SSE，`thinking:{disabled}` 换取瞬时首 token，服务器端回声清洗，窗口关闭后保持连接以便进行中的工具调用仍能落地），并通过 4 个声明的工具**行动**：`give_quest` / `trigger_raid` / `grant_blessing` / `adjust_favor`。流式工具调用片段由按 index 稀疏重组的 `FMAToolCallAggregator` 拼装。**LLM 提议，服务器裁决**：`MANarrativeSubsystem::ExecuteToolCall` 依次执行白名单 → 参数钳制（击杀数 1–10，时限 0\|60–600 秒，突袭数量 1–6，祝福时长 30–180 秒）→ 每局预算 + 共享冷却 + 赛末封锁 → 好感门槛。规则核心（`MAQuestRules` / `MAWorldEventRules` / `MAFavorRules`）是纯逻辑且有单元测试。 |
| **剧情模式循环** | 在 NPC 发布任务之前，竞技场是空的。任务开始锚定在**对话关闭**这一刻：发布者离场（Niagara 消失特效）→ 2.5 秒节拍 → 敌人环形刷出并打上截止时间戳；击杀归属汇入任务进度；完成或超时后敌人清理，经过 3 秒节拍，发布者带着奖励归来（特效）。每个节拍都经 `AMAGameState` 多播一条底部居中字幕。增益 GE（`GE_Buff_Speed`/`GE_Buff_Attack`）和治疗奖励都是**运行时构造的临时 `UGameplayEffect`** —— C++ 里不引用任何内容资产。 |
| **好感经济** | PlayerState 上的 `NpcFavor`，钳制在 [−10, +10]，单次调用增量 ±2。信任度 ≥ +3 解锁 system prompt 里的战场情报和 `grant_blessing` 工具；≤ −3 则禁用除道歉式 `adjust_favor` 外的所有工具。system prompt **每条消息**都从实时任务状态 + 好感态度重建，所以 NPC 的认知始终与世界一致。 |
| **LLM 战术顾问** | 仅服务器的 `UMATacticalAdvisorSubsystem` 定期为 LLM 概括战况，并落地它给出的结构化决策 —— 攻击槽位覆写进战斗调度器、焦点目标建议进 `BTService_UpdateTargetInfo`（经校验，可自动回退）、以及一句嘲讽进击杀信息栏。严格分层：LLM 是认知层，BT 反射层从不等它 —— 超时或胡言乱语一律回退默认值，不会有任何一帧被阻塞。决策解析（`MAAdvisorDecision`）是纯逻辑且有单元测试。 |
| **API 密钥安全** | Moonshot 密钥绝不进入仓库或安装包：从 `MOONSHOT_API_KEY` 环境变量读取（并有 `HKCU\Environment` 注册表回退），**仅保存在主机侧** —— 客户端只与 listen server 通信，绝不直连 LLM。打包版本附带 `SetupApiKey.bat`（一个 `setx` 助手）以便分发。 |
| **GAS 五大支柱** | `GameplayAbility`（LocalPredicted ×6：近战连招 / 冲刺 / 闪避 / 火球 / 格挡 / 突进斩，外加 ServerInitiated 的 HitReact）· `GameplayEffect`（伤害 Execution / 冷却 / 体力消耗 / 周期性体力回复 / 运行时构建的叙事增益，带 **SetByCaller 伤害倍率**）· `AttributeSet`（Health/MaxHealth/Stamina/AttackPower/Armor + Damage 元属性）· `GameplayCue`（带 `FHitResult` 位置/法线的 `Static` notify + 数据驱动的 C++ 爆裂 cue 基类）· `PredictionKey` |
| **武士刀连招（4 段）** | 一个 ability 实例 + 一个多 section 的**全身根运动** Montage（`AM_KatanaCombo_GS`）。挥刀过程中的再次按键，经由 **GAS 通用的复制 `InputPressed` 事件**（`AbilityLocalInputPressed` 的 InputID 路由）缓冲 —— 这是一个按键**队列**，所以连按 N 次会在客户端**和**服务器上都产出 N 段连击，且不需要任何自定义 RPC。每个 section 的 `ComboWindow` notify 开启一个**窗口期**：排队的按键在窗口开启时接续，之后的按键则通过 `MontageJumpToSection` 立即接续（取消后摇）—— section 状态经 `FGameplayAbilityRepAnimMontage` 复制，因此预测失误的修正表现为 montage 内部的 section 跳转，而不是整个 ability 回滚。每一刀都带有 section 级的 **Motion Warping** 窗口，通过共享的 `MAAttackLunge`/`MAWarpOps` 助手突进到锁定/锥形范围内的目标（落空时距离有上限）。 |
| **防御与反应** | **长按格挡**（RMB）：`State.Blocking` 在伤害 ExecCalc 内部通过捕获的目标标签开启 70% 减伤；**全身武士刀防御循环**（`AM_KatanaGuardLoop`）保持架势，每次挡下的攻击都叠加一次招架反应（`AM_KatanaGuardHit`）。**硬直**：`GA_HitReact`（ServerInitiated，由 AttributeSet 中抛出的 `Event.Damage.Taken` gameplay 事件触发，`bRetriggerInstancedAbility`）—— 每次命中都会让对方一顿并打断其连招；格挡/施法/闪避期间抑制。 |
| **锁定与 8 向环绕** | `UMALockOnComponent` 软锁定（**R3 / 鼠标中键**切换，右摇杆轻拨切换目标）—— 锁定目标会复制，所以模拟代理端的环绕移动也正确。锁定状态下，`UMAAnimInstance` 从复制的速度与朝向推导 `Direction` + `bStrafing`，再用 `BlendPosesByBool` 混入**8 向武士刀环绕 blendspace**（`BS_Katana_Strafe`），底层是武士刀战斗待机。 |
| **突进斩（Motion Warping）** | `UGA_DashSlash`（**E / 手柄 RB**）：一次根运动的武士刀突进，通过共享的 `MAAttackLunge::PickLungeTarget` → `MAWarpOps` SkewWarp 配置扭曲到瞄准/锁定目标上 —— **同一个助手也驱动每一刀的连招 warp**，因此二者在客户端和服务器上的解算完全一致。落空的突进会播放其编排的上步动作，且前向无目标距离有上限。 |
| **编排的死亡动画** | 死亡动画以单节点方式播放（绕过 ABP，不占 montage 插槽），随后**在动画结束时把尸体交接给 ragdoll** —— 由物理把原地的最终姿势沉降到地面。AI 尸体停止思考（停止大脑 + 清空黑板 —— 不会有死后仍在追踪目标，也不会在重生时打出幽灵攻击）。 |
| **Lua 三层（UnLua）** | 为了做 live-ops 式调参而有意划分的 C++/Lua 边界：**① C++ 战斗核心** —— 热路径绝不触碰 `lua_State`。**② Lua 配置**（`Content/Script/AbilityConfig.lua`）：能力可调参数（近战 trace/warp、突进 warp、火球 AoE、montage 播放速率、经 SetByCaller 送进 ExecCalc 的**玩家与 AI 分离的伤害倍率**）只解析一次，缓存进 GameInstance subsystem 上的 C++ 缓存；**保存文件即热重载**（1 秒时间戳轮询，非 Shipping）—— 无需重编译、无需重启 —— 另有 `MA.Lua.ReloadAbilityConfig` 控制台命令兜底；错误的改动会保留上一份可用缓存，缺失的键回退到 `UPROPERTY` 默认值。**③ Lua UI**（`Content/Script/UI/QuestJournal.lua`、`EscMenu.lua`）：C++ 控件实现 `IUnLuaInterface`，把呈现逻辑交给 Lua 模块 —— 布局、刷新节奏、按钮接线全都可以不碰 C++ 就改。运行在**内置的 UnLua 2.3.6（已移植到 UE5.5）**之上。 |
| **对话与叙事 UI** | `MADialogueWidget` —— 流式字幕文本、思考中动效指示，以及一个专用的**任务按钮**（任务进行中或回复在途时禁用）；NPC **头顶名牌**在半径内提示「按 T 对话 · 可接任务」；右侧**任务追踪** HUD；**J** 打开 Lua 驱动的任务日志（任务 / 倒计时 / 生效增益 / 好感态度 / K-D）；**ESC** 打开 Lua 驱动的暂停菜单（继续 / 日志 / 返回主菜单 / 退出），并绑定手柄（**Special Left/Right**）。公告字幕与按位置播放的 Niagara 特效都搭 GameState 多播。 |
| **死斗循环（保留，默认关闭）** | 基于引擎 `MatchState` 的死斗 —— 复制的计时 / 击杀目标 / 结算、击杀信息栏、"KILLED BY"、长按 Tab 的计分板兼结算界面、自动重开 —— 代码完整保留，但剧情模式出厂时 `MatchDuration`/`KillTarget` ≤ 0，从而关闭计时与按分数结束。在 GameMode 默认值里把两者设为 > 0，就能把竞技场切回 5 分钟 / 先到 N 杀的死斗。击杀归属（效果上下文 instigator → `CheckDeath` → GameMode 路由）现在同时也汇入任务进度。 |
| **服务器端延迟补偿** | `UMALagCompSubsystem`（`UTickableWorldSubsystem`）把每个战斗单位的胶囊体逐帧记入**时间环形缓冲**；近战命中路径（`MAMeleeHitOps`，受 `ma.LagComp.Enabled` 控制）把目标回溯到攻击者客户端所见的那一刻（ping + 插值延迟），再做**扫掠球对胶囊体**的几何相交，并带有**偏向攻击方**的回溯上限（`ma.LagComp.MaxRewindSeconds` 0.3）。纯几何核心，完整单元测试覆盖。 |
| **预测修正指标** | `UMAPredictionMovementComponent` 继承 CMC 并覆写 `OnClientCorrectionReceived`，把服务器和解情况（修正频率 + 位置误差）量化进纯逻辑的 `FMACorrectionTracker`；`ma.PredDebug` 会在屏幕上绘制读数，以及修正前/后变换的**红/绿幽灵**。 |
| **打击感** | `MAHitFeel`（`ma.HitFeel` 总开关）：通过短暂冻结骨骼网格的动画速率实现 **hit-stop** —— 绝不使用全局时间膨胀，其余模拟照常运行 —— 外加按力度缩放的**镜头震动**（`MACameraShakes` 轻/重）、命中音效和击退，全部由 `GCN_MeleeImpact` cue 触发。 |
| **AI 战斗调度器** | 仅服务器的 `UMACombatDirectorSubsystem` 发放数量受限的**攻击令牌**（`MAAttackTokenLedger`、`ma.AI.MaxAttackers`、`ma.AI.TokenTTL`），让蜂拥的 AI 轮流上而不是一拥而上：`BTTask_ClaimAttackToken`/`ReleaseAttackToken` 控制近战节点，`BTTask_CircleTarget` 在等待时环绕走位。`MAAIDefenseComponent` + `MAAIDefensePolicy` 在来袭挥砍时抬手防御 —— 激活的是玩家用的**同一个 `GA_Block`**；AI 之间阵营免伤。台账与策略均为纯逻辑且有单元测试。战术顾问作为可选的认知层位于其上。 |
| **VFX 与手工材质** | 叙事特效（发布者消失/出现、敌人刷出）是渲染 `M_MANarrativeGlow` 的 Niagara 爆裂系统 —— 那是一个**通过 MCP 材质图工具手工编写的 sprite 材质**：一个 HLSL `Custom` 节点（径向核心 + 光晕 + 4 角星衰减）× ParticleColor，输出到 additive-unlit 自发光，并显式设置了 Niagara usage 标志（少了这个标志 Shipping 会静默拒绝编译 —— 编辑器视口能容忍，打包版本不会）。 |
| **Win64 打包** | `BuildCookRun -pak -nodebuginfo` 的 Shipping 构建，约 495 MB（pak + Oodle；松散 staging 是 958 MB，**而且**每个资产都是可读的）。Cook 经验已沉淀：字符串引用的地图需要 `MapsToCook`/`DirectoriesToAlwaysCook`（cook 图不会跟随软字符串引用），编辑器模块的原生 gameplay tag 会变成 cook 错误（已改为 ini 标签注册），首次加载的 PSO/着色器预热并不是卡死。 |
| **自动化测试套件** | **22 个 UE Automation 测试**覆盖各纯逻辑核心，全部无头运行：`MultiPlayerAction.LagComp.*`（×5 几何/历史/回溯）、`.LLM.*`（×5：请求体序列化含 tools 与 thinking 标志、SSE 解析器、OpenAI chunk 解码、工具调用解析、片段聚合器）、`.Narrative.*`（×3：任务 / 世界事件 / 好感规则）、`.LuaConfig.*`（×3：缓存查找、真实 `lua_State` 桥接解析、subsystem 重载）、`.AICombat.*`（×3：令牌台账、防御策略、顾问决策）、`.Lunge.TargetSelection`、`.Prediction.CorrectionTracker`、`.HitFeel.ComputeHitFeel`。 |
| **武器与动画资源** | 挂在 `hand_r` 插槽上的武士刀 + **GhostSamurai** 动画集（4 段攻击根运动、16 向走/跑环绕、防御循环 + 招架、待机），**通过一条全程程序化的流水线做 IK 重定向，从 UE4 迁到 UE5 的 Manny 骨架上**（`IK_UE4Manny` + `RTG_UE4Manny_to_UE5`），并被产品化为 MCP 插件的 `retarget_animations` 工具。 |
| **投射物网络同步** | Lyra/GASShooter 风格的远程 AoE：预测的施法 montage（客户端即时反馈）+ **仅服务器生成**一个复制的 `AMAProjectile`，其携带的伤害 spec 是**施法时刻的快照** —— 即使施法者在飞行途中死亡，火球依然按施法时的属性结算。AoE 重叠把同一套 ExecCalc 应用到半径内每个 ASC；爆炸特效经 GameplayCue 复制。 |
| **伤害公式** | `UGameplayEffectExecutionCalculation` 捕获来源 `AttackPower`（快照）+ 目标 `Armor`（实时）+ 一个 **SetByCaller 的 `Data.DamageMultiplier`**（由 Lua 按玩家/AI 分别调参），输出到 `Damage` 元属性，再由 AS 路由到 `Health`。沿用 Lyra 的 `ULyraDamageExecution` 模式。 |
| **HUD 架构** | 自包含的 C++ View，由 `UMAUserWidget::InitFromASC` 的树扫描自动接线：`UMAHealthBarWidget`（街霸式的**削减血条**）和 `UMASkillSlotWidget`（冷却扫描 + 激活标签高亮）。**同一个削减血条控件**也兼作 NPC 头顶血条。对局/叙事 UI 层（计时与 K-D 覆层、击杀信息栏、计分板、任务追踪、公告、对话、名牌）**完全由代码构建** —— 在 C++ 里用 `WidgetTree::ConstructWidget`，零 BP 资产；日志和 ESC 菜单在此之上再叠一层 Lua 呈现层。另有触摸控制与面向移动端的 DPI/安全区缩放。 |
| **动画** | `ABP_Manny` 里的上半身分层骨架（缓存全身姿势 → `UpperBody` 插槽 → 在 `spine_01` 上做逐骨骼分层混合）让双腿在**火球施法**时继续跑动；`UMAAnimInstance` 再加一层**躯干瞄准扭转**（脊柱链朝相机 yaw，钳制在 ±90°，受 State 标签控制）。 |
| **AI** | `BehaviorTree` + 自定义 `UBTService_UpdateTargetInfo`（最近的**存活**玩家，优先 **NavMesh 可达**的目标，并感知顾问指定的焦点）+ 自定义 `UBTTask_TryActivateAbilityByTag`。AI 通过 Enhanced Input 驱动**与玩家相同的 `UGA_MeleeAttack` 武士刀类**，由攻击令牌调度器协调。任务敌人由叙事 subsystem 在竞技场周围环形刷出。 |
| **会话前端** | `UMASessionSubsystem`（架在 OnlineSubsystem 会话接口之上的 GameInstance subsystem），配 UMG **MVVM** 主菜单 —— `UMAMainMenuViewModel` + FieldNotify 绑定、已发现会话的 ListView、带在途保护的 host/join/refresh，以及网络故障后回到菜单的恢复。 |
| **网络同步** | 服务器权威的伤害；玩家 ASC 用 `Mixed` 复制（冷却只发给拥有者）+ NPC 用 `Minimal`。死亡表现、击杀事件、叙事公告与特效走 `NetMulticast` RPC。对话、开发者作弊和重生倒计时截止时间走 `Server`/`Client` RPC。 |
| **死亡/重生** | `IMACombatantInterface` 抽象；`State.Dead` 松散标签阻止重复激活；在 `GameMode->RestartPlayer()` 之前调用 `UnPossess()` 以强制生成全新 pawn。拥有端客户端基于一次 `Client` RPC 下发的服务器时间截止点渲染重生倒计时。 |

---

## 模块结构

```
Source/MultiPlayerAction/
├── MultiPlayerActionCharacter.{h,cpp}                                玩家 pawn：输入绑定、经 PlayerState 初始化 GAS、武器网格、编排死亡
├── MultiPlayerActionGameMode.{h,cpp}                                 对局规则：击杀路由（计分 + 任务进度）、可选的死斗结束条件
├── MAGameState.{h,cpp}                                               复制的对局数据 + 击杀 / 公告 / 叙事特效 / 嘲讽多播
├── LLM/
│   ├── MALLMTypes.h                                                  消息 / 角色 / 工具调用 / 工具声明的 POD
│   ├── MALLMSettings.h                                               配置（模型、最大 token、thinking 与温度策略）
│   ├── MALLMRequestBody.{h,cpp}                                      纯 OpenAI 兼容请求序列化（messages + tools）—— 有单测
│   ├── MASSEStream.{h,cpp}                                           纯 SSE 行解析 + chunk 解码 + FMAToolCallAggregator —— 有单测
│   └── MALLMStreamingClient.{h,cpp}                                  流式 HTTP 客户端：增量、工具调用拼装、密钥解析（环境变量 + 注册表）
├── Narrative/
│   ├── MAQuestTypes.h                                                任务状态 POD（BlueprintType —— Lua/BP 的读取接缝）
│   ├── MAQuestRules.{h,cpp}                                          纯任务校验 / 生命周期规则 —— 有单测
│   ├── MAWorldEventRules.{h,cpp}                                     纯突袭与祝福的预算 / 冷却 / 钳制 —— 有单测
│   ├── MAFavorRules.{h,cpp}                                          纯好感钳制 + 信任/禁用门槛 + 态度文案 —— 有单测
│   └── MANarrativeSubsystem.{h,cpp}                                  服务器编排器：工具声明、ExecuteToolCall 流水线、任务生命周期节奏、
│                                                                     敌人环形刷出、运行时 GE 构造、公告、特效
├── Dialogue/
│   ├── MADialogueSubsystem.{h,cpp}                                   服务器对话代理：每条消息的 system prompt、历史裁剪（保证 tool 成对）、
│   │                                                                 回声清洗、窗口关闭后保持连接
│   └── MADialogueComponent.{h,cpp}                                   NPC 侧：交互半径、名牌、任务敌人类、叙事特效播放
├── AbilitySystem/
│   ├── MAAbilitySystemComponent.h                                    自定义 ASC 子类（扩展钩子）
│   ├── MAAbilityInputID.h                                            用于 AbilityLocalInputPressed 路由的 InputID 枚举（连招输入缓冲）
│   ├── MAAttributeSet.{h,cpp}                                        复制属性 + Damage 元属性 + PostExecute 钳制/硬直事件 + CheckDeath
│   ├── MACombatantInterface.h                                        「一切会死的东西」抽象，由 AS::CheckDeath 调用
│   ├── MAGameplayTags.{h,cpp}                                        原生 gameplay 标签（Ability.* / GameplayCue.* / State.* / Event.* / Data.*）
│   ├── AnimNotifies/AN_SendGameplayEvent.{h,cpp}
│   ├── Abilities/                                                    MAGameplayAbilityBase + GA_MeleeAttack / HitReact / Block / Sprint / Dodge /
│   │                                                                 Fireball / DashSlash（基类按玩家/AI 读取 Lua 伤害倍率）
│   ├── Cues/                                                         GCN_ParticleBurst（数据驱动的爆裂基类）+ GCN_MeleeImpact（打击感）
│   └── Executions/MADamageExecutionCalculation.{h,cpp}               AP 快照 × 护甲 × 格挡 × SetByCaller 倍率
├── Config/
│   ├── MAConfigTypes.h                                               能力可调参数 POD
│   ├── MALuaBridge.{h,cpp}                                           真实 lua_State 表解析成 C++ 结构体 —— 有单测
│   └── MALuaAbilityConfig.{h,cpp}                                    GameInstance subsystem 缓存 + 保存触发的自动热重载 + 控制台重载
├── Combat/                                                           MAProjectile · MAMeleeHitOps（延迟补偿入口）· MAAttackLunge · MAWarpOps ·
│                                                                     MAHitFeel · MACameraShakes
├── Targeting/MALockOnComponent.h                                     软锁定：目标选取 / 切换，复制当前目标
├── Network/                                                          MALagCompGeometry/Subsystem（回溯命中解算）· MACorrectionTracker ·
│                                                                     MAPredictionMovementComponent（和解指标 + 幽灵）
├── Player/
│   ├── MAPlayerState.{h,cpp}                                         ASC + AttributeSet 的持有者；复制 Kills/Deaths/ActiveQuest/NpcFavor；
│   │                                                                 为 Lua UI 提供 BlueprintPure 读取接缝（任务状态 / 好感 / 生效增益行）
│   └── MAPlayerController.{h,cpp}                                    持有全部本地 UI；按键路由（T 对话 · J 日志 · ESC 菜单 · 手柄）；
│                                                                     菜单动作（继续 / 返回菜单 / 退出）以 BlueprintCallable 暴露给 Lua
├── Online/MASessionSubsystem.{h,cpp}                                 OnlineSubsystem 会话（host/find/join，故障恢复）
├── Animation/MAAnimInstance.{h,cpp}                                  躯干瞄准扭转 + 环绕参数
├── UI/
│   ├── MAUserWidget / MAHealthBarWidget / MASkillSlotWidget          HUD 核心（削减血条、技能槽）
│   ├── MAMatchStatusWidget / MAScoreboardWidget / MAKillFeedWidget   死斗层（关闭时自动隐藏）
│   ├── MADialogueWidget.{h,cpp}                                      流式对话：字幕、思考指示、专用任务按钮
│   ├── MANpcNameplateWidget.{h,cpp}                                  NPC 头顶名字 + 交互提示（按半径显示）
│   ├── MAQuestTrackerWidget.{h,cpp}                                  右侧任务 HUD（进度 + 倒计时）
│   ├── MAAnnounceWidget.{h,cpp}                                      底部居中叙事字幕（GameState 多播）
│   ├── MAQuestJournalWidget.{h,cpp}                                  骨架容器 + AddLine API —— 逻辑在 Lua 里（UI.QuestJournal）
│   ├── MAEscMenuWidget.{h,cpp}                                       骨架容器 + AddButton API —— 逻辑在 Lua 里（UI.EscMenu）
│   ├── MALockOnReticleWidget / MATouchControlsWidget                 锁定准星 · 移动端触摸控制
│   └── MainMenu/                                                     MVVM 前端（ViewModel + FieldNotify + 会话 ListView）
├── AI/
│   ├── MATargetDummy / MAEnemyController                             Pawn 持有 ASC（Minimal 复制），BT 引导
│   ├── BTTask_TryActivateAbilityByTag / BTService_UpdateTargetInfo（感知顾问焦点）
│   ├── BTTask_ClaimAttackToken / BTTask_ReleaseAttackToken / BTTask_CircleTarget
│   └── Combat/
│       ├── MACombatDirectorSubsystem.{h,cpp}                         服务器攻击令牌代理
│       ├── MAAttackTokenLedger.{h,cpp}                               纯攻击槽位台账 —— 有单测
│       ├── MAAIDefensePolicy.{h,cpp} + MAAIDefenseComponent.{h,cpp}  事件驱动的防御（复用 GA_Block）
│       ├── MAAdvisorDecision.{h,cpp}                                 纯 LLM 决策解析/净化 —— 有单测
│       └── MATacticalAdvisorSubsystem.{h,cpp}                        仅服务器的 LLM 顾问（槽位 / 焦点 / 嘲讽，失败即放行）
└── Tests/                                                            22 个自动化测试：MALagCompGeometryTest · MASSEStreamTest ·
                                                                      MALLMRequestBodyTest · MAQuestRulesTest · MAWorldEventRulesTest ·
                                                                      MAFavorRulesTest · MALuaConfigTest · MAConfigLookupTest ·
                                                                      MAAttackTokenLedgerTest · MAAIDefensePolicyTest · MAAdvisorDecisionTest ·
                                                                      MAAttackLungeTest · MACorrectionTrackerTest · MAHitFeelTest
```

`Content/` 下的内容要点：
- `Script/` —— Lua 层：`AbilityConfig.lua`（战斗调参，保存即热重载）、`UI/QuestJournal.lua`、`UI/EscMenu.lua`（UnLua 控件模块）
- `VFX/` —— `M_MANarrativeGlow`（手工编写的 HLSL sprite 材质）+ `NS_GiverVanish` / `NS_EnemySpawn` Niagara 系统
- `AbilitySystem/` —— GA/GE/Cue 蓝图（对 C++ 类的纯配置）、`AM_KatanaCombo_GS` 以及防御/施法/受击 montage
- `AnimLibrary/GhostSamurai/` —— UE4→UE5 重定向骨架 + 重定向后的武士刀动画集 + `BS_Katana_Strafe`
- `Blueprints/` —— `BP_TargetDummy`、`BP_EnemyController`、`BP_Projectile_Fireball`、`WBP_HUD`/`WBP_MainMenu`（MVVM）；对局/叙事覆层控件**没有资产** —— 全部由 C++ 构建
- `AI/` —— `BB_Enemy`、`BT_Enemy`

> 上述内容的绝大部分是**通过 UnrealAgentMCP 在 agent 侧**编写的（私有仓库）：montage、AnimNotify、GE 配置、BehaviorTree、UMG 控件树 + MVVM 绑定、AnimGraph 手术、Niagara 系统、HLSL 材质图以及竞技场场景布置，全都经由 MCP 工具完成 —— 其中若干工具正是因为这个项目需要才被造出来。这条自产自用的闭环，是这份作品集的另一半。

---

## 构建

环境要求：
- Unreal Engine **5.5**（与 `MultiPlayerAction.uproject` 的 `EngineAssociation` 一致）
- Visual Studio 2022（Windows）/ Xcode 15+（macOS），需安装 C++ 工作负载
- 可选：给 LLM NPC 用的 **Moonshot API 密钥** —— 在主机上设置 `MOONSHOT_API_KEY` 环境变量（可在 platform.moonshot.cn 申请）。没有它时，战斗和无任务的探索照常可玩，对话窗口会提示缺少密钥；该密钥绝不入库、绝不打包、绝不下发给客户端。
- 可选：Starter Content（爆裂 cue 用到的粒子模板）

步骤：
```
git clone https://github.com/alertform/ue5-multiplayer-action.git
cd ue5-multiplayer-action
# 右键 MultiPlayerAction.uproject → "Generate Visual Studio project files"
# 打开 MultiPlayerAction.sln，把 MultiPlayerActionEditor 设为启动项，Build。
```

打包（Win64 Shipping，pak + Oodle，约 495 MB）：
```
"<UE_5.5>/Engine/Build/BatchFiles/RunUAT.bat" BuildCookRun \
  -project="<abs>/MultiPlayerAction.uproject" -platform=Win64 -clientconfig=Shipping \
  -cook -build -stage -pak -nodebuginfo -package -archive -archivedirectory="<out>"
```
打包版本内含 `SetupApiKey.bat` —— 一个只需回答一个提示的 `setx` 助手，让试玩者不必手动改注册表就能装上自己的密钥。

### 怎么玩（剧情模式）

从主菜单开房（或从 `Content/Maps/ThirdPersonMap` 进 PIE，Number of Players ≥ 2 以便验证复制）。竞技场一开始是**空的** —— 走到那位游荡的剑客跟前（有头顶名牌），按 **T** 交谈。可以随便聊 —— NPC 会流式回复并记得上下文 —— 也可以按专用的**任务按钮**直接讨活干。接下任务、关闭对话，然后看这段节拍演完：剑客在一团光里离场，敌人环形浮现，任务追踪和截止时间出现。把他们清掉（或者拖到超时），他会带着评价、奖励，以及一份记恨或一个笑容回来 —— 好感是个真实的数值：攒到 **+3** 他开始分享战场情报并赐予速度/攻击祝福，掉到 **−3** 他就不再接你的话。

战斗：**LMB** 4 段武士刀连招（连按接续，在窗口内按可取消后摇）、**RMB 长按**格挡、**E** 突进斩、**Q** 火球、**Shift** 冲刺、**Ctrl** 闪避无敌帧、**R3 / 鼠标中键**锁定。UI：**J** 任务日志（任务 / 倒计时 / 生效增益 / 好感 / K-D —— Lua 驱动）、**ESC** 暂停菜单（Lua 驱动；手柄 **Special Left/Right** = 日志/菜单）、**Tab** 计分板。实时调战斗手感：编辑 `Content/Script/AbilityConfig.lua` 并保存 —— 数值一秒内热重载，无需重编译。

想恢复原来的死斗模式：把 GameMode 上的 `MatchDuration`/`KillTarget` 设为 > 0。

### 测试

22 个 UE Automation 测试覆盖各纯逻辑核心 —— 延迟补偿几何、LLM 请求/SSE/工具调用协议、叙事规则（任务/世界事件/好感）、Lua 配置桥接、AI 令牌台账 / 防御策略 / 顾问决策、突进目标选取、和解统计、打击感曲线 —— 都是不依赖引擎、可无头运行的逻辑。可从编辑器的 **Session Frontend → Automation** 运行，或：
```
"<UE_5.5>/Engine/Binaries/Win64/UnrealEditor-Cmd.exe" \
  "<absolute-path>/MultiPlayerAction.uproject" \
  -ExecCmds="Automation RunTests MultiPlayerAction; Quit" -unattended -nullrhi -nop4 -log
```
测试套件：`MultiPlayerAction.{LagComp,LLM,Narrative,LuaConfig,AICombat,Lunge,Prediction,HitFeel}.*`。

---

## 给评审者的说明

- 代码取向是 **C++ 优先、Blueprint 只做组装** —— 做的是引擎层面的工作，不是 BP 连线。Lua 层是一条有意划出的接缝（配置 + UI 呈现），不是绕开编译的脚本后门：战斗热路径绝不触碰 `lua_State`。
- LLM 被当作**不可信输入源**对待：它输出的一切在触及玩法之前都要过一遍纯逻辑、有单测的校验规则，所有状态变更都在服务器侧，API 密钥从不离开主机进程（读环境变量/注册表，不入库、不进 pak、不做客户端复制）。
- 架构决策在适用处沿用 Lyra 的惯例（HUD 控件绑定、NPC 的 ASC 归属、`Damage` 元属性路由、BT 与 GAS 的集成、复制模式）。
- 多人正确性已在 PIE listen-server、2 名玩家下验证；打包的 Shipping 版本做过端到端冒烟测试（开房、对话、任务循环）。值得知道的 Shipping 专有故障模式：编辑器会放过的材质 usage 标志、cook 期间 ensure 即报错、cook 图不会跟随的软字符串引用、看起来像卡死的首次加载 PSO 预热。
- 开发中的坑都记在了 commit message 里 —— UE 5.5 的弃用项、`RestartPlayer` 复用既有 pawn 的传送陷阱、组件状态要用 `NetMulticast`、montage 混出的空窗期、`bWarningsAsErrors` 下的同名遮蔽坑。

---

## 许可

尚未声明许可协议 —— 复用代码前请先联系。
