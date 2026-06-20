#include "AbilitySystem/Abilities/GA_HitReact.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "AbilitySystem/MAGameplayTags.h"
#include "AIController.h"
#include "BrainComponent.h"
#include "GameFramework/Pawn.h"

UGA_HitReact::UGA_HitReact()
{
	// Server decides flinches (the damage event only exists server-side); activation and the
	// chosen montage replicate to the owning client and simulated proxies automatically.
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerInitiated;

	// Every hit staggers: without retrigger, hits landing during an active react (0.7s) are
	// swallowed and the victim's next attack "armors" through the rest of a combo.
	bRetriggerInstancedAbility = true;

	FGameplayTagContainer Tags;
	Tags.AddTag(MAGameplayTags::Ability_Reaction_HitReact);
	SetAssetTags(Tags);

	// Getting hit INTERRUPTS your attack (standard action-game stagger): the react montage
	// shares the UpperBody slot, so it stomps the combo montage -> the melee task's
	// OnInterrupted ends the ability and the chain dies. Casting stays protected for now —
	// interrupting a committed fireball would eat its cost+cooldown without a refund path.
	ActivationBlockedTags.AddTag(MAGameplayTags::State_Dead);
	ActivationBlockedTags.AddTag(MAGameplayTags::State_Casting);
	ActivationBlockedTags.AddTag(MAGameplayTags::State_Dodging);
	// While guarding, GA_Block absorbs hits with its own Block_Hit overlay — no flinch.
	ActivationBlockedTags.AddTag(MAGameplayTags::State_Blocking);

	// Auto-trigger from the AttributeSet's damage event.
	FAbilityTriggerData Trigger;
	Trigger.TriggerTag = MAGameplayTags::Event_Damage_Taken;
	Trigger.TriggerSource = EGameplayAbilityTriggerSource::GameplayEvent;
	AbilityTriggers.Add(Trigger);
}

void UGA_HitReact::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	if (!CommitAbility(Handle, ActorInfo, ActivationInfo) || HitReactMontages.Num() == 0)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	// Server-side random pick — the chosen montage asset replicates with the play call.
	UAnimMontage* Montage = HitReactMontages[FMath::RandRange(0, HitReactMontages.Num() - 1)];
	if (!Montage)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	UAbilityTask_PlayMontageAndWait* MontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(
		this, NAME_None, Montage, PlayRate);
	MontageTask->OnCompleted.AddDynamic(this, &UGA_HitReact::OnMontageFinished);
	MontageTask->OnInterrupted.AddDynamic(this, &UGA_HitReact::OnMontageFinished);
	MontageTask->OnCancelled.AddDynamic(this, &UGA_HitReact::OnMontageFinished);
	MontageTask->ReadyForActivation();

	// Hit-stun the AI's DECISIONS, not its movement: pause the behavior tree so a flinched enemy
	// stops circle-strafing out of the player's combo, while leaving MOVE_Walking intact so the
	// AttributeSet's knockback LaunchCharacter still shoves it (MOVE_None would swallow that — the
	// bug from the previous attempt). AI only — a human victim keeps full control. StopMovement
	// cancels the in-flight circle path. Resumed in EndAbility (covers end/interrupt/retrigger/death).
	if (const APawn* Pawn = Cast<APawn>(GetAvatarActorFromActorInfo()))
	{
		if (!Pawn->IsPlayerControlled())
		{
			if (AAIController* AI = Cast<AAIController>(Pawn->GetController()))
			{
				AI->StopMovement();
				if (UBrainComponent* Brain = AI->GetBrainComponent())
				{
					Brain->PauseLogic(TEXT("HitReact"));
				}
			}
		}
	}
}

void UGA_HitReact::EndAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility, bool bWasCancelled)
{
	// Resume the behavior tree paused on activate (no-op for players or pawns without an AI brain).
	if (const APawn* Pawn = Cast<APawn>(GetAvatarActorFromActorInfo()))
	{
		if (AAIController* AI = Cast<AAIController>(Pawn->GetController()))
		{
			if (UBrainComponent* Brain = AI->GetBrainComponent())
			{
				Brain->ResumeLogic(TEXT("HitReact"));
			}
		}
	}
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void UGA_HitReact::OnMontageFinished()
{
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}
