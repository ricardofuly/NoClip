// Copyright Fuly Games. All Rights Reserved.

#include "Game/NoclipMenuPlayerController.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/PlayerStart.h"
#include "EngineUtils.h"
#include "Engine/World.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(NoclipMenuPlayerController)

void ANoclipMenuPlayerController::BeginPlay()
{
	Super::BeginPlay();

	if (!IsLocalController())
	{
		return;
	}

	// Camera na altura dos olhos, sobre o primeiro PlayerStart.
	CameraBase = FVector(0.f, 0.f, 150.f);
	for (TActorIterator<APlayerStart> It(GetWorld()); It; ++It)
	{
		CameraBase = It->GetActorLocation() + FVector(0.f, 0.f, 60.f);
		break;
	}

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	MenuCamera = GetWorld()->SpawnActor<ACameraActor>(CameraBase, FRotator(-4.f, 0.f, 0.f), Params);
	if (MenuCamera)
	{
		MenuCamera->GetCameraComponent()->SetFieldOfView(80.f);
		MenuCamera->GetCameraComponent()->bConstrainAspectRatio = false;
		SetViewTarget(MenuCamera);
	}
}

void ANoclipMenuPlayerController::PlayerTick(float DeltaTime)
{
	Super::PlayerTick(DeltaTime);

	if (!MenuCamera)
	{
		return;
	}
	CameraTime += DeltaTime;
	// Giro lento + respiracao sutil (parece alguem parado olhando o corredor).
	const FRotator Rot(-4.f + FMath::Sin(CameraTime * 0.35f) * 1.5f, CameraTime * CameraYawSpeed, FMath::Sin(CameraTime * 0.5f) * 0.6f);
	const FVector Loc = CameraBase + FVector(0.f, 0.f, FMath::Sin(CameraTime * 0.9f) * 1.5f);
	MenuCamera->SetActorLocationAndRotation(Loc, Rot);
	if (GetViewTarget() != MenuCamera)
	{
		SetViewTarget(MenuCamera);
	}
}
