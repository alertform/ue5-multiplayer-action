#include "AI/Combat/MAAIDefenseComponent.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "AI/Combat/MAAIDefensePolicy.h"
#include "AIController.h"
#include "AbilitySystem/MAGameplayTags.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "TimerManager.h"

// ma.AI.Debug is defined once in MACombatDirectorSubsystem.cpp — read it through the manager
// (same pattern as GA_MeleeAttack's ma.LagComp.Enabled mirror).
static bool MAAIDebugEnabled()
{
	static IConsoleVariable* Var = IConsoleManager::Get().FindConsoleVariable(TEXT("ma.AI.Debug"));
	return Var && Var->GetInt() != 0;
}

void UMAAIDefenseComponent::BindToBlackboard()
{
	AAIController* AIC = Cast<AAIController>(GetOwner());
	UBlackboardComponent* BB = AIC ? AIC->GetBlackboardComponent() : nullptr;
	if (!BB)
	{
		return;
	}

	BB->UnregisterObserversFrom(this); // idempotent re-bind (e.g. repossession)
	const FBlackboard::FKey KeyID = BB->GetKeyID(TargetActorKeyName);
	if (KeyID == FBlackboard::InvalidKey)
	{
		return;
	}
	BB->RegisterObserver(KeyID, this,
		FOnBlackboardChangeNotification::CreateUObject(this, &UMAAIDefenseComponent::OnTargetKeyChanged));

	// Pick up a target that was already set before we bound.
	BindTargetASC(Cast<AActor>(BB->GetValueAsObject(TargetActorKeyName)));
}

void UMAAIDefenseComponent::UnbindAll()
{
	if (AAIController* AIC = Cast<AAIController>(GetOwner()))
	{
		if (UBlackboardComponent* BB = AIC->GetBlackboardComponent())
		{
			BB->UnregisterObserversFrom(this);
		}
	}
	BindTargetASC(nullptr);
	if (UWorld* W = GetWorld())
	{
		W->GetTimerManager().ClearTimer(ReactionDelayTimer);
		W->GetTimerManager().ClearTimer(BlockHoldTimer);
	}
}

void UMAAIDefenseComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	UnbindAll();
	Super::EndPlay(EndPlayReason);
}

EBlackboardNotificationResult UMAAIDefenseComponent::OnTargetKeyChanged(const UBlackboardComponent& BB, FBlackboard::FKey Key)
{
	BindTargetASC(Cast<AActor>(BB.GetValueAsObject(TargetActorKeyName)));
	return EBlackboardNotificationResult::ContinueObserving;
}

void UMAAIDefenseComponent::BindTargetASC(AActor* NewTarget)
{
	UAbilitySystemComponent* NewASC = NewTarget
		? UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(NewTarget) : nullptr;
	if (TargetASC.Get() == NewASC)
	{
		return;
	}

	if (UAbilitySystemComponent* Old = TargetASC.Get())
	{
		Old->UnregisterGameplayTagEvent(TargetTagHandle, MAGameplayTags::State_Attacking,
			EGameplayTagEventType::NewOrRemoved);
	}
	TargetTagHandle.Reset();
	TargetASC = NewASC;

	if (NewASC)
	{
		TargetTagHandle = NewASC->RegisterGameplayTagEvent(MAGameplayTags::State_Attacking,
			EGameplayTagEventType::NewOrRemoved)
			.AddUObject(this, &UMAAIDefenseComponent::OnTargetAttackingChanged);
	}
}

void UMAAIDefenseComponent::OnTargetAttackingChanged(const FGameplayTag Tag, int32 NewCount)
{
	if (NewCount <= 0)
	{
		return; // swing ended — only react to starts
	}

	APawn* MyPawn = GetControlledPawn();
	AActor* TargetAvatar = TargetASC.IsValid() ? TargetASC->GetAvatarActor_Direct() : nullptr;
	UWorld* W = GetWorld();
	if (!MyPawn || !TargetAvatar || !W)
	{
		return;
	}

	FMADefenseParams Params;
	Params.ReactRangeCm = ReactRangeCm;
	Params.Chance = ReactChance;
	Params.CooldownSeconds = ReactionCooldownSeconds;

	const float Dist = FVector::Dist(MyPawn->GetActorLocation(), TargetAvatar->GetActorLocation());
	const float Now = W->GetTimeSeconds();
	if (!MAAICombat::ShouldReactToAttack(Dist, Now - LastReactionTime, FMath::FRand(), Params))
	{
		return;
	}

	LastReactionTime = Now;
	W->GetTimerManager().SetTimer(ReactionDelayTimer, this, &UMAAIDefenseComponent::StartBlock,
		ReactionDelaySeconds, false);
}

void UMAAIDefenseComponent::StartBlock()
{
	APawn* MyPawn = GetControlledPawn();
	UAbilitySystemComponent* MyASC = GetOwnASC();
	UWorld* W = GetWorld();
	if (!MyPawn || !MyASC || !W)
	{
		return;
	}

	FGameplayTagContainer Tags;
	Tags.AddTag(MAGameplayTags::Ability_Defense_Block);
	if (!MyASC->TryActivateAbilitiesByTag(Tags))
	{
		return; // dead / staggered / already blocking — existing tag rules decide
	}

	W->GetTimerManager().SetTimer(BlockHoldTimer, this, &UMAAIDefenseComponent::StopBlock,
		BlockHoldSeconds, false);

	if (MAAIDebugEnabled())
	{
		DrawDebugString(W, MyPawn->GetActorLocation() + FVector(0.f, 0.f, 140.f),
			TEXT("BLK"), nullptr, FColor::Cyan, BlockHoldSeconds, /*bDrawShadow=*/true);
	}
}

void UMAAIDefenseComponent::StopBlock()
{
	if (UAbilitySystemComponent* MyASC = GetOwnASC())
	{
		// The AI mirror of the player's input release (MultiPlayerActionCharacter.cpp pattern).
		FGameplayTagContainer Tags;
		Tags.AddTag(MAGameplayTags::Ability_Defense_Block);
		MyASC->CancelAbilities(&Tags);
	}
}

APawn* UMAAIDefenseComponent::GetControlledPawn() const
{
	const AAIController* AIC = Cast<AAIController>(GetOwner());
	return AIC ? AIC->GetPawn() : nullptr;
}

UAbilitySystemComponent* UMAAIDefenseComponent::GetOwnASC() const
{
	APawn* MyPawn = GetControlledPawn();
	return MyPawn ? UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(MyPawn) : nullptr;
}
