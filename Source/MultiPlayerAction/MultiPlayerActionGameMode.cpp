// Copyright Epic Games, Inc. All Rights Reserved.

#include "MultiPlayerActionGameMode.h"
#include "MultiPlayerActionCharacter.h"
#include "MAGameState.h"
#include "Player/MAPlayerState.h"
#include "Player/MAPlayerController.h"
#include "AI/MATargetDummy.h"
#include "AbilitySystemComponent.h"
#include "Narrative/MANarrativeSubsystem.h"
#include "EngineUtils.h"
#include "TimerManager.h"
#include "UObject/ConstructorHelpers.h"

AMultiPlayerActionGameMode::AMultiPlayerActionGameMode()
{
	// Use our custom PlayerState that owns the ASC
	PlayerStateClass = AMAPlayerState::StaticClass();

	// Replicated match data (clock / kill target / verdict) for the deathmatch loop
	GameStateClass = AMAGameState::StaticClass();

	// PC binds WBP_HUD asset itself — no BP_PlayerController child needed
	PlayerControllerClass = AMAPlayerController::StaticClass();

	// set default pawn class to our Blueprinted character
	static ConstructorHelpers::FClassFinder<APawn> PlayerPawnBPClass(TEXT("/Game/ThirdPerson/Blueprints/BP_ThirdPersonCharacter"));
	if (PlayerPawnBPClass.Class != NULL)
	{
		DefaultPawnClass = PlayerPawnBPClass.Class;
	}
}

void AMultiPlayerActionGameMode::InitGameState()
{
	Super::InitGameState();

	// Push match config into replicated GameState so clients can render "K x / target".
	if (AMAGameState* GS = GetGameState<AMAGameState>())
	{
		GS->KillTarget = KillTarget;
	}
}

void AMultiPlayerActionGameMode::HandleMatchHasStarted()
{
	Super::HandleMatchHasStarted();

	if (AMAGameState* GS = GetGameState<AMAGameState>())
	{
		// <=0 = 剧情模式不限时：MatchEndServerTime 保持 -1，HUD 时钟隐藏、末段锁定不触发。
		if (MatchDuration > 0.f)
		{
			GS->MatchEndServerTime = GS->GetServerWorldTimeSeconds() + MatchDuration;
		}
	}
}

void AMultiPlayerActionGameMode::NotifyKill(AActor* KillerActor, AActor* VictimActor)
{
	if (!IsMatchInProgress())
	{
		return; // post-match stragglers don't move the board
	}

	// Killer resolution: player ASCs are owned by the PlayerState, dummy ASCs by the pawn.
	AMAPlayerState* KillerPS = Cast<AMAPlayerState>(KillerActor);
	if (!KillerPS)
	{
		if (const APawn* KillerPawn = Cast<APawn>(KillerActor))
		{
			KillerPS = KillerPawn->GetPlayerState<AMAPlayerState>();
		}
	}

	AMAPlayerState* VictimPS = nullptr;
	if (const APawn* VictimPawn = Cast<APawn>(VictimActor))
	{
		VictimPS = VictimPawn->GetPlayerState<AMAPlayerState>(); // null for dummies
	}

	if (VictimPS)
	{
		VictimPS->AddDeath();
	}
	if (KillerPS && KillerPS != VictimPS)
	{
		KillerPS->AddKill(); // suicides count the death but never the kill

		// 叙事任务进度与击杀计分同源 —— 同一处路由，保证口径一致。
		if (UMANarrativeSubsystem* Narrative = GetWorld()->GetSubsystem<UMANarrativeSubsystem>())
		{
			Narrative->NotifyKill(KillerPS);
		}
	}

	// Kill feed: dummies have no PlayerState — present them by a fixed name. A suicide
	// keeps the killer column empty (the feed renders "<victim> died").
	if (AMAGameState* GS = GetGameState<AMAGameState>())
	{
		const FString DummyName = TEXT("Dummy");
		const FString KillerName = (KillerPS && KillerPS != VictimPS) ? KillerPS->GetPlayerName()
			: (!KillerPS && KillerActor) ? DummyName : FString();
		const FString VictimName = VictimPS ? VictimPS->GetPlayerName() : DummyName;
		GS->Multicast_OnKill(KillerName, VictimName);
	}
	// Win condition is polled by AGameMode::Tick via ReadyToEndMatch — no check needed here.
}

bool AMultiPlayerActionGameMode::ReadyToEndMatch_Implementation()
{
	const AMAGameState* GS = GetGameState<AMAGameState>();
	if (!GS)
	{
		return false;
	}

	// Clock expired?
	if (GS->MatchEndServerTime > 0.f && GS->GetServerWorldTimeSeconds() >= GS->MatchEndServerTime)
	{
		return true;
	}

	// Kill target reached? (<=0 = 剧情模式不以杀数终结)
	if (KillTarget > 0)
	{
		for (APlayerState* PS : GS->PlayerArray)
		{
			const AMAPlayerState* MPS = Cast<AMAPlayerState>(PS);
			if (MPS && MPS->GetKills() >= KillTarget)
			{
				return true;
			}
		}
	}
	return false;
}

void AMultiPlayerActionGameMode::HandleMatchHasEnded()
{
	Super::HandleMatchHasEnded();

	// Verdict + post-match countdown. Written BEFORE the engine pushes WaitingPostMatch
	// into the GameState (AGameMode::SetMatchState runs OnMatchStateSet first), so the
	// fields replicate in the same bunch as the state flip — clients never render a
	// results screen with a missing winner.
	AMAGameState* GS = GetGameState<AMAGameState>();
	if (GS)
	{
		TArray<AMAPlayerState*> Ranked;
		for (APlayerState* PS : GS->PlayerArray)
		{
			if (AMAPlayerState* MPS = Cast<AMAPlayerState>(PS))
			{
				Ranked.Add(MPS);
			}
		}
		Ranked.Sort([](const AMAPlayerState& A, const AMAPlayerState& B)
		{
			if (A.GetKills() != B.GetKills())
			{
				return A.GetKills() > B.GetKills();
			}
			return A.GetDeaths() < B.GetDeaths();
		});

		if (Ranked.Num() == 1)
		{
			GS->WinnerName = Ranked[0]->GetPlayerName();
		}
		else if (Ranked.Num() > 1)
		{
			const bool bDraw = Ranked[0]->GetKills() == Ranked[1]->GetKills()
				&& Ranked[0]->GetDeaths() == Ranked[1]->GetDeaths();
			GS->WinnerName = bDraw ? FString() : Ranked[0]->GetPlayerName();
		}

		GS->RestartServerTime = GS->GetServerWorldTimeSeconds() + PostMatchDuration;
	}

	// Authoritative combat freeze. (Local input lockdown is replication-driven:
	// AMAGameState::HandleMatchHasEnded on each machine.)
	if (GS)
	{
		for (APlayerState* PS : GS->PlayerArray)
		{
			AMAPlayerState* MPS = Cast<AMAPlayerState>(PS);
			if (UAbilitySystemComponent* ASC = MPS ? MPS->GetAbilitySystemComponent() : nullptr)
			{
				ASC->CancelAbilities();
			}
		}
	}
	for (TActorIterator<AMATargetDummy> It(GetWorld()); It; ++It)
	{
		It->FreezeForPostMatch();
	}

	GetWorldTimerManager().SetTimer(RestartTimerHandle, this,
		&AMultiPlayerActionGameMode::RestartMatch, PostMatchDuration, false);
}

void AMultiPlayerActionGameMode::RestartMatch()
{
	// Full (non-seamless) reload of the current map: fresh GameState + PlayerStates
	// (scores zeroed by construction), clients auto-travel along; the NULL-OSS session
	// lives on the GameInstance and survives.
	RestartGame();
}
