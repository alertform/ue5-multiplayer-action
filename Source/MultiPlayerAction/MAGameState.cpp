#include "MAGameState.h"
#include "Player/MAPlayerController.h"
#include "Net/UnrealNetwork.h"
#include "Engine/Engine.h"
#include "Engine/World.h"

void AMAGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AMAGameState, MatchEndServerTime);
	DOREPLIFETIME(AMAGameState, KillTarget);
	DOREPLIFETIME(AMAGameState, WinnerName);
	DOREPLIFETIME(AMAGameState, RestartServerTime);
}

void AMAGameState::Multicast_OnKill_Implementation(const FString& KillerName, const FString& VictimName)
{
	OnKillEvent.Broadcast(KillerName, VictimName);
}

void AMAGameState::Multicast_OnTaunt_Implementation(const FString& Text)
{
	OnTauntEvent.Broadcast(Text);
}

void AMAGameState::HandleMatchHasEnded()
{
	Super::HandleMatchHasEnded();

	// Local-player reaction: drop to UI-only input (the same mode PC::BeginPlay reclaims
	// after the restart travel) and pin the scoreboard with the verdict. Dedicated servers
	// have no local PC and skip; the authoritative freeze lives in the GameMode.
	if (AMAPlayerController* PC = Cast<AMAPlayerController>(GEngine->GetFirstLocalPlayerController(GetWorld())))
	{
		PC->OnLocalMatchEnded();
	}
}
