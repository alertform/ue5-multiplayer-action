#include "AI/BTService_UpdateTargetInfo.h"
#include "AIController.h"
#include "AI/Combat/MATacticalAdvisorSubsystem.h"
#include "BehaviorTree/BehaviorTreeComponent.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "Dialogue/MADialogueSubsystem.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Player/MAPlayerController.h"
#include "NavigationSystem.h"
#include "NavigationPath.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystem/MAGameplayTags.h"

UBTService_UpdateTargetInfo::UBTService_UpdateTargetInfo()
{
	NodeName = TEXT("Update Target Info");
	Interval = 0.5f;
	RandomDeviation = 0.f;

	TargetActorKey.AddObjectFilter(this, GET_MEMBER_NAME_CHECKED(UBTService_UpdateTargetInfo, TargetActorKey), AActor::StaticClass());
	InRangeKey.AddBoolFilter(this, GET_MEMBER_NAME_CHECKED(UBTService_UpdateTargetInfo, InRangeKey));
}

void UBTService_UpdateTargetInfo::TickNode(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds)
{
	Super::TickNode(OwnerComp, NodeMemory, DeltaSeconds);

	UBlackboardComponent* BB = OwnerComp.GetBlackboardComponent();
	AAIController* AIC = OwnerComp.GetAIOwner();
	if (!BB || !AIC) return;

	APawn* MyPawn = AIC->GetPawn();
	if (!MyPawn) return;

	// Target = nearest LIVING player pawn, preferring REACHABLE ones.
	// - GetPlayerPawn(0) only ever tracked the host's pawn — clients were invisible in multiplayer.
	// - Dead pawns are skipped: the ragdoll persists through the death→respawn gap.
	// - Unreachable pawns (no nav path, e.g. standing where the navmesh is disconnected) lose to
	//   reachable ones regardless of distance — otherwise the AI locks onto a target it can never
	//   reach instead of switching. If NO ONE is reachable we still take the nearest living pawn,
	//   so close-range rotate/attack keeps working without a path.
	UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(MyPawn->GetWorld());
	const FVector MyLocation = MyPawn->GetActorLocation();

	// 对话中的玩家享有豁免：AI 不选其为目标（对话 = 安全时刻）。
	const UMADialogueSubsystem* Dialogue = MyPawn->GetWorld()->GetSubsystem<UMADialogueSubsystem>();

	APawn* PlayerPawn = nullptr;
	float BestDistSq = TNumericLimits<float>::Max();
	bool bBestReachable = false;
	for (FConstPlayerControllerIterator It = MyPawn->GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		const APlayerController* PC = It->Get();
		APawn* Candidate = PC ? PC->GetPawn() : nullptr;
		if (!Candidate)
		{
			continue;
		}

		if (Dialogue && Dialogue->IsInDialogue(Cast<AMAPlayerController>(PC)))
		{
			continue;
		}

		if (const UAbilitySystemComponent* ASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Candidate))
		{
			if (ASC->HasMatchingGameplayTag(MAGameplayTags::State_Dead))
			{
				continue;
			}
		}

		// Synchronous path probe — cheap at this service's 0.5s interval with ≤ a handful of players.
		bool bReachable = false;
		if (NavSys)
		{
			const UNavigationPath* Path = NavSys->FindPathToLocationSynchronously(
				MyPawn->GetWorld(), MyLocation, Candidate->GetActorLocation(), MyPawn);
			bReachable = Path && Path->IsValid() && !Path->IsPartial();
		}

		const float DistSq = FVector::DistSquared(MyLocation, Candidate->GetActorLocation());
		// Reachability outranks distance; distance breaks ties within the same tier.
		const bool bBetter = (bReachable && !bBestReachable)
			|| (bReachable == bBestReachable && DistSq < BestDistSq);
		if (bBetter)
		{
			BestDistSq = DistSq;
			PlayerPawn = Candidate;
			bBestReachable = bReachable;
		}
	}

	// LLM 军师指定了集火目标时优先采纳 —— 但必须通过与普通候选相同的硬校验
	// （存活、非对话中；死亡/失效由 GetFocusPawn 的弱指针与上面的循环兜底）。
	if (const UMATacticalAdvisorSubsystem* Advisor = MyPawn->GetWorld()->GetSubsystem<UMATacticalAdvisorSubsystem>())
	{
		if (APawn* Focus = Advisor->GetFocusPawn())
		{
			bool bFocusDead = false;
			if (const UAbilitySystemComponent* ASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Focus))
			{
				bFocusDead = ASC->HasMatchingGameplayTag(MAGameplayTags::State_Dead);
			}
			const bool bFocusInDialogue = Dialogue &&
				Dialogue->IsInDialogue(Cast<AMAPlayerController>(Focus->GetController()));
			if (!bFocusDead && !bFocusInDialogue)
			{
				PlayerPawn = Focus;
				BestDistSq = FVector::DistSquared(MyLocation, Focus->GetActorLocation());
			}
		}
	}

	BB->SetValueAsObject(TargetActorKey.SelectedKeyName, PlayerPawn);

	const bool bInRange = PlayerPawn && BestDistSq <= AttackRange * AttackRange;
	BB->SetValueAsBool(InRangeKey.SelectedKeyName, bInRange);

	// 观测到目标 = 常驻 focus：接近/绕圈/后撤全程身体面向玩家（八向 strafe 补移动动画），
	// ControlRotation 经 CharacterMovement 的期望朝向按 RotationRate 平滑转过去。
	// 目标丢失（无人存活/死亡间隙）回落到路径朝向；死亡清理用同一优先级 ClearFocus。
	if (PlayerPawn)
	{
		AIC->SetFocus(PlayerPawn, EAIFocusPriority::Gameplay);
	}
	else
	{
		AIC->ClearFocus(EAIFocusPriority::Gameplay);
	}
}
