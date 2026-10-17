// Copyright Fuly Games. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "NoclipMenuGameMode.generated.h"

/**
 * GameMode do mapa de menu (L_MainMenu): sem pawn, so a UI do menu principal.
 * Gera um Level 0 com seed aleatoria so de cenario, com a camera passeando devagar.
 */
UCLASS()
class NOCLIP_API ANoclipMenuGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	ANoclipMenuGameMode();

	virtual void InitGameState() override;
	virtual void HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer) override;
};
