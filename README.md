# ue5-multiplayer-action

Third-person multiplayer action game built in Unreal Engine 5 — C++ · Gameplay Ability System · network replication. Portfolio project.

## Stack

| Layer | Tech |
|-------|------|
| Engine | Unreal Engine 5.5 |
| Language | C++ (gameplay code) · Blueprint (composition only) |
| Abilities | Gameplay Ability System (GAS) — `GameplayAbilities` · `GameplayTags` · `GameplayTasks` |
| Input | Enhanced Input subsystem |
| Networking | Replicated Character + PlayerState · `NetCore` · `OnlineSubsystem` |

## Module Layout

```
Source/MultiPlayerAction/
├── MultiPlayerActionCharacter.{h,cpp}    Player pawn: input, movement, replication
├── MultiPlayerActionGameMode.{h,cpp}     Default game mode
├── AbilitySystem/
│   ├── MAAbilitySystemComponent.h        Custom ASC entry point
│   └── MAAttributeSet.{h,cpp}            Replicated attributes — Health / MaxHealth / Stamina
└── Player/
    └── MAPlayerState.{h,cpp}             Owns the ASC + AttributeSet (standard GAS pattern)
```

GAS ownership follows the canonical **PlayerState owns the ASC** pattern so attributes survive pawn re-possession on respawn.

## Build

Requirements:
- Unreal Engine **5.5** (matches `MultiPlayerAction.uproject` `EngineAssociation`)
- Visual Studio 2022 (Windows) / Xcode 15+ (macOS) with C++ workload
- Git LFS (for any binary content added later)

Steps:
```
git clone https://github.com/alertform/ue5-multiplayer-action.git
cd ue5-multiplayer-action
# Right-click MultiPlayerAction.uproject → "Generate Visual Studio project files"
# (or via UnrealBuildTool on macOS)
# Open MultiPlayerAction.sln, set MultiPlayerActionEditor as startup, Build.
```

## Status

This is an in-progress portfolio project. Current scope:

- [x] Project scaffold on UE 5.5 with GAS plugin enabled
- [x] Third-person `Character` with SpringArm + FollowCamera, tuned movement
- [x] Enhanced Input wired up — Move / Look / Jump actions
- [x] `IAbilitySystemInterface` on the Character
- [x] AttributeSet (Health / MaxHealth / Stamina) with `OnRep_*` callbacks
- [x] PlayerState-owned ASC — correct GAS replication pattern, init on both server (`PossessedBy`) and client (`OnRep_PlayerState`)
- [ ] Concrete `GameplayAbility` implementations
- [ ] Combat loop + replicated damage flow
- [ ] Multiplayer session / lobby setup via `OnlineSubsystem`
- [ ] Animation + montage replication

Code intentionally leans toward C++ over Blueprint to demonstrate engine-level work.

## License

No license declared yet — reach out before reusing code.
