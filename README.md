# ue5-multiplayer-action

Story-driven multiplayer action demo built on Unreal Engine 5.5 — a katana combat sandbox where the quest-giver is an **LLM**. A wandering swordsman NPC chats with the player through a **streaming LLM dialogue** (Kimi k2.6, OpenAI-compatible SSE) and drives the game through a **tool-calling protocol** — issuing kill quests, triggering raids, granting blessings, tracking favor — with every tool call passing a **server-authoritative validation pipeline** before it touches gameplay. Underneath sits a full **Gameplay Ability System** stack, a **katana combat kit** (4-stage root-motion combos with per-swing Motion Warping, lock-on, block/parry, staggers, authored deaths, a Motion-Warped dash-slash), **server-side lag compensation** plus prediction-correction metrics and a hit-feel pass, AI driven by **BehaviorTree** — sharing the same `GameplayAbility` C++ classes as the player, coordinated by a server attack-token director and an **LLM tactical advisor** — and a **three-layer Lua integration** (C++ combat core / hot-reloaded Lua config / Lua-scripted UI via UnLua). Ships as a **Win64 Shipping pak build** (~495 MB). Backed by a 22-test headless UE Automation suite. Personal portfolio project.

> Built alongside (and largely *with*) **UnrealAgentMCP** — a self-developed in-editor MCP server (77 tools) that lets an AI agent author Blueprints, UMG, AnimGraphs, BehaviorTrees, montages, Niagara systems, material graphs (including HLSL Custom nodes), IK retargeting and level content directly inside the running editor. Most of this project's content-side work was authored agent-side through it. The plugin is developed in a separate **private** repo (demo available on request); it is not required to build or run this project.

---

## Architecture

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

### Narrative flow (LLM tool-calling, server-validated)

```
Player presses T near the swordsman NPC (nameplate advertises the interaction)
     ↓
MADialogueComponent (NPC side) ←→ MADialogueSubsystem (SERVER — owns history + protocol)
     ↓ system prompt rebuilt PER MESSAGE: live quest state + favor attitude
       + battle intel (only when favor ≥ +3) + output-discipline rules
     ↓
MALLMStreamingClient → Moonshot kimi-k2.6 (OpenAI-compatible SSE, thinking disabled
     → first token in ~1s; the key is read host-side from MOONSHOT_API_KEY, never
     shipped, never sent to clients)
     ← content deltas → server-side echo scrub → streamed subtitles in MADialogueWidget
     ← tool_call deltas (id/name first frame, argument fragments after)
            ↓ FMAToolCallAggregator (index-sparse reassembly)
     ↓
MANarrativeSubsystem::ExecuteToolCall — SERVER validation pipeline:
     whitelist → param clamps (kills 1–10 · time 0|60–600s · raid 1–6 · blessing 30–180s)
     → budgets (2 raids + 2 blessings / match) + 60s shared cooldown + match-end lockout
     → favor gates (≤ −3 mutes everything but apology; blessing requires ≥ +3)
     ↓ accepted:
        give_quest     → PENDING until the dialogue window closes
                          → giver departs (vanish FX) → 2.5s → enemy ring spawns,
                            deadline stamped at spawn → tracker/journal update
        trigger_raid   → extra enemy wave, auto-dissolves after 90s
        grant_blessing → timed speed/attack multiplier GE — runtime-constructed
                          transient UGameplayEffect, no content refs in C++
        adjust_favor   → ±2-clamped delta on PlayerState.NpcFavor
     ↓
every beat → AMAGameState::Multicast_OnNarrativeAnnounce (bottom-center subtitle)
           + Multicast_OnNarrativeFX (per-location Niagara: vanish / appear / spawn)
     ↓
quest terminal (cleared / expired) → enemies cleaned up → 3s beat
     → giver returns (appear FX) + reward (runtime heal GE) → journal reflects outcome
```

The LLM never mutates state directly — it emits *proposals* that the server's pure rules cores (`MAQuestRules`, `MAWorldEventRules`, `MAFavorRules`) accept, clamp, or reject. A rejected call still produces a graceful spoken line; a closed dialogue window no longer cancels an in-flight request (the tool call lands, late text is dropped client-side by a stale message id).

### Damage flow

```
Player input (LMB)                            AI BT tick
     ↓                                         ↓
Enhanced Input → ASC.AbilityLocalInputPressed(InputID)    ASC.TryActivateAbilitiesByTag
     │   idle spec → activate; active spec → replicated InputPressed = combo buffer
                                ↓
                    UGA_MeleeAttack::ActivateAbility (LocalPredicted)
                                ↓
                    CommitAbility → BP_GE_StaminaCost (-10 Stamina) + BP_GE_Cooldown_Melee (1s)
                                ↓
                    PlayMontageAndWait + WaitGameplayEvent (AnimNotify Event.Montage.Hit)
                    + WaitGameplayEvent (Event.Montage.ComboWindow) + WaitInputPress (re-armed)
                    │     window opens → consume queued press → MontageJumpToSection(Combo2/3)
                                ↓
                    SERVER: PerformHitTrace → SphereSweep ECC_Pawn (camera-yaw aim,
                    lag-comp rewind when ma.LagComp.Enabled)
                                ↓
                    Apply BP_GE_Damage (MADamageExecutionCalculation) with SetByCaller
                    Data.DamageMultiplier — Lua-tuned, separate player / AI values
                                ↓
                    Source.AttackPower (snapshot) × (1 - Target.Armor × 0.05)
                            × 0.3 if target State.Blocking × Data.DamageMultiplier
                                ↓
                    Output Damage meta-attribute on target ASC
                                ↓
                    Target AS::PostGameplayEffectExecute routes Damage → Health
                       ├─ survivor: Event.Damage.Taken → GA_HitReact (ServerInitiated stagger)
                       └─ Health<=0: AS::CheckDeath(ASC, instigator)
                                ↓
                    State.Dead + CancelAbilities + Execute_HandleDeath + GameMode::NotifyKill
                       ├─ deathmatch scoring (when enabled)
                       └─ Narrative->NotifyKill(KillerPS) → quest progress / completion
                                ↓
                    Multicast_PlayDeath: authored death clip (single-node) → ragdoll handoff
                    at clip end · AI also: brain stop + blackboard wipe
                                ↓
                    ScheduleRespawn(3s) + Client respawn countdown → UnPossess +
                    GameMode.RestartPlayer → fresh pawn at PlayerStart
```

### Match loop (deathmatch — retained, disabled by default)

Story mode ships with `MatchDuration = 0` / `KillTarget = 0`, which suppresses the clock and score-based endings; set both > 0 on the GameMode to restore the full deathmatch below.

```
AGameMode MatchState machine: WaitingToStart → InProgress → WaitingPostMatch
     ↓ HandleMatchHasStarted: GameState.MatchEndServerTime = now + MatchDuration
       (replicated once — every client renders the clock off GetServerWorldTimeSeconds)
each kill: CheckDeath(ASC, effect-context instigator) → GameMode::NotifyKill
     ├─ PlayerState.Kills/Deaths (replicated; suicide counts the death, never the kill)
     └─ GameState.Multicast_OnKill → kill feed line + "KILLED BY" on the victim's HUD
ReadyToEndMatch (engine Tick poll): clock expired OR kill target reached
     ↓ HandleMatchHasEnded: verdict written BEFORE the state flips — same actor, same
       frame, same replication bunch as MatchState, so no client ever renders a
       results screen with a missing winner
     ↓ combat freeze → pinned scoreboard results (winner row gold) → +10s RestartGame()
```

### Fireball flow (ranged AoE — predicted cast, authoritative projectile)

```
Player input (Q) → ASC.TryActivateAbilitiesByTag(Ability.Ranged.Fireball)
     ↓
UGA_Fireball::ActivateAbility (LocalPredicted — cast starts INSTANTLY on the owning client)
     ↓
CommitAbility (Stamina -20 + 3s cooldown) · MOBILE cast — montage on the UpperBody slot,
     legs keep running; torso aim-twists toward the camera (UMAAnimInstance, State.Casting)
     ↓
PlayMontageAndWait(AM_FireballCastUB) + WaitGameplayEvent(Event.Montage.SpawnProjectile)
     ↓ (release-frame AnimNotify — windup masks the projectile's replication latency)
SERVER ONLY: snapshot damage spec (source AttackPower captured NOW)
     ↓
SpawnActorDeferred<AMAProjectile> (replicated) → impact SphereOverlap → snapshotted spec
applied to every ASC in radius (same ExecCalc as melee) → GameplayCue burst FX → Destroy
```

### HUD binding (Lyra-style)

```
PlayerController.BeginPlay
     ↓ CreateWidget<UMAUserWidget>(WBP_HUD) + AddToViewport
PlayerController.OnRep_PlayerState
     ↓ HUDWidget->InitFromASC(PS->GetAbilitySystemComponent())
                  ↓
UMAUserWidget::InitFromASC binds attribute-change delegates
                  ↓ BlueprintImplementableEvent OnHealthChanged / OnStaminaChanged / ...
                  ↓ WBP_HUD (BP child) overrides events → ProgressBar.SetPercent(...)
```

Character / PlayerState carry **no HUD-facing API**. NPCs reuse the same widget base via `UWidgetComponent` for floating health bars. The Lua-driven widgets flip the seam the other way: C++ exposes `BlueprintPure` read accessors on PlayerState (`GetActiveQuestState` / `GetNpcFavor` / `GetActiveBuffLines`) and skeleton widget APIs (`AddLine`/`AddButton`), and the Lua module decides everything the player sees.

---

## Highlights

| Area | What's in |
|---|---|
| **LLM narrative engine** | The NPC talks through `MALLMStreamingClient` (SSE over HTTP, `thinking:{disabled}` for instant first tokens, server-side echo scrub, keep-alive across window close so an in-flight tool call still lands) and **acts** through 4 declared tools: `give_quest` / `trigger_raid` / `grant_blessing` / `adjust_favor`. Streamed tool-call fragments reassemble via an index-sparse `FMAToolCallAggregator`. **The LLM proposes; the server disposes**: `MANarrativeSubsystem::ExecuteToolCall` runs whitelist → parameter clamping (kills 1–10, time limit 0\|60–600 s, raid count 1–6, blessing duration 30–180 s) → per-match budgets + shared cooldowns + match-end lockout → favor gates. Rules cores (`MAQuestRules` / `MAWorldEventRules` / `MAFavorRules`) are pure and unit-tested. |
| **Story mode loop** | Arena is empty until the NPC issues a quest. Quest starts anchor to **dialogue close**: the giver departs (Niagara vanish FX) → 2.5 s beat → an enemy ring spawns and the deadline stamps; kill attribution feeds quest progress; on clear/expiry the enemies clean up, a 3 s beat passes, and the giver returns (FX) with a reward. Every beat multicasts a bottom-center subtitle via `AMAGameState`. Buff GEs (`GE_Buff_Speed`/`GE_Buff_Attack`) and the heal reward are **runtime-constructed transient `UGameplayEffect`s** — no content refs in C++. |
| **Favor economy** | `NpcFavor` on PlayerState, clamped [−10, +10], per-call delta ±2. Trust ≥ +3 unlocks battle intel in the system prompt and the `grant_blessing` tool; ≤ −3 mutes every tool except apology-driven `adjust_favor`. The system prompt is rebuilt **per message** from live quest state + favor attitude, so the NPC's knowledge always matches the world. |
| **LLM tactical advisor** | Server-only `UMATacticalAdvisorSubsystem` periodically summarises the battle for the LLM and applies its structured decision — attack-slot override into the combat director, focus-target suggestion into `BTService_UpdateTargetInfo` (validated, auto-fallback), and a taunt line into the kill feed. Strict layering: LLM is the cognition layer, the BT reflex layer never waits on it — timeout/nonsense falls back to defaults, no frame ever blocks. Decision parsing (`MAAdvisorDecision`) is pure and unit-tested. |
| **API-key security** | The Moonshot key never enters the repo or the package: read from the `MOONSHOT_API_KEY` env var (with an `HKCU\Environment` registry fallback), held **host-side only** — clients talk to the listen server, never to the LLM. Packaged builds ship `SetupApiKey.bat` (a `setx` helper) for distribution. |
| **GAS 5 pillars** | `GameplayAbility` (LocalPredicted ×6: Melee-combo / Sprint / Dodge / Fireball / Block / Dash-slash + ServerInitiated HitReact) · `GameplayEffect` (Damage Execution / Cooldown / Stamina Cost / Periodic Stamina Regen / runtime-built narrative buffs with **SetByCaller damage multipliers**) · `AttributeSet` (Health/MaxHealth/Stamina/AttackPower/Armor + Damage meta) · `GameplayCue` (`Static` notify w/ `FHitResult` location/normal + data-driven C++ burst cue base) · `PredictionKey` |
| **Katana combo (4-stage)** | One ability instance + one multi-section **full-body root-motion** montage (`AM_KatanaCombo_GS`). Re-presses while swinging buffer through the **GAS generic replicated `InputPressed` event** (`AbilityLocalInputPressed` InputID routing) — a press QUEUE, so N mashes yield N chained swings on client AND server with no custom RPC. Each section's `ComboWindow` notify opens a **window-period**: queued presses chain at open, later presses chain instantly (recovery cancel) via `MontageJumpToSection` — section state replicates through `FGameplayAbilityRepAnimMontage`, so a misprediction corrects as a within-montage section snap, never an ability rollback. Each swing carries a per-section **Motion Warping** window that lunges onto the locked/cone target via the shared `MAAttackLunge`/`MAWarpOps` helper (whiff distance-capped). |
| **Defense & reactions** | **Hold-to-block** (RMB): `State.Blocking` gates a 70% mitigation read from captured target tags inside the damage ExecCalc; a **full-body katana guard loop** (`AM_KatanaGuardLoop`) holds the pose and each absorbed hit overlays a parry react (`AM_KatanaGuardHit`). **Staggers**: `GA_HitReact` (ServerInitiated, triggered by a `Event.Damage.Taken` gameplay event raised in the AttributeSet, `bRetriggerInstancedAbility`) — every hit flinches and interrupts the victim's combo; suppressed while blocking/casting/dodging. |
| **Lock-on & 8-way strafe** | `UMALockOnComponent` soft-lock (**R3 / Middle-Mouse** toggle, right-stick flick switches target) — the locked target replicates, so simulated proxies strafe correctly. While locked, `UMAAnimInstance` derives `Direction` + `bStrafing` from replicated velocity-vs-facing and `BlendPosesByBool` into an **8-way katana strafe blendspace** (`BS_Katana_Strafe`) over a katana combat idle. |
| **Dash-slash (Motion Warping)** | `UGA_DashSlash` (**E / gamepad RB**): a root-motion katana dash warped onto the aim/locked target via the shared `MAAttackLunge::PickLungeTarget` → `MAWarpOps` SkewWarp setup — the **same helper drives per-swing combo warp**, so both resolve identically on client and server. A whiffed dash plays its authored step-in, capped at a forward no-target distance. |
| **Authored deaths** | Death clips play single-node (bypassing the ABP, no montage slot), then **hand the corpse to ragdoll as the clip ends** — physics settles the in-place final pose onto the ground. AI corpses stop thinking (brain stop + blackboard wipe — no posthumous target tracking, no phantom attack on respawn). |
| **Lua, three layers (UnLua)** | A deliberate C++/Lua split for live-ops-style tuning: **① C++ combat core** — hot paths never touch `lua_State`. **② Lua config** (`Content/Script/AbilityConfig.lua`): ability tunables (melee trace/warp, dash warp, fireball AoE, montage rates, **player-vs-AI damage multipliers** fed through SetByCaller into the ExecCalc) parsed once into a C++ cache on a GameInstance subsystem; **saving the file hot-reloads it** (1 s timestamp poll, non-shipping) — no recompile, no restart — plus a `MA.Lua.ReloadAbilityConfig` console fallback; bad edits keep the last good cache, missing keys fall back to `UPROPERTY` defaults. **③ Lua UI** (`Content/Script/UI/QuestJournal.lua`, `EscMenu.lua`): C++ widgets implement `IUnLuaInterface` and hand presentation logic to Lua modules — layout, refresh cadence, button wiring all editable without touching C++. Runs on a **vendored UnLua 2.3.6 ported to UE5.5**. |
| **Dialogue & narrative UI** | `MADialogueWidget` — streaming subtitle text, animated thinking indicator, and a dedicated **quest button** (disabled while a quest is active or a reply is in flight); NPC **overhead nameplate** advertises "按 T 对话 · 可接任务" in radius; right-side **quest tracker** HUD; **J** opens the Lua-driven quest journal (quest / countdown / active buffs / favor attitude / K-D); **ESC** opens the Lua-driven pause menu (resume / journal / return to main menu / quit) with gamepad bindings (**Special Left/Right**). Announce subtitles and per-location Niagara FX ride GameState multicasts. |
| **Deathmatch loop (retained, off by default)** | The engine-`MatchState` deathmatch — replicated clock / kill target / verdict, kill feed, "KILLED BY", hold-Tab scoreboard doubling as pinned results, auto-restart — is intact but story mode ships with `MatchDuration`/`KillTarget` ≤ 0, which disables the clock and score endings. Set both > 0 in the GameMode defaults to flip the arena back into a 5-minute / first-to-N deathmatch. Kill attribution (effect-context instigator → `CheckDeath` → GameMode router) now also feeds quest progress. |
| **Server-side lag compensation** | `UMALagCompSubsystem` (`UTickableWorldSubsystem`) records every combatant's capsule into a per-frame **time ring buffer**; the melee hit path (`MAMeleeHitOps`, gated on `ma.LagComp.Enabled`) rewinds targets to the attacker's client-seen moment (ping + interpolation delay) and runs a **swept-sphere-vs-capsule** geometric intersection, with a **favor-the-shooter** rewind cap (`ma.LagComp.MaxRewindSeconds` 0.3). Pure-geometry core, fully unit-tested. |
| **Prediction-correction metrics** | `UMAPredictionMovementComponent` subclasses the CMC and overrides `OnClientCorrectionReceived` to measure server reconciliation (correction frequency + position error) into a pure `FMACorrectionTracker`; `ma.PredDebug` draws an on-screen readout and a **red/green ghost** of the pre/post-correction transforms. |
| **Hit feel** | `MAHitFeel` (`ma.HitFeel` master toggle): **hit-stop** by briefly freezing the mesh anim rate — never global time dilation, the rest of the sim keeps running — plus weight-scaled **camera shake** (`MACameraShakes` light/heavy), impact SFX and knockback, fired through the `GCN_MeleeImpact` cue. |
| **AI combat director** | Server-only `UMACombatDirectorSubsystem` hands out a bounded number of **attack tokens** (`MAAttackTokenLedger`, `ma.AI.MaxAttackers`, `ma.AI.TokenTTL`) so swarming AI take turns instead of gang-piling: `BTTask_ClaimAttackToken`/`ReleaseAttackToken` gate the melee node, `BTTask_CircleTarget` circle-strafes while waiting. `MAAIDefenseComponent` + `MAAIDefensePolicy` raise a guard on incoming swings by activating the **same `GA_Block`** the player uses; AI are faction-immune to each other. Ledger + policy are pure, unit-tested. The tactical advisor sits above this as an optional cognition layer. |
| **VFX & hand-authored material** | Narrative FX (giver vanish/appear, enemy spawn) are Niagara burst systems rendering `M_MANarrativeGlow` — a **hand-authored sprite material built through MCP material-graph tools**: an HLSL `Custom` node (radial core + halo + 4-point star falloff) × ParticleColor into additive-unlit emissive, with the Niagara usage flag set explicitly (the flag Shipping silently refuses to compile without — editor viewport forgives, packaged builds don't). |
| **Win64 packaging** | `BuildCookRun -pak -nodebuginfo` Shipping build, ~495 MB (pak + Oodle; loose staging was 958 MB *and* shipped every asset readable). Cook lessons captured: string-referenced maps need `MapsToCook`/`DirectoriesToAlwaysCook` (the cook graph doesn't follow soft string refs), editor-module native gameplay tags become cook errors (moved to ini tag registration), first-load PSO/shader warmup ≠ a hang. |
| **Automated test suite** | **22 UE Automation tests** over the pure cores, all headless: `MultiPlayerAction.LagComp.*` (×5 geometry/history/rewind), `.LLM.*` (×5: request-body serialization incl. tools + thinking flags, SSE parser, OpenAI chunk decode, tool-call parse, fragment aggregator), `.Narrative.*` (×3: quest / world-event / favor rules), `.LuaConfig.*` (×3: cache lookup, real-`lua_State` bridge parse, subsystem reload), `.AICombat.*` (×3: token ledger, defense policy, advisor decision), `.Lunge.TargetSelection`, `.Prediction.CorrectionTracker`, `.HitFeel.ComputeHitFeel`. |
| **Weapon & animation set** | Katana on a `hand_r` socket + the **GhostSamurai** clip set (4-stage attack root-motion, 16-way walk/run strafe, guard loop + parry, idle) **IK-retargeted UE4→UE5 onto the Manny skeleton through a fully programmatic pipeline** (`IK_UE4Manny` + `RTG_UE4Manny_to_UE5`), productized as the MCP plugin's `retarget_animations` tool. |
| **Projectile netcode** | Lyra/GASShooter-style ranged AoE: predicted cast montage (instant client feedback) + **server-only spawn** of a replicated `AMAProjectile` carrying a damage spec **snapshotted at cast time** — the fireball lands with cast-time stats even if the caster dies mid-flight. AoE overlap applies the same ExecCalc to every ASC in radius; explosion FX replicate via GameplayCue. |
| **Damage formula** | `UGameplayEffectExecutionCalculation` capturing source `AttackPower` (snapshot) + target `Armor` (live) + a **SetByCaller `Data.DamageMultiplier`** (Lua-tuned per player/AI), output to `Damage` meta-attribute, AS routes to `Health`. Lyra `ULyraDamageExecution` pattern. |
| **HUD architecture** | Self-contained C++ Views auto-wired by `UMAUserWidget::InitFromASC` tree scan: `UMAHealthBarWidget` (Street-Fighter-style **chip damage bar**) and `UMASkillSlotWidget` (cooldown sweep + active-tag highlight). The **same chip-bar widget** doubles as the NPC overhead bar. The match/narrative UI layer (clock/K-D overlay, kill feed, scoreboard, quest tracker, announce, dialogue, nameplate) is **entirely code-built** — `WidgetTree::ConstructWidget` in C++, zero BP assets; the journal and ESC menu add the Lua presentation layer on top. Touch controls + DPI/safe-area scaling for mobile targets. |
| **Animation** | Upper-body layering rig in `ABP_Manny` (cached full pose → `UpperBody` slot → layered blend per bone on `spine_01`) keeps the legs running under the **fireball cast**; `UMAAnimInstance` adds a **torso aim twist** (spine-chain yaw toward camera, clamped ±90°, State-tag gated). |
| **AI** | `BehaviorTree` + custom `UBTService_UpdateTargetInfo` (nearest **living** player, preferring **NavMesh-reachable** targets, advisor-focus aware) + custom `UBTTask_TryActivateAbilityByTag`. AI drives the **same `UGA_MeleeAttack` katana class** the player uses via Enhanced Input, coordinated by the attack-token director. Quest enemies spawn from the narrative subsystem into a ring around the arena. |
| **Session front-end** | `UMASessionSubsystem` (GameInstance subsystem over the OnlineSubsystem session interface) with a UMG **MVVM** main menu — `UMAMainMenuViewModel` + FieldNotify bindings, ListView of discovered sessions, host/join/refresh with in-flight guards and network-failure recovery back to the menu. |
| **Networking** | Server-authoritative damage; `Mixed` ASC replication for players (cooldown to owner) + `Minimal` for NPCs. `NetMulticast` RPCs for death visuals, kill events, narrative announcements and FX. `Server`/`Client` RPCs for dialogue, dev cheats and the respawn countdown deadline. |
| **Death/Respawn** | `IMACombatantInterface` abstraction; `State.Dead` loose tag gates re-activation; `UnPossess()` before `GameMode->RestartPlayer()` to force fresh pawn spawn. The owning client renders a respawn countdown off a server-time deadline pushed once over a `Client` RPC. |

---

## Module layout

```
Source/MultiPlayerAction/
├── MultiPlayerActionCharacter.{h,cpp}     Player pawn: input bindings, GAS init via PlayerState, weapon mesh, authored death
├── MultiPlayerActionGameMode.{h,cpp}      Match rules: kill router (scoring + quest progress), optional deathmatch endings
├── MAGameState.{h,cpp}                    Replicated match data + kill / announce / narrative-FX / taunt multicasts
├── LLM/
│   ├── MALLMTypes.h                       Message / role / tool-call / tool-spec PODs
│   ├── MALLMSettings.h                    Config (model, max tokens, thinking + temperature policy)
│   ├── MALLMRequestBody.{h,cpp}           Pure OpenAI-compatible request serialization (messages + tools) — unit-tested
│   ├── MASSEStream.{h,cpp}                Pure SSE line parser + chunk decode + FMAToolCallAggregator — unit-tested
│   └── MALLMStreamingClient.{h,cpp}       Streaming HTTP client: deltas, tool-call assembly, key resolution (env + registry)
├── Narrative/
│   ├── MAQuestTypes.h                     Quest state PODs (BlueprintType — the Lua/BP read seam)
│   ├── MAQuestRules.{h,cpp}               Pure quest validation / lifecycle rules — unit-tested
│   ├── MAWorldEventRules.{h,cpp}          Pure raid & blessing budgets / cooldowns / clamps — unit-tested
│   ├── MAFavorRules.{h,cpp}               Pure favor clamps + trust/mute gates + attitude text — unit-tested
│   └── MANarrativeSubsystem.{h,cpp}       Server orchestrator: tool specs, ExecuteToolCall pipeline, quest lifecycle
│                                          pacing, enemy ring spawn, runtime GE construction, announcements, FX
├── Dialogue/
│   ├── MADialogueSubsystem.{h,cpp}        Server dialogue broker: per-message system prompt, history trim (tool-pair
│   │                                      safe), echo scrub, keep-alive across window close
│   └── MADialogueComponent.{h,cpp}        NPC-side: interaction radius, nameplate, quest-enemy class, narrative FX playback
├── AbilitySystem/
│   ├── MAAbilitySystemComponent.h         Custom ASC subclass (extension hook)
│   ├── MAAbilityInputID.h                 InputID enum for AbilityLocalInputPressed routing (combo input buffer)
│   ├── MAAttributeSet.{h,cpp}             Replicated attrs + Damage meta + PostExecute clamp/stagger event + CheckDeath
│   ├── MACombatantInterface.h             "anything that dies" abstraction, called by AS::CheckDeath
│   ├── MAGameplayTags.{h,cpp}             Native gameplay tags (Ability.* / GameplayCue.* / State.* / Event.* / Data.*)
│   ├── AnimNotifies/AN_SendGameplayEvent.{h,cpp}
│   ├── Abilities/                         MAGameplayAbilityBase + GA_MeleeAttack / HitReact / Block / Sprint / Dodge /
│   │                                      Fireball / DashSlash (base reads the Lua damage-multiplier per player/AI)
│   ├── Cues/                              GCN_ParticleBurst (data-driven burst base) + GCN_MeleeImpact (hit feel)
│   └── Executions/MADamageExecutionCalculation.{h,cpp}   AP snapshot × armor × block × SetByCaller multiplier
├── Config/
│   ├── MAConfigTypes.h                    Ability tunable PODs
│   ├── MALuaBridge.{h,cpp}                Real-lua_State table parse into C++ structs — unit-tested
│   └── MALuaAbilityConfig.{h,cpp}         GameInstance subsystem cache + save-triggered auto hot-reload + console reload
├── Combat/                                MAProjectile · MAMeleeHitOps (lag-comp entry) · MAAttackLunge · MAWarpOps ·
│                                          MAHitFeel · MACameraShakes
├── Targeting/MALockOnComponent.h          Soft lock-on: target pick / cycle, replicated current target
├── Network/                               MALagCompGeometry/Subsystem (rewind hit resolve) · MACorrectionTracker ·
│                                          MAPredictionMovementComponent (reconciliation metrics + ghost)
├── Player/
│   ├── MAPlayerState.{h,cpp}              ASC + AttributeSet owner; replicated Kills/Deaths/ActiveQuest/NpcFavor;
│   │                                      BlueprintPure read seam for Lua UI (quest state / favor / active buff lines)
│   └── MAPlayerController.{h,cpp}         Owns all local UI; key routing (T dialogue · J journal · ESC menu · gamepad);
│                                          menu actions (resume / return to menu / quit) exposed BlueprintCallable for Lua
├── Online/MASessionSubsystem.{h,cpp}      OnlineSubsystem sessions (host/find/join, failure recovery)
├── Animation/MAAnimInstance.{h,cpp}       Torso aim twist + strafe params
├── UI/
│   ├── MAUserWidget / MAHealthBarWidget / MASkillSlotWidget          HUD core (chip bar, skill slots)
│   ├── MAMatchStatusWidget / MAScoreboardWidget / MAKillFeedWidget   Deathmatch layer (auto-hides when disabled)
│   ├── MADialogueWidget.{h,cpp}           Streaming dialogue: subtitles, thinking indicator, dedicated quest button
│   ├── MANpcNameplateWidget.{h,cpp}       NPC overhead name + interaction hint (radius-gated)
│   ├── MAQuestTrackerWidget.{h,cpp}       Right-side quest HUD (progress + countdown)
│   ├── MAAnnounceWidget.{h,cpp}           Bottom-center narrative subtitles (GameState multicast)
│   ├── MAQuestJournalWidget.{h,cpp}       Skeleton container + AddLine API — logic lives in Lua (UI.QuestJournal)
│   ├── MAEscMenuWidget.{h,cpp}            Skeleton container + AddButton API — logic lives in Lua (UI.EscMenu)
│   ├── MALockOnReticleWidget / MATouchControlsWidget                 Lock-on reticle · mobile touch controls
│   └── MainMenu/                          MVVM front-end (ViewModel + FieldNotify + session ListView)
├── AI/
│   ├── MATargetDummy / MAEnemyController  Pawn-owned ASC (Minimal rep), BT bootstrap
│   ├── BTTask_TryActivateAbilityByTag / BTService_UpdateTargetInfo (advisor-focus aware)
│   ├── BTTask_ClaimAttackToken / BTTask_ReleaseAttackToken / BTTask_CircleTarget
│   └── Combat/
│       ├── MACombatDirectorSubsystem.{h,cpp}     Server attack-token broker
│       ├── MAAttackTokenLedger.{h,cpp}           Pure attack-slot ledger — unit-tested
│       ├── MAAIDefensePolicy.{h,cpp} + MAAIDefenseComponent.{h,cpp}  Event-driven guard (reuses GA_Block)
│       ├── MAAdvisorDecision.{h,cpp}             Pure LLM-decision parse/sanitize — unit-tested
│       └── MATacticalAdvisorSubsystem.{h,cpp}    Server-only LLM advisor (slots / focus / taunt, fail-open)
└── Tests/                                 22 automation tests: MALagCompGeometryTest · MASSEStreamTest ·
                                           MALLMRequestBodyTest · MAQuestRulesTest · MAWorldEventRulesTest ·
                                           MAFavorRulesTest · MALuaConfigTest · MAConfigLookupTest ·
                                           MAAttackTokenLedgerTest · MAAIDefensePolicyTest · MAAdvisorDecisionTest ·
                                           MAAttackLungeTest · MACorrectionTrackerTest · MAHitFeelTest
```

Content highlights under `Content/`:
- `Script/` — the Lua layer: `AbilityConfig.lua` (combat tunables, save-to-hot-reload), `UI/QuestJournal.lua`, `UI/EscMenu.lua` (UnLua widget modules)
- `VFX/` — `M_MANarrativeGlow` (hand-authored HLSL sprite material) + `NS_GiverVanish` / `NS_EnemySpawn` Niagara systems
- `AbilitySystem/` — GA/GE/Cue Blueprints (pure config on the C++ classes), `AM_KatanaCombo_GS` + guard/cast/react montages
- `AnimLibrary/GhostSamurai/` — UE4→UE5 retarget rigs + the retargeted katana clip set + `BS_Katana_Strafe`
- `Blueprints/` — `BP_TargetDummy`, `BP_EnemyController`, `BP_Projectile_Fireball`, `WBP_HUD`/`WBP_MainMenu` (MVVM); the match/narrative overlay widgets have **no assets** — C++-built
- `AI/` — `BB_Enemy`, `BT_Enemy`

> The bulk of the content above was authored **agent-side via UnrealAgentMCP** (private repo): montages, AnimNotifies, GE configuration, BehaviorTrees, UMG trees + MVVM bindings, AnimGraph surgery, Niagara systems, the HLSL material graph and the arena dressing all happened through MCP tools — several of which were built precisely because this project needed them. That dogfooding loop is the second half of the portfolio.

---

## Build

Requirements:
- Unreal Engine **5.5** (matches `MultiPlayerAction.uproject` `EngineAssociation`)
- Visual Studio 2022 (Windows) / Xcode 15+ (macOS) with C++ workload
- Optional: a **Moonshot API key** for the LLM NPC — set the `MOONSHOT_API_KEY` env var on the host machine (get one at platform.moonshot.cn). Without it, combat and quests-off exploration work normally and the dialogue window reports the missing key; the key is never committed, never packaged, and never sent to clients.
- Optional: Starter Content (particle templates for the burst cues)

Steps:
```
git clone https://github.com/alertform/ue5-multiplayer-action.git
cd ue5-multiplayer-action
# Right-click MultiPlayerAction.uproject → "Generate Visual Studio project files"
# Open MultiPlayerAction.sln, set MultiPlayerActionEditor as startup, Build.
```

Package (Win64 Shipping, pak + Oodle, ~495 MB):
```
"<UE_5.5>/Engine/Build/BatchFiles/RunUAT.bat" BuildCookRun \
  -project="<abs>/MultiPlayerAction.uproject" -platform=Win64 -clientconfig=Shipping \
  -cook -build -stage -pak -nodebuginfo -package -archive -archivedirectory="<out>"
```
Packaged builds include `SetupApiKey.bat` — a one-prompt `setx` helper so a playtester can install their own key without touching the registry by hand.

### Playing it (story mode)

Host from the main menu (or PIE from `Content/Maps/ThirdPersonMap`, Number of Players ≥ 2 to exercise replication). The arena starts **empty** — walk up to the wandering swordsman (overhead nameplate), press **T** to talk. Chat freely — the NPC streams its replies and remembers the conversation — or press the dedicated **quest button** to ask for work. Accept a quest, close the dialogue, and watch the beat play out: the swordsman departs in a burst of light, enemies materialize in a ring, the quest tracker and deadline appear. Clear them (or run out of time) and he returns with a verdict, a reward, and a grudge or a smile — favor is a real number; earn **+3** and he starts sharing battle intel and granting speed/attack blessings, hit **−3** and he stops taking your calls.

Combat: **LMB** 4-hit katana combo (mash to chain, press in the window to cancel recovery), **RMB hold** block, **E** dash-slash, **Q** fireball, **Shift** sprint, **Ctrl** dodge i-frame, **R3 / Middle-Mouse** lock-on. UI: **J** quest journal (quest / countdown / active buffs / favor / K-D — Lua-driven), **ESC** pause menu (Lua-driven; gamepad **Special Left/Right** = journal/menu), **Tab** scoreboard. Tune combat live: edit `Content/Script/AbilityConfig.lua` and save — values hot-reload within a second, no recompile.

To restore the original deathmatch: set `MatchDuration`/`KillTarget` > 0 on the GameMode.

### Tests

22 UE Automation tests over the pure cores — lag-comp geometry, LLM request/SSE/tool-call protocol, narrative rules (quest/world-event/favor), Lua config bridge, AI token ledger / defense policy / advisor decision, lunge target pick, reconciliation stats, hit-feel curve — engine-independent logic that runs headless. From the editor's **Session Frontend → Automation**, or:
```
"<UE_5.5>/Engine/Binaries/Win64/UnrealEditor-Cmd.exe" \
  "<absolute-path>/MultiPlayerAction.uproject" \
  -ExecCmds="Automation RunTests MultiPlayerAction; Quit" -unattended -nullrhi -nop4 -log
```
Suites: `MultiPlayerAction.{LagComp,LLM,Narrative,LuaConfig,AICombat,Lunge,Prediction,HitFeel}.*`.

---

## Notes for reviewers

- Code leans toward **C++ first, Blueprint composition only** — engine-level work, not BP scripting. The Lua layer is a deliberate seam (config + UI presentation), not a scripting escape hatch: combat hot paths never touch `lua_State`.
- The LLM is treated as an **untrusted input source**: everything it emits goes through pure, unit-tested validation rules before touching gameplay, all mutations are server-side, and the API key never leaves the host process (env/registry read, no repo, no pak, no client replication).
- Architecture decisions follow Lyra conventions where applicable (HUD widget binding, NPC ASC ownership, `Damage` meta-attribute routing, BT + GAS integration, replication modes).
- Multiplayer correctness validated in PIE listen-server with 2 players; the packaged Shipping build is smoke-tested end-to-end (host, dialogue, quest loop). Shipping-only failure modes worth knowing: material usage flags the editor forgives, ensure-as-error during cook, soft string refs the cook graph won't follow, first-load PSO warmup that looks like a hang.
- Dev gotchas captured in commit messages — UE 5.5 deprecations, the `RestartPlayer` teleport-existing trap, `NetMulticast` for component state, montage blend-out no-op windows, `bWarningsAsErrors` shadow-name pitfalls.

---

## License

No license declared yet — reach out before reusing code.
