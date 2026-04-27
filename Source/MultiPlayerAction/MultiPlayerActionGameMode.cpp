// Copyright Epic Games, Inc. All Rights Reserved.

#include "MultiPlayerActionGameMode.h"
#include "MultiPlayerActionCharacter.h"
#include "Player/MAPlayerState.h"
#include "UObject/ConstructorHelpers.h"

AMultiPlayerActionGameMode::AMultiPlayerActionGameMode()
{
	// Use our custom PlayerState that owns the ASC
	PlayerStateClass = AMAPlayerState::StaticClass();

	// set default pawn class to our Blueprinted character
	static ConstructorHelpers::FClassFinder<APawn> PlayerPawnBPClass(TEXT("/Game/ThirdPerson/Blueprints/BP_ThirdPersonCharacter"));
	if (PlayerPawnBPClass.Class != NULL)
	{
		DefaultPawnClass = PlayerPawnBPClass.Class;
	}
}
