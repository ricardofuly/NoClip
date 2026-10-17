// Copyright Fuly Games. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Game/NoclipPlayerController.h"
#include "NoclipMenuPlayerController.generated.h"

class ACameraActor;

/** PlayerController do menu principal: abre a UI de menu e passeia a camera pelo cenario. */
UCLASS()
class NOCLIP_API ANoclipMenuPlayerController : public ANoclipPlayerController
{
	GENERATED_BODY()

public:
	virtual bool IsFrontend() const override { return true; }

	/** Graus por segundo do giro lento da camera de fundo. */
	UPROPERTY(EditAnywhere, Category = "Menu")
	float CameraYawSpeed = 3.f;

protected:
	virtual void BeginPlay() override;
	virtual void PlayerTick(float DeltaTime) override;

private:
	UPROPERTY(Transient)
	TObjectPtr<ACameraActor> MenuCamera;

	float CameraTime = 0.f;
	FVector CameraBase = FVector::ZeroVector;
};
