// Copyright Fuly Games. All Rights Reserved.

#include "Game/NoclipMenuGameMode.h"
#include "Game/NoclipMenuPlayerController.h"
#include "Game/NoclipGameState.h"
#include "GameFramework/HUD.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(NoclipMenuGameMode)

ANoclipMenuGameMode::ANoclipMenuGameMode()
{
	DefaultPawnClass = nullptr;
	PlayerControllerClass = ANoclipMenuPlayerController::StaticClass();
	GameStateClass = ANoclipGameState::StaticClass();
	HUDClass = AHUD::StaticClass();
}

void ANoclipMenuGameMode::InitGameState()
{
	Super::InitGameState();
	if (ANoclipGameState* GS = GetGameState<ANoclipGameState>())
	{
		GS->SetLevelSeed(FMath::RandRange(1, MAX_int32 - 1));
	}
}

void ANoclipMenuGameMode::HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer)
{
	// Sem pawn no menu: nao chama RestartPlayer.
}
