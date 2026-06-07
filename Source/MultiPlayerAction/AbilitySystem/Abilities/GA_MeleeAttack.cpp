#include "AbilitySystem/Abilities/GA_MeleeAttack.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "Abilities/Tasks/AbilityTask_WaitInputPress.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystem/MAGameplayTags.h"
#include "Engine/World.h"
#include "MultiPlayerActionCharacter.h"
#include "TimerManager.h"

UGA_MeleeAttack::UGA_MeleeAttack()
{
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;

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
	const AActor* DiagAvatar = ActorInfo ? ActorInfo->AvatarActor.Get() : nullptr;
	UE_LOG(LogTemp, Warning, TEXT("[MBDIAG] melee ACTIVATE role=%d"),
		DiagAvatar ? (int32)DiagAvatar->GetLocalRole() : -1);

	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		UE_LOG(LogTemp, Warning, TEXT("[MBDIAG] melee COMMIT FAILED"));
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

	// Play montage at configurable rate (default 2.0x — see MontagePlayRate UPROPERTY)
	UAbilityTask_PlayMontageAndWait* MontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(
		this, NAME_None, AttackMontage, MontagePlayRate);

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

	// Routes through the ASC's montage control: section change replicates via
	// FGameplayAbilityRepAnimMontage — no custom replication.
	MontageJumpToSection(ComboSections[ComboIndex]);
}

void UGA_MeleeAttack::OnMontageEvent(FGameplayEventData EventData)
{
	PerformHitTrace(GetCurrentActorInfo());
}

void UGA_MeleeAttack::OnMontageEnded()
{
	UE_LOG(LogTemp, Warning, TEXT("[MBDIAG] melee OnMontageEnded"));
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void UGA_MeleeAttack::EndAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility, bool bWasCancelled)
{
	const AActor* DiagAvatar = ActorInfo ? ActorInfo->AvatarActor.Get() : nullptr;
	UE_LOG(LogTemp, Warning, TEXT("[MBDIAG] melee END role=%d cancelled=%d active=%d"),
		DiagAvatar ? (int32)DiagAvatar->GetLocalRole() : -1, bWasCancelled ? 1 : 0, IsActive() ? 1 : 0);

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void UGA_MeleeAttack::PerformHitTrace(const FGameplayAbilityActorInfo* ActorInfo)
{
	// Use AvatarActor (any AActor) instead of casting to AMultiPlayerActionCharacter,
	// so AI-controlled pawns (TargetDummy) can use the same GA class as players.
	AActor* Avatar = ActorInfo ? ActorInfo->AvatarActor.Get() : nullptr;
	if (!Avatar)
	{
		return;
	}

	// Only do authoritative hit detection on server (or standalone)
	if (!Avatar->HasAuthority())
	{
		return;
	}

	// Trace along the camera yaw, not the body facing — the actor keeps orient-to-movement
	// during the (mobile) swing, so the body may point elsewhere while the player aims.
	// AI pawns: GetBaseAimRotation falls back to the actor rotation — same as before.
	const APawn* AvatarPawn = Cast<APawn>(Avatar);
	const FRotator AimYaw(0.f,
		AvatarPawn ? AvatarPawn->GetBaseAimRotation().Yaw : Avatar->GetActorRotation().Yaw, 0.f);
	const FVector AimDir = AimYaw.Vector();

	const FVector Start = Avatar->GetActorLocation() + AimDir * 50.f;
	const FVector End = Start + AimDir * TraceDistance;

	FCollisionQueryParams QueryParams;
	QueryParams.AddIgnoredActor(Avatar);

	TArray<FHitResult> HitResults;
	const bool bHit = Avatar->GetWorld()->SweepMultiByChannel(
		HitResults, Start, End, FQuat::Identity,
		ECC_Pawn, FCollisionShape::MakeSphere(TraceRadius), QueryParams);

	if (!bHit || !DamageEffect)
	{
		return;
	}

	for (const FHitResult& Hit : HitResults)
	{
		AActor* HitActor = Hit.GetActor();
		if (!HitActor || HitActor == Avatar)
		{
			continue;
		}

		// Apply damage GE through source ASC so prediction key + instigator/context route correctly
		UAbilitySystemComponent* TargetASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(HitActor);
		UAbilitySystemComponent* SourceASC = GetAbilitySystemComponentFromActorInfo();
		if (!TargetASC || !SourceASC)
		{
			continue;
		}

		FGameplayEffectSpecHandle SpecHandle = MakeOutgoingGameplayEffectSpec(DamageEffect, GetAbilityLevel());
		if (SpecHandle.IsValid())
		{
			SourceASC->ApplyGameplayEffectSpecToTarget(*SpecHandle.Data.Get(), TargetASC);
		}

		// Fire hit-impact cue with hit location + normal so FX spawn at exact contact point.
		// Static cue notify (BP_GCN_MeleeHit) routes by tag and replicates to all relevant clients.
		FGameplayCueParameters CueParams;
		CueParams.Location = Hit.ImpactPoint;
		CueParams.Normal = Hit.ImpactNormal;
		CueParams.PhysicalMaterial = Hit.PhysMaterial;
		CueParams.Instigator = Avatar;
		CueParams.EffectCauser = Avatar;
		SourceASC->ExecuteGameplayCue(MAGameplayTags::GameplayCue_Melee_Hit, CueParams);
	}
}
