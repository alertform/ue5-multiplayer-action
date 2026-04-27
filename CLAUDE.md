# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project

Unreal Engine **5.5** C++ project (`MultiPlayerAction.uproject`) — a third-person multiplayer action template built on **GAS** (Gameplay Ability System) with **Enhanced Input** and standard UE replication.

The single C++ module is `MultiPlayerAction` (Runtime). Editor target: `MultiPlayerActionEditor`. Game target: `MultiPlayerAction`.

## Build & Run

UE5 projects build via UnrealBuildTool, not directly via MSBuild on the .sln. The .sln is regenerated and is gitignored — never edit it by hand.

```bash
# Regenerate VS/Rider project files after adding/removing .cpp/.h or editing Build.cs
"C:/Program Files/Epic Games/UE_5.5/Engine/Binaries/DotNET/UnrealBuildTool/UnrealBuildTool.exe" \
  -projectfiles -project="F:/MultiPlayerAction/MultiPlayerAction.uproject" -game -rocket -progress

# Build editor (Development Editor, Win64) — required after C++ changes before launching the editor
"C:/Program Files/Epic Games/UE_5.5/Engine/Build/BatchFiles/Build.bat" \
  MultiPlayerActionEditor Win64 Development \
  -Project="F:/MultiPlayerAction/MultiPlayerAction.uproject" -WaitMutex -FromMsBuild

# Launch editor with this project
"C:/Program Files/Epic Games/UE_5.5/Engine/Binaries/Win64/UnrealEditor.exe" \
  "F:/MultiPlayerAction/MultiPlayerAction.uproject"

# Cook + package shipping build
"C:/Program Files/Epic Games/UE_5.5/Engine/Build/BatchFiles/RunUAT.bat" BuildCookRun \
  -project="F:/MultiPlayerAction/MultiPlayerAction.uproject" \
  -platform=Win64 -clientconfig=Shipping -cook -build -stage -package -archive \
  -archivedirectory="F:/MultiPlayerAction/Builds"
```

When the editor build fails to find headers in subdirectories, check `Source/MultiPlayerAction/MultiPlayerAction.Build.cs` — `PublicIncludePaths.Add(ModuleDirectory)` is what makes paths like `AbilitySystem/MAAttributeSet.h` resolve. (See commit `6c59262`.)

## Architecture: GAS ownership lives on PlayerState, not Character

This is the single most important non-obvious thing about this codebase. Read this before touching any GAS-related code.

```
AMAPlayerState (owns ASC + AttributeSet)
    ├─ UMAAbilitySystemComponent   ← replicated, Mixed replication mode
    └─ UMAAttributeSet              ← Health, MaxHealth, Stamina, AttackPower

AMultiPlayerActionCharacter (cached pointers only, no ownership)
    ├─ PossessedBy()        ← server: pulls ASC from PS, calls InitAbilityActorInfo, GiveDefaultAbilities
    └─ OnRep_PlayerState()  ← client: same init via PlayerState replication
```

**Why PlayerState:** abilities, cooldowns, and attribute state must survive pawn death/respawn. The Character is just the AvatarActor; the PlayerState is the OwnerActor.

**Implications when modifying code:**
- Adding new attributes → edit `UMAAttributeSet` (`MAAttributeSet.h/cpp`). Must add `UPROPERTY(ReplicatedUsing=...)`, an `OnRep_*` UFUNCTION, accessor macro, init in ctor, and `DOREPLIFETIME_CONDITION_NOTIFY` in `GetLifetimeReplicatedProps`.
- Granting new default abilities → add `TSubclassOf<UGameplayAbility>` entries to `AMultiPlayerActionCharacter::DefaultAbilities` (in BP defaults). Granting happens server-side only in `GiveDefaultAbilities()`.
- Authoritative gameplay logic (damage, hit detection, GE application) → guard with `HasAuthority()`. See `UGA_MeleeAttack::PerformHitTrace` for the pattern.

## Architecture: ability activation flow

Input is wired through Enhanced Input → tag-based ability activation, not direct C++ binding to ability classes.

1. `AMultiPlayerActionCharacter::SetupPlayerInputComponent` binds `AttackAction` to `OnAttackInput`.
2. `OnAttackInput` calls `AbilitySystemComponent->TryActivateAbilitiesByTag` with tag `Ability.MeleeAttack`.
3. The ASC finds any granted ability whose `AbilityTags` contain that tag and activates it (currently only `UGA_MeleeAttack`).
4. `UGA_MeleeAttack::ActivateAbility` plays an `AnimMontage` via `UAbilityTask_PlayMontageAndWait` and waits for an `Event.Attack` gameplay event (sent from an `AnimNotify` in the montage) via `UAbilityTask_WaitGameplayEvent`.
5. On the event, `PerformHitTrace` does a server-side sphere sweep and applies the configured `DamageEffect` GE to each hit ASC.

**Adding a new ability:** subclass `UMAGameplayAbilityBase` (not `UGameplayAbility` directly — the base sets `InstancedPerActor` + `LocalPredicted` defaults and provides `GetMACharacter`). Set `AbilityTags` in the BP defaults so `TryActivateAbilitiesByTag` can find it. Bind a new input action and call `TryActivateAbilitiesByTag` with the matching tag.

**Networking model:** `LocalPredicted` execution policy on abilities — client predicts the activation, server confirms. Hit detection and GE application are server-authoritative (see the `HasAuthority()` early-out).

## Source layout

```
Source/MultiPlayerAction/
├── MultiPlayerAction.Build.cs        # Module deps: GameplayAbilities, GameplayTags, GameplayTasks, EnhancedInput, NetCore
├── MultiPlayerActionCharacter.{h,cpp} # ACharacter + IAbilitySystemInterface, input bindings
├── MultiPlayerActionGameMode.{h,cpp}  # Sets PlayerStateClass = AMAPlayerState
├── Player/MAPlayerState.{h,cpp}       # Owns ASC + AttributeSet
└── AbilitySystem/
    ├── MAAbilitySystemComponent.h     # Subclass hook (currently empty)
    ├── MAAttributeSet.{h,cpp}         # Replicated attributes
    └── Abilities/
        ├── MAGameplayAbilityBase.{h,cpp}  # Base for all GAs
        └── GA_MeleeAttack.{h,cpp}         # Reference ability impl
```

`Content/` holds the BP_ThirdPersonCharacter Blueprint that derives from `AMultiPlayerActionCharacter` (set as `DefaultPawnClass` in the GameMode via `ConstructorHelpers::FClassFinder`). Do not add direct content references in C++ — keep them in the Blueprint defaults.

## Required Gameplay Tags

These tags must exist in the project's tag config (Project Settings → GameplayTags) for the system to function. Missing tags cause silent activation failures:

- `Ability.MeleeAttack` — used by `OnAttackInput` to find the melee ability
- `Event.Attack` — sent from the AttackMontage's AnimNotify to trigger hit detection

## Testing

No automated test target exists yet. UE Automation tests would live under `Source/MultiPlayerAction/Tests/` with a separate `.Build.cs` if added. Until then, verify changes by:
1. Building the editor target
2. Launching with `?listen` for host or play-in-editor with `Number of Players` ≥ 2 to exercise replication
3. Watching `Output Log` for GAS warnings (missing tags, ungranted abilities, ASC init failures)
