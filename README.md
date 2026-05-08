# ue5-multiplayer-action

Lyra-style PvE melee combat demo built on Unreal Engine 5.5 — full **Gameplay Ability System** (GAS) stack, multiplayer replication, AI driven by **BehaviorTree** sharing the same `GameplayAbility` C++ classes as the player. Personal portfolio project.

---

## Highlights

| Area | What's in |
|---|---|
| **GAS 5 pillars** | `GameplayAbility` (LocalPredicted) · `GameplayEffect` (Damage Execution / Cooldown / Stamina Cost / Periodic Stamina Regen) · `AttributeSet` (Health/MaxHealth/Stamina/AttackPower/Armor + Damage meta) · `GameplayCue` (`Static` notify w/ `FHitResult` location/normal) · `PredictionKey` |
| **Damage formula** | `UGameplayEffectExecutionCalculation` capturing source `AttackPower` (snapshot) + target `Armor` (live), output to `Damage` meta-attribute, AS routes to `Health`. Lyra `ULyraDamageExecution` pattern. |
| **HUD architecture** | `UMAUserWidget` abstract C++ base self-binds to `ASC->GetGameplayAttributeValueChangeDelegate`. BP children handle visuals only. Same widget base reused for player HUD + enemy floating health bar. |
| **AI** | `BehaviorTree` + custom `UBTService_UpdateTargetInfo` (per-tick player tracking) + custom `UBTTask_TryActivateAbilityByTag` (BT node → `ASC.TryActivateAbilitiesByTag`). AI calls the **same `UGA_MeleeAttack` C++ class** the player drives via Enhanced Input. |
| **Networking** | Server-authoritative damage; `Mixed` ASC replication for players (cooldown to owner) + `Minimal` for NPCs. `NetMulticast` RPC for ragdoll (component state doesn't auto-replicate). `Server` RPC for dev cheats. |
| **Death/Respawn** | `IMACombatantInterface` abstraction; `State.Dead` loose tag gates re-activation; `UnPossess()` before `GameMode->RestartPlayer()` to force fresh pawn spawn (vs the engine's default teleport-existing). |

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
Enhanced Input → ASC.TryActivateAbilitiesByTag(Ability.Melee.Attack)
                                ↓
                    UGA_MeleeAttack::ActivateAbility (LocalPredicted)
                                ↓
                    CommitAbility → BP_GE_StaminaCost (-10 Stamina) + BP_GE_Cooldown_Melee (1s)
                                ↓
                    PlayMontageAndWait + WaitGameplayEvent (AnimNotify Event.Montage.Hit)
                                ↓
                    SERVER: PerformHitTrace → SphereSweep ECC_Pawn
                                ↓
                    Apply BP_GE_Damage (uses MADamageExecutionCalculation)
                                ↓
                    Source.AttackPower (snapshot) × (1 - Target.Armor × 0.05)
                                ↓
                    Output Damage meta-attribute on target ASC
                                ↓
                    Target AS::PostGameplayEffectExecute routes Damage → Health
                                ↓
                    On Health<=0: AS::CheckDeath → IMACombatantInterface::Execute_HandleDeath
                                ↓
                    NetMulticast_PlayDeath (ragdoll) + ScheduleRespawn(3s)
                                ↓
                    Respawn: UnPossess + GameMode.RestartPlayer → fresh pawn at PlayerStart
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
├── MultiPlayerActionCharacter.{h,cpp}     Player pawn: input bindings, GAS init via PlayerState, IMACombatantInterface
├── MultiPlayerActionGameMode.{h,cpp}      Wires PlayerStateClass + PlayerControllerClass
├── AbilitySystem/
│   ├── MAAbilitySystemComponent.h         Custom ASC subclass (extension hook)
│   ├── MAAttributeSet.{h,cpp}             Replicated attrs + Damage meta + PostExecute clamping + CheckDeath helper
│   ├── MACombatantInterface.h             "anything that dies" abstraction, called by AS::CheckDeath
│   ├── MAGameplayTags.{h,cpp}             Native gameplay tags (Ability.* / GameplayCue.* / State.* / Event.*)
│   ├── AnimNotifies/
│   │   └── AN_SendGameplayEvent.{h,cpp}   Montage notify → SendGameplayEventToActor
│   ├── Abilities/
│   │   ├── MAGameplayAbilityBase.{h,cpp}  Base for all GAs (LocalPredicted + InstancedPerActor defaults)
│   │   ├── GA_MeleeAttack.{h,cpp}         Reference ability — montage + sphere trace + damage GE + GameplayCue
│   │   └── GA_Sprint.{h,cpp}              Hold-to-activate sprint, periodic Stamina drain, auto-end on Stamina=0
│   └── Executions/
│       └── MADamageExecutionCalculation.{h,cpp}   Lyra-style damage formula (source AP snapshot × (1 - target Armor scale))
├── Player/
│   ├── MAPlayerState.{h,cpp}              Owns ASC + AttributeSet
│   └── MAPlayerController.{h,cpp}         Spawns + binds HUD widget; ScheduleRespawn timer; DamageSelf Server RPC
├── UI/
│   └── MAUserWidget.{h,cpp}               Abstract HUD widget base, self-binds to ASC delegates
└── AI/
    ├── MATargetDummy.{h,cpp}              Pawn-owned ASC (Minimal rep), AI possess, floating health bar component
    ├── MAEnemyController.{h,cpp}          AAIController + RunBehaviorTree on possess
    ├── BTTask_TryActivateAbilityByTag.{h,cpp}    BT task: ASC.TryActivateAbilitiesByTag(AbilityTag)
    └── BTService_UpdateTargetInfo.{h,cpp}        BT service: per-tick player + range update to BB
```

Content (BP / assets) under `Content/`:
- `AbilitySystem/Abilities/` — `BP_GA_MeleeAttack`, `BP_GA_Sprint`
- `AbilitySystem/GE/` — `BP_GE_Damage` (uses `MADamageExecutionCalculation`), `BP_GE_Cooldown_Melee`, `BP_GE_StaminaCost`, `BP_GE_StaminaRegen` (Periodic, OngoingTagRequirements suppresses during sprint)
- `AbilitySystem/Cues/` — `BP_GCN_MeleeHit` (`GameplayCueNotify_Static`, P_Sparks at `FHitResult.ImpactPoint`)
- `Blueprints/AI/` — `BP_TargetDummy`, `BP_EnemyController`
- `AI/` — `BB_Enemy` (Blackboard), `BT_Enemy` (BehaviorTree)
- `UI/` — `WBP_HUD` (player), `WBP_EnemyHealthBar` (NPC)

---

## Build

Requirements:
- Unreal Engine **5.5** (matches `MultiPlayerAction.uproject` `EngineAssociation`)
- Visual Studio 2022 (Windows) / Xcode 15+ (macOS) with C++ workload
- Optional: Starter Content (used by `BP_GCN_MeleeHit` for `P_Sparks` particle template)

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

PIE: open `Content/ThirdPerson/Maps/ThirdPersonMap`, set Number of Players ≥ 2 to exercise multiplayer replication. Walk near the `BP_TargetDummy` placed in the map — it will rotate + attack via BT. Console `DamageSelf 100` self-damages (Server RPC) to test ragdoll + respawn.

---

## Status

Stage 1-2 (4/19 - 5/5): minimal melee combat loop end-to-end. ✅
Stage 2.5 (5/7): Lyra-style HUD architecture, NPC ASC pattern, Death/Respawn, GameplayCue, Stamina regen. ✅
Stage 3 (5/8): ExecutionCalculation damage formula, AI BehaviorTree, Sprint with multi-ability cancel. ✅

| Component | Status |
|---|---|
| Project scaffold on UE 5.5 with GAS plugin | ✅ |
| Third-person Character + Enhanced Input | ✅ |
| AttributeSet (Health/MaxHealth/Stamina/AttackPower/Armor + Damage meta) | ✅ |
| PlayerState-owned ASC, init on both server + client | ✅ |
| Native gameplay tags (`UE_DEFINE_GAMEPLAY_TAG_COMMENT`) | ✅ |
| Melee `GameplayAbility` with montage + sphere trace + damage GE | ✅ |
| `GameplayEffectExecutionCalculation` damage formula | ✅ |
| `Damage` meta-attribute routing in `PostGameplayEffectExecute` | ✅ |
| `GameplayCue` hit FX with `FHitResult` location/normal | ✅ |
| Periodic Stamina regen GE with `OngoingTagRequirements` suppression | ✅ |
| Sprint ability (hold-to-activate, drain, auto-end, multi-ability cancel) | ✅ |
| Lyra-style HUD via abstract `UMAUserWidget` base self-binding to ASC | ✅ |
| NPC ASC (Pawn-owned, Minimal replication) + floating health bar | ✅ |
| Death + Respawn loop with `NetMulticast` ragdoll + `UnPossess` fix | ✅ |
| `BehaviorTree` AI sharing player's `UGA_MeleeAttack` via `TryActivateAbilityByTag` | ✅ |
| `AIController` + custom `BTService` + custom `BTTask` | ✅ |
| Multiplayer PIE listen-server end-to-end verified | ✅ |
| Online session / lobby (`OnlineSubsystem`) | ⏳ future |
| Damage numbers, hit-react montages, dodge i-frames | ⏳ future |

---

## Notes for reviewers

- Code leans toward **C++ first, Blueprint composition only** — engine-level work, not BP scripting.
- Architecture decisions follow Lyra conventions where applicable (HUD widget binding, NPC ASC ownership, `Damage` meta-attribute routing, `BehaviorTree` + GAS integration, replication modes).
- Multiplayer correctness validated in PIE listen-server with 2 players: server-authoritative damage, attribute replication, ragdoll Multicast, owning-client HUD binding.
- Dev gotchas captured in commit messages — UE 5.5 deprecations (`SetAssetTags`, `SetNetUpdateFrequency`, GE Components system), the `RestartPlayer` teleport-existing trap, `NetMulticast` for component state, `bWarningsAsErrors` shadow-name pitfalls.

`CLAUDE.md` in the repo root has more in-depth architecture notes.

---

## License

No license declared yet — reach out before reusing code.
