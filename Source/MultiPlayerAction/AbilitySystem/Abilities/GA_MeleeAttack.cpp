#include "AbilitySystem/Abilities/GA_MeleeAttack.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "Abilities/Tasks/AbilityTask_WaitInputPress.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystem/MAGameplayTags.h"
#include "Engine/World.h"
#include "MultiPlayerActionCharacter.h"
#include "TimerManager.h"
#include "Combat/MAAttackLunge.h"
#include "Combat/MAMeleeHitOps.h"
#include "Combat/MAWarpOps.h"

const FName UGA_MeleeAttack::WarpTargetName(TEXT("AttackTarget"));

UGA_MeleeAttack::UGA_MeleeAttack()
{
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;

	// Lua 配置表键名（Content/Lua/AbilityConfig.lua），缺键回退本类 UPROPERTY 默认
	ConfigKey = TEXT("MeleeAttack");

	// Identify this ability by tag so TryActivateAbilitiesByTag can find it; BP children inherit this.
	// UE 5.5+ deprecates direct AbilityTags mutation — use SetAssetTags in constructor only.
	FGameplayTagContainer Tags;
	Tags.AddTag(MAGameplayTags::Ability_Melee_Attack);
	SetAssetTags(Tags);

	// Cannot attack while dead — checked at TryActivate time, no need for runtime guard
	ActivationBlockedTags.AddTag(MAGameplayTags::State_Dead);
	// No attacking out of a rooted cast — the melee montage would interrupt the fireball montage,
	// wasting the already-committed fireball cost + cooldown before its projectile spawns.
	ActivationBlockedTags.AddTag(MAGameplayTags::State_Casting);
	// Release the guard to swing — attacking from inside a block would keep the mitigation
	// tag live through the swing.
	ActivationBlockedTags.AddTag(MAGameplayTags::State_Blocking);
	// Owned for the swing's duration: gates the AnimInstance upper-body aim twist
	// (spine chain toward camera yaw) — see UMAAnimInstance::NativeUpdateAnimation.
	ActivationOwnedTags.AddTag(MAGameplayTags::State_Attacking);

	// Default 3-hit chain — section names the combo montage must define. BP children may
	// trim/extend; an empty array (or a montage without these sections) = single swing.
	ComboSections = { FName("Combo1"), FName("Combo2"), FName("Combo3") };
}

void UGA_MeleeAttack::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	if (!AttackMontage)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	// Fresh chain: instance-per-actor means these members persist between activations.
	ComboIndex = 0;
	BufferedComboPresses = 0;
	bComboWindowOpen = false;

	// No actor rotation here: the upper body visually turns toward the camera via the
	// AnimInstance spine twist (gated on our owned State.Attacking), the legs keep
	// following orient-to-movement, and PerformHitTrace aims with the camera yaw directly.

	// First swing's lunge target — re-resolved per chained swing in TryAdvanceCombo. Must run
	// before the montage so the section's AttackTarget warp window has its target on frame one.
	SetupWarpTarget();

	// Play montage at configurable rate (default 2.0x — Lua 配置可覆盖，缺键回退 UPROPERTY)
	UAbilityTask_PlayMontageAndWait* MontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(
		this, NAME_None, AttackMontage, ReadConfigFloat(TEXT("MontagePlayRate"), MontagePlayRate));

	// OnBlendOut + OnCompleted both fire on natural end — bind only OnCompleted to avoid double EndAbility
	MontageTask->OnCompleted.AddDynamic(this, &UGA_MeleeAttack::OnMontageEnded);
	MontageTask->OnInterrupted.AddDynamic(this, &UGA_MeleeAttack::OnMontageEnded);
	MontageTask->OnCancelled.AddDynamic(this, &UGA_MeleeAttack::OnMontageEnded);
	MontageTask->ReadyForActivation();

	// Wait for Event.Montage.Hit gameplay event (sent from AnimNotify in montage).
	// OnlyTriggerOnce=false (default): one task serves every section's hit notify.
	UAbilityTask_WaitGameplayEvent* EventTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(
		this, MAGameplayTags::Event_Montage_Hit);

	EventTask->EventReceived.AddDynamic(this, &UGA_MeleeAttack::OnMontageEvent);
	EventTask->ReadyForActivation();

	// Combo decision point — fired by AnimNotify near each section's end. Deterministic montage
	// frame on client and server (replicated montage position), so both sides make the same
	// chain-vs-finish call; no wall-clock timer race.
	UAbilityTask_WaitGameplayEvent* ComboWindowTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(
		this, MAGameplayTags::Event_Montage_ComboWindow);

	ComboWindowTask->EventReceived.AddDynamic(this, &UGA_MeleeAttack::OnComboWindow);
	ComboWindowTask->ReadyForActivation();

	ArmComboInputTask();
}

void UGA_MeleeAttack::ArmComboInputTask()
{
	// One-shot task. On the owning client it fires from the local press; the same press reaches
	// the server instance via the GAS generic replicated InputPressed event
	// (AbilityLocalInputPressed -> InvokeReplicatedEvent -> ServerSetReplicatedEvent), so the
	// buffer fills on BOTH ends without any custom RPC.
	UAbilityTask_WaitInputPress* InputTask = UAbilityTask_WaitInputPress::WaitInputPress(this);
	InputTask->OnPress.AddDynamic(this, &UGA_MeleeAttack::OnComboInputPressed);
	InputTask->ReadyForActivation();
}

void UGA_MeleeAttack::OnComboInputPressed(float TimeWaited)
{
	// QUEUE the press. A boolean here eats fast triple-mashes (the 3rd press lands before the
	// 1st window and is consumed with it); counting presses makes N mashes yield N swings.
	BufferedComboPresses = FMath::Min(BufferedComboPresses + 1, ComboSections.Num());

	// Reactive play: once the window is open, a press chains INSTANTLY (cancels the swing's
	// recovery) instead of dying in the queue after the notify already passed.
	if (bComboWindowOpen)
	{
		TryAdvanceCombo();
	}

	// Re-arm DEFERRED to the next tick — never synchronously from inside this callback.
	// On a server hosting a remote client, WaitInputPress::Activate immediately fires for a
	// cached replicated InputPressed event; a synchronous re-arm here re-enters that path
	// and recurses until stack overflow (found by dedicated-server PIE testing).
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimerForNextTick(
			FTimerDelegate::CreateWeakLambda(this, [this]()
			{
				if (IsActive())
				{
					ArmComboInputTask();
				}
			}));
	}
}

void UGA_MeleeAttack::OnComboWindow(FGameplayEventData EventData)
{
	// The notify OPENS the window (placed just after the hit frame). Queued presses chain
	// right here; later presses chain instantly in OnComboInputPressed while open.
	bComboWindowOpen = true;
	TryAdvanceCombo();
}

void UGA_MeleeAttack::TryAdvanceCombo()
{
	// NOTE: the jump must happen BEFORE the section's dead-end blend-out begins, and the
	// anticipation scales with play rate: the stop triggers BlendOut.BlendTime (REAL seconds)
	// before the boundary, i.e. BlendTime * MontagePlayRate in montage-time. Once blending,
	// the ASC has already cleared LocalAnimMontageInfo (OnMontageBlendingOut) and
	// MontageJumpToSection is a silent no-op — hence the tight 0.1s BlendOut on the montage.
	if (BufferedComboPresses <= 0 || ComboIndex + 1 >= ComboSections.Num())
	{
		// No chain (yet): the window stays open for a later press; without one the section
		// runs out — montage end -> OnMontageEnded -> EndAbility.
		return;
	}

	// Defensive: if the blend-out already started, the jump would no-op — bail instead of
	// advancing ComboIndex into a phantom swing.
	UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();
	if (!ASC || !ASC->IsAnimatingAbility(this))
	{
		return;
	}

	++ComboIndex;
	--BufferedComboPresses; // consume one queued press per chained swing
	bComboWindowOpen = false; // the next section's own notify re-opens it

	// Re-aim this swing's lunge at the current foe before the new section's warp window opens.
	// Runs on client (prediction) and server alike — both sides resolve from their own world
	// state, yielding the same small, prediction-measurable corrections as the dash slash.
	SetupWarpTarget();

	// Routes through the ASC's montage control: section change replicates via
	// FGameplayAbilityRepAnimMontage — no custom replication.
	MontageJumpToSection(ComboSections[ComboIndex]);
}

void UGA_MeleeAttack::SetupWarpTarget()
{
	FMALungeParams Params;
	Params.ConeHalfAngleDeg = ReadConfigFloat(TEXT("ConeHalfAngleDeg"), ConeHalfAngleDeg);
	Params.MaxLungeDistanceCm = ReadConfigFloat(TEXT("MaxLungeDistanceCm"), MaxLungeDistanceCm);
	Params.StopDistanceCm = ReadConfigFloat(TEXT("StopDistanceCm"), StopDistanceCm);
	MAWarpOps::SetupWarpTargetForAbility(this, WarpTargetName, Params,
		ReadConfigFloat(TEXT("NoTargetDashCm"), NoTargetDashCm));
}

void UGA_MeleeAttack::OnMontageEvent(FGameplayEventData EventData)
{
	PerformHitTrace(GetCurrentActorInfo());
}

void UGA_MeleeAttack::OnMontageEnded()
{
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void UGA_MeleeAttack::EndAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility, bool bWasCancelled)
{
	// Drop the warp target so a stale lunge point can't bleed into the next, unwarped activation.
	MAWarpOps::ClearWarpTarget(this, WarpTargetName);
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void UGA_MeleeAttack::PerformHitTrace(const FGameplayAbilityActorInfo* ActorInfo)
{
	AActor* Avatar = ActorInfo ? ActorInfo->AvatarActor.Get() : nullptr;
	// Authoritative hit detection only (server / standalone).
	if (!Avatar || !Avatar->HasAuthority() || !DamageEffect)
	{
		return;
	}
	UAbilitySystemComponent* SourceASC = GetAbilitySystemComponentFromActorInfo();
	if (!SourceASC)
	{
		return;
	}
	// Sweep/filter/apply shared with GA_DashSlash — see MAMeleeHitOps.
	const FGameplayEffectSpecHandle SpecHandle = MakeOutgoingGameplayEffectSpec(DamageEffect, GetAbilityLevel());
	// lua 可调伤害系数，玩家/AI 分键（缺键=1.0）；ExecCalc 乘进最终公式。
	SpecHandle.Data->SetSetByCallerMagnitude(MAGameplayTags::Data_DamageMultiplier,
		ReadDamageMultiplier());
	MAMeleeHitOps::SweepAndApplyMeleeHit(Avatar, SourceASC, SpecHandle,
		ReadConfigFloat(TEXT("TraceDistance"), TraceDistance),
		ReadConfigFloat(TEXT("TraceRadius"), TraceRadius));
}
