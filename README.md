# ue5-multiplayer-action

Lyra-style multiplayer action demo built on Unreal Engine 5.5 — full **Gameplay Ability System** (GAS) stack, a sword-combo combat kit (3-hit chains, block, staggers, authored deaths), a complete **deathmatch loop** (K/D, kill feed, scoreboard, win conditions, auto-restart), and AI driven by **BehaviorTree** sharing the same `GameplayAbility` C++ classes as the player. Personal portfolio project.

> Built alongside (and largely *with*) **UnrealAgentMCP** — a self-developed in-editor MCP server (59 tools / 64 automation tests) that lets an AI agent author Blueprints, UMG, AnimGraphs, BehaviorTrees, montages, IK retargeting and level content directly inside the running editor. Most of this project's content-side work was authored agent-side through it. The plugin is developed in a separate **private** repo (demo available on request); it is not required to build or run this project.

| | |
|---|---|
| ![Arena vista](docs/screenshots/arena_vista.png) | ![Arena rocks](docs/screenshots/arena_rocks.png) |
| ![Combat + kill feed](docs/screenshots/combat_killfeed.png) | ![Scoreboard](docs/screenshots/scoreboard.png) |
| ![Ragdoll + kill feed](docs/screenshots/ragdoll_killfeed.png) | ![Post-match results](docs/screenshots/postmatch.png) |

---

## Highlights

| Area | What's in |
|---|---|
| **GAS 5 pillars** | `GameplayAbility` (LocalPredicted ×5: Melee-combo / Sprint / Dodge / Fireball / Block + ServerInitiated HitReact) · `GameplayEffect` (Damage Execution / Cooldown / Stamina Cost / Periodic Stamina Regen) · `AttributeSet` (Health/MaxHealth/Stamina/AttackPower/Armor + Damage meta) · `GameplayCue` (`Static` notify w/ `FHitResult` location/normal + data-driven C++ burst cue base) · `PredictionKey` |
| **3-hit sword combo** | One ability instance + one multi-section montage. Re-presses while swinging buffer through the **GAS generic replicated `InputPressed` event** (`AbilityLocalInputPressed` InputID routing) — a press QUEUE, so N mashes yield N chained swings on client AND server with no custom RPC. Each section's `ComboWindow` notify opens a **window-period**: queued presses chain at open, later presses chain instantly (recovery cancel) via `MontageJumpToSection` — section state replicates through `FGameplayAbilityRepAnimMontage`, so a misprediction corrects as a within-montage section snap, never an ability rollback. |
| **Defense & reactions** | **Hold-to-block** (RMB): `State.Blocking` gates a 70% mitigation read from captured target tags inside the damage ExecCalc; a looping guard-stance montage holds the pose and each absorbed hit overlays a block-hit react. **Staggers**: `GA_HitReact` (ServerInitiated, triggered by a `Event.Damage.Taken` gameplay event raised in the AttributeSet, `bRetriggerInstancedAbility`) — every hit flinches and interrupts the victim's combo; suppressed while blocking/casting/dodging. |
| **Authored deaths** | Death clips play single-node (bypassing the ABP, no montage slot), then **hand the corpse to ragdoll as the clip ends** — physics settles the in-place final pose onto the ground. AI corpses stop thinking (brain stop + blackboard wipe — no posthumous target tracking, no phantom attack on respawn). |
| **Deathmatch loop** | Engine **MatchState machine** (no hand-rolled phase enum): replicated match clock / kill target / verdict on `AMAGameState`, countdowns derived from `GetServerWorldTimeSeconds` (zero per-second RPCs). Kill attribution flows from the **effect-context instigator** through `CheckDeath` into the GameMode's kill router — suicides count the death but never the kill. K/D live on PlayerState; kills broadcast via `NetMulticast` → local delegate into a **kill feed** and the victim's "KILLED BY" line. Hold-**Tab** scoreboard doubles as the pinned post-match results (gold winner row, restart countdown); `RestartGame()` reloads the map — fresh scores, the LAN session survives on the GameInstance. |
| **Weapon & animation set** | KayKit (CC0) sword on a `hand_r` bone socket + 76 KayKit clips **IK-retargeted to the UE5 Manny skeleton through a fully programmatic pipeline** (auto-characterize → anatomical chain alignment → exact T-pose retarget pose from per-bone local deltas → finger track strip), productized as the MCP plugin's `retarget_animations` tool. |
| **Projectile netcode** | Lyra/GASShooter-style ranged AoE: predicted cast montage (instant client feedback) + **server-only spawn** of a replicated `AMAProjectile` carrying a damage spec **snapshotted at cast time** — the fireball lands with cast-time stats even if the caster dies mid-flight. AoE overlap applies the same ExecCalc to every ASC in radius; explosion FX replicate via GameplayCue. |
| **Damage formula** | `UGameplayEffectExecutionCalculation` capturing source `AttackPower` (snapshot) + target `Armor` (live), output to `Damage` meta-attribute, AS routes to `Health`. Lyra `ULyraDamageExecution` pattern. |
| **HUD architecture** | Self-contained C++ Views auto-wired by `UMAUserWidget::InitFromASC` tree scan: `UMAHealthBarWidget` (Street-Fighter-style **chip damage bar** — front fill drops instantly, a chip segment accumulates hits for 3 s then drains; styles force-built in `NativePreConstruct`) and `UMASkillSlotWidget` (cooldown sweep from active cooldown GE query + active-tag highlight). The **same chip-bar widget** doubles as the NPC overhead bar (`bHideUntilDamaged` — appears on first blood, drains and vanishes on death, re-hides at full). The match UI layer (clock/K-D overlay, kill feed, scoreboard) is **entirely code-built** — `WidgetTree::ConstructWidget` in C++, spawned straight from the class, zero BP assets. |
| **Animation** | Upper-body layering rig in `ABP_Manny` (cached full pose → `UpperBody` slot → layered blend per bone on `spine_01`) keeps the legs running while attack/cast montages play. `UMAAnimInstance` adds a **torso aim twist**: while `State.Attacking`/`State.Casting` is on the ASC, the spine chain rotates toward the camera yaw (clamped ±90°, interp-smoothed) — legs keep orient-to-movement, body language follows the crosshair. |
| **AI** | `BehaviorTree` + custom `UBTService_UpdateTargetInfo` (nearest **living** player, preferring **NavMesh-reachable** targets over closer unreachable ones) + custom `UBTTask_TryActivateAbilityByTag` (BT node → `ASC.TryActivateAbilitiesByTag`). AI calls the **same `UGA_MeleeAttack` C++ class** the player drives via Enhanced Input. |
| **Session front-end** | `UMASessionSubsystem` (GameInstance subsystem over the OnlineSubsystem session interface) with a UMG **MVVM** main menu — `UMAMainMenuViewModel` + FieldNotify bindings, ListView of discovered sessions, host/join/refresh with in-flight guards and network-failure recovery back to the menu. |
| **Networking** | Server-authoritative damage; `Mixed` ASC replication for players (cooldown to owner) + `Minimal` for NPCs. `NetMulticast` RPCs for death visuals and kill events (component state doesn't auto-replicate). `Server`/`Client` RPCs for dev cheats and the respawn countdown deadline. |
| **Death/Respawn** | `IMACombatantInterface` abstraction; `State.Dead` loose tag gates re-activation; `UnPossess()` before `GameMode->RestartPlayer()` to force fresh pawn spawn (vs the engine's default teleport-existing). The owning client renders a respawn countdown off a server-time deadline pushed once over a `Client` RPC. |

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
     ├─ Spawns + binds UMAUserWidget (WBP_HUD)                  └─ RunBehaviorTree(BT_Enemy)
     └─ DamageSelf (Server RPC) for testing                          ├─ Service: BTService_UpdateTargetInfo
                                                                     └─ BTTask_TryActivateAbilityByTag
                                                                            (AbilityTag = Ability.Melee.Attack)
                                                                            ↓
                                                                     Same UGA_MeleeAttack C++ class as player
```

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
                    SERVER: PerformHitTrace → SphereSweep ECC_Pawn (camera-yaw aim)
                                ↓
                    Apply BP_GE_Damage (uses MADamageExecutionCalculation)
                                ↓
                    Source.AttackPower (snapshot) × (1 - Target.Armor × 0.05)
                            × 0.3 if target State.Blocking (captured target tags)
                                ↓
                    Output Damage meta-attribute on target ASC
                                ↓
                    Target AS::PostGameplayEffectExecute routes Damage → Health
                       ├─ survivor: Event.Damage.Taken → GA_HitReact (ServerInitiated stagger,
                       │     retriggerable — interrupts the victim's combo; blocked while
                       │     State.Blocking: GA_Block plays its absorb overlay instead)
                       └─ Health<=0: AS::CheckDeath(ASC, instigator)
                                ↓
                    State.Dead + CancelAbilities + Execute_HandleDeath + GameMode::NotifyKill
                                ↓
                    Multicast_PlayDeath: authored death clip (single-node) → ragdoll handoff
                    at clip end · AI also: brain stop + blackboard wipe
                                ↓
                    ScheduleRespawn(3s) + Client respawn countdown → UnPossess +
                    GameMode.RestartPlayer → fresh pawn at PlayerStart
```

### Match loop (deathmatch)

```
AGameMode MatchState machine: WaitingToStart → InProgress → WaitingPostMatch
     ↓ HandleMatchHasStarted: GameState.MatchEndServerTime = now + 5min
       (replicated once — every client renders the clock off GetServerWorldTimeSeconds)
each kill: CheckDeath(ASC, effect-context instigator) → GameMode::NotifyKill
     ├─ PlayerState.Kills/Deaths (replicated; suicide counts the death, never the kill;
     │   dummy kills credit the killer so the loop demos solo)
     └─ GameState.Multicast_OnKill → kill feed line + "KILLED BY" on the victim's HUD
ReadyToEndMatch (engine Tick poll): clock expired OR kill target (10) reached
     ↓ HandleMatchHasEnded: verdict (kills desc / deaths asc; tie = DRAW) written BEFORE
       the state flips — same actor, same frame, same replication bunch as MatchState,
       so no client ever renders a results screen with a missing winner
     ↓ combat freeze: abilities cancelled, dummy brains stopped, respawns suppressed;
       AMAGameState::HandleMatchHasEnded runs on EVERY machine → local PC drops to
       UI-only input and pins the scoreboard as the results screen (winner row gold)
     ↓ +10s RestartGame() — full (non-seamless) reload of the same map: fresh GameState +
       PlayerStates (scores zeroed by construction); the NULL-OSS session lives on the
       GameInstance and survives the travel
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
     ↓ (release-frame AnimNotify, ~1.65s — windup masks the projectile's replication latency)
SERVER ONLY: snapshot damage spec (source AttackPower captured NOW)
     ↓
Aim = camera-ray impact point (heights work; ground shots are intentional AoE placement)
     ↓
SpawnActorDeferred<AMAProjectile> (bReplicates + movement replication) → InitProjectile(spec, radius)
     ↓
Impact (server): SphereOverlap(ECC_Pawn, r=300) — instigator excluded
     ↓
Apply snapshotted spec to every ASC in radius (same MADamageExecutionCalculation as melee)
     ↓
Source ASC ExecuteGameplayCue(GameplayCue.Fireball.Explosion) → replicated burst FX, then Destroy
```

### HUD binding (Lyra-style)

```
PlayerController.BeginPlay
     ↓ CreateWidget<UMAUserWidget>(WBP_HUD) + AddToViewport
PlayerController.OnRep_PlayerState
     ↓ HUDWidget->InitFromASC(PS->GetAbilitySystemComponent())
                  ↓
UMAUserWidget::InitFromASC binds:
   ASC->GetGameplayAttributeValueChangeDelegate(Health/MaxHealth/Stamina/AttackPower)
                  ↓
   BlueprintImplementableEvent OnHealthChanged / OnStaminaChanged / ...
                  ↓
   WBP_HUD (BP child) overrides events → ProgressBar.SetPercent(...)
```

Character / PlayerState carry **no HUD-facing API**. NPCs reuse the same widget base via `UWidgetComponent` for floating health bars — `MATargetDummy::BeginPlay` calls the same `InitFromASC` on the spawned widget.

---

## Module layout

```
Source/MultiPlayerAction/
├── MultiPlayerActionCharacter.{h,cpp}     Player pawn: input bindings, GAS init via PlayerState, weapon mesh, authored death
├── MultiPlayerActionGameMode.{h,cpp}      Deathmatch loop: MatchState overrides, kill router, verdict, freeze, restart
├── MAGameState.{h,cpp}                    Replicated match data (clock end / kill target / verdict) + kill-event multicast
├── AbilitySystem/
│   ├── MAAbilitySystemComponent.h         Custom ASC subclass (extension hook)
│   ├── MAAbilityInputID.h                 InputID enum for AbilityLocalInputPressed routing (combo input buffer)
│   ├── MAAttributeSet.{h,cpp}             Replicated attrs + Damage meta + PostExecute clamp/stagger event + CheckDeath
│   ├── MACombatantInterface.h             "anything that dies" abstraction, called by AS::CheckDeath
│   ├── MAGameplayTags.{h,cpp}             Native gameplay tags (Ability.* / GameplayCue.* / State.* / Event.*)
│   ├── AnimNotifies/
│   │   └── AN_SendGameplayEvent.{h,cpp}   Montage notify → SendGameplayEventToActor
│   ├── Abilities/
│   │   ├── MAGameplayAbilityBase.{h,cpp}  Base for all GAs (LocalPredicted + InstancedPerActor defaults)
│   │   ├── GA_MeleeAttack.{h,cpp}         3-hit combo: replicated press queue + window-period section jumps + sphere trace
│   │   ├── GA_HitReact.{h,cpp}            ServerInitiated stagger — GameplayEvent-triggered, retriggerable per hit
│   │   ├── GA_Block.{h,cpp}               Hold guard: State.Blocking (70% ExecCalc mitigation) + stance loop/absorb montages
│   │   ├── GA_Sprint.{h,cpp}              Hold-to-activate sprint, periodic Stamina drain, auto-end on Stamina=0
│   │   ├── GA_Dodge.{h,cpp}               Dash + i-frame via State.Dodging ActivationOwnedTags (GE IgnoreTags)
│   │   └── GA_Fireball.{h,cpp}            Predicted cast + server-authoritative projectile spawn (cast-time spec snapshot)
│   ├── Cues/
│   │   └── GCN_ParticleBurst.{h,cpp}      Data-driven burst cue base — BP children are pure config, no graphs
│   └── Executions/
│       └── MADamageExecutionCalculation.{h,cpp}   Lyra-style formula (AP snapshot × armor scale × block mitigation)
├── Combat/
│   └── MAProjectile.{h,cpp}               Server-authoritative replicated projectile: AoE overlap → spec → cue → destroy
├── Player/
│   ├── MAPlayerState.{h,cpp}              Owns ASC + AttributeSet; replicated Kills/Deaths (BlueprintPure accessors)
│   └── MAPlayerController.{h,cpp}         Owns all local UI (HUD/match overlay/scoreboard/kill feed); respawn timer + RPCs
├── Online/
│   └── MASessionSubsystem.{h,cpp}         GameInstance subsystem over OnlineSubsystem sessions (host/find/join,
│                                          in-flight guards, network-failure recovery to the menu)
├── Animation/
│   └── MAAnimInstance.{h,cpp}             Torso aim twist: spine-chain yaw toward camera, gated on State tags
├── UI/
│   ├── MAUserWidget.{h,cpp}               Abstract HUD widget base; auto-wires child Views in its tree
│   ├── MAHealthBarWidget.{h,cpp}          SF-style chip health bar (code-built styles, hide-until-damaged, death drain)
│   ├── MASkillSlotWidget.{h,cpp}          Skill slot: cooldown sweep + active highlight + ability/hotkey label stack
│   ├── MAMatchStatusWidget.{h,cpp}        Code-built match overlay: clock, K x/target + D, respawn countdown, KILLED BY
│   ├── MAScoreboardWidget.{h,cpp}         Code-built hold-Tab standings / pinned post-match results (pooled rows)
│   ├── MAKillFeedWidget.{h,cpp}           Code-built top-right kill feed (GameState kill event, timed line expiry)
│   └── MainMenu/                          MVVM front-end: MAMainMenuViewModel/Widget, MASessionListEntryVM/RowWidget
└── AI/
    ├── MATargetDummy.{h,cpp}              Pawn-owned ASC (Minimal rep), AI possess, overhead chip bar, death lifecycle
    ├── MAEnemyController.{h,cpp}          AAIController + RunBehaviorTree on possess
    ├── BTTask_TryActivateAbilityByTag.{h,cpp}    BT task: ASC.TryActivateAbilitiesByTag(AbilityTag)
    └── BTService_UpdateTargetInfo.{h,cpp}        BT service: nearest living player, reachability-preferred
```

Content (BP / assets) under `Content/`:
- `AbilitySystem/Abilities/` — `BP_GA_MeleeAttack`, `BP_GA_Sprint`, `BP_GA_Dodge`, `BP_GA_Fireball`, `BP_GA_HitReact`, `BP_GA_Block`
- `AbilitySystem/GE/` — `BP_GE_Damage` (uses `MADamageExecutionCalculation`), `BP_GE_Cooldown_Melee`/`_Dodge`/`_Fireball`, `BP_GE_StaminaCost`/`_Fireball`, `BP_GE_StaminaRegen` (Periodic, OngoingTagRequirements suppresses during sprint)
- `AbilitySystem/Cues/` — `BP_GCN_MeleeHit` (`GameplayCueNotify_Static`, P_Sparks at `FHitResult.ImpactPoint`), `BP_GCN_FireballExplosion` (`GCN_ParticleBurst` child — pure data, no graph)
- `AbilitySystem/AM_Montage/` — `AM_SwordComboUB` (3 dead-end combo sections + hit/window notifies), `AM_FireballCastUB`, `AM_HitReactA/B_UB`, `AM_BlockLoopUB` (self-linked looping stance), `AM_BlockHitUB` — all UpperBody-slot, all authored via the MCP `create_anim_montage` tool
- `AnimLibrary/` — KayKit Knight clip library (76 clips) + `Manny/` IK-retargeted set used by the sword combo / blocks / reacts / deaths
- `Characters/Mannequins/Animations/ABP_Manny` — upper-body layered blend rig + spine aim-twist chain (authored node-by-node via MCP AnimGraph tools)
- `Weapons/` — KayKit (CC0) sword static mesh on the character's `hand_r`
- `Blueprints/Combat/` — `BP_Projectile_Fireball` (visuals on top of `AMAProjectile`)
- `Blueprints/AI/` — `BP_TargetDummy`, `BP_EnemyController`
- `AI/` — `BB_Enemy` (Blackboard), `BT_Enemy` (BehaviorTree)
- `Blueprints/UI/` — `WBP_HUD` (chip health bar + 5-slot skill bar), `WBP_SkillSlot`, `WBP_MainMenu` (MVVM); `UI/` — `WBP_HealthBar` (shared player/NPC chip bar), `WBP_MASessionRowWidget`. The match overlay / scoreboard / kill feed have **no widget assets** — they are C++-built (`WidgetTree::ConstructWidget`)

> The bulk of the content above was authored **agent-side via UnrealAgentMCP** (private repo): montage creation/cropping, AnimNotify placement, GE configuration, BehaviorTree nodes, UMG widget trees + MVVM bindings, AnimGraph surgery (cached-pose/slot/layered-blend/ModifyBone chains) and the arena dressing all happened through MCP tools — several of which were built (with save/reload regression tests) precisely because this project needed them. That dogfooding loop is the second half of the portfolio.

---

## Build

Requirements:
- Unreal Engine **5.5** (matches `MultiPlayerAction.uproject` `EngineAssociation`)
- Visual Studio 2022 (Windows) / Xcode 15+ (macOS) with C++ workload
- Optional: Starter Content (used by `BP_GCN_MeleeHit`/`BP_GCN_FireballExplosion` for `P_Sparks`/`P_Explosion`/`P_Fire` particle templates)
- Optional: Mage Animation Bundle samples (source `AnimSequence` + skeleton for `AM_FireballCast`; the montage itself is committed, and `SK_Mannequin` carries a compatible-skeleton entry pointing at the bundle's skeleton — it dangles harmlessly as a soft reference if the bundle isn't installed, only the fireball cast animation degrades)

Steps:
```
git clone https://github.com/alertform/ue5-multiplayer-action.git
cd ue5-multiplayer-action
# Right-click MultiPlayerAction.uproject → "Generate Visual Studio project files"
# Open MultiPlayerAction.sln, set MultiPlayerActionEditor as startup, Build.
```

Or via UBT externally:
```
"<UE_5.5>/Engine/Build/BatchFiles/Build.bat" \
  MultiPlayerActionEditor Win64 Development \
  -Project="<absolute-path>/MultiPlayerAction.uproject" -WaitMutex
```

PIE: open `Content/Maps/ThirdPersonMap`, set Number of Players ≥ 2 to exercise multiplayer replication (or start from `Maps/MainMenu` and Host/Join through the session front-end). **LMB** 3-hit sword combo (mash to chain, press in the window to cancel recovery), **RMB hold** block (70% mitigation, guard stance), **Q** fireball (mobile upper-body cast, projectile flies at the camera-ray aim point), **Shift** sprint, **Ctrl** dodge i-frame, **Tab** scoreboard — all five abilities on the HUD skill bar. The match is a **5-minute / first-to-10 deathmatch**: kills feed the top-right ticker and the K/D line under the clock, dying shows the killer + a respawn countdown, and the match ends into a pinned results screen that auto-restarts the map 10 s later. Walk near the `BP_TargetDummy` placed in the map — it chases (NavMesh, reachability-aware targeting) and attacks via BT with the same melee GA; staggers interrupt your combo, blocking absorbs them, and dummy kills count toward the match (solo-friendly loop). Console `DamageSelf 100` self-damages (Server RPC) to test the authored death → ragdoll → respawn chain. Acceptance was also run at 100ms emulated latency (PIE Network Emulation).

---

## Notes for reviewers

- Code leans toward **C++ first, Blueprint composition only** — engine-level work, not BP scripting.
- Architecture decisions follow Lyra conventions where applicable (HUD widget binding, NPC ASC ownership, `Damage` meta-attribute routing, `BehaviorTree` + GAS integration, replication modes).
- Multiplayer correctness validated in PIE listen-server with 2 players: server-authoritative damage, attribute replication, ragdoll Multicast, owning-client HUD binding.
- Dev gotchas captured in commit messages — UE 5.5 deprecations (`SetAssetTags`, `SetNetUpdateFrequency`, GE Components system), the `RestartPlayer` teleport-existing trap, `NetMulticast` for component state, `bWarningsAsErrors` shadow-name pitfalls.

---

## License

No license declared yet — reach out before reusing code.
