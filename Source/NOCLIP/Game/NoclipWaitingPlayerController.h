// Copyright Fuly Games. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Game/NoclipPlayerController.h"
#include "NoclipWaitingPlayerController.generated.h"

/**
 * PlayerController da sala de espera: abre a tela da sala (camera de seguranca) e desliga a renderizacao
 * do mundo no viewport, ja que a tela cobre tudo e a camera e renderizada a parte.
 */
UCLASS()
class NOCLIP_API ANoclipWaitingPlayerController : public ANoclipPlayerController
{
	GENERATED_BODY()

public:
	virtual bool IsWaitingRoom() const override { return true; }

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	void SetWorldRenderingDisabled(bool bDisabled);
};
