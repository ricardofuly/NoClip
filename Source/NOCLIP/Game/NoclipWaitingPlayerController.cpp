// Copyright Fuly Games. All Rights Reserved.

#include "Game/NoclipWaitingPlayerController.h"
#include "Engine/GameViewportClient.h"
#include "Engine/LocalPlayer.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(NoclipWaitingPlayerController)

void ANoclipWaitingPlayerController::BeginPlay()
{
	Super::BeginPlay();
	if (IsLocalController())
	{
		SetWorldRenderingDisabled(true);
	}
}

void ANoclipWaitingPlayerController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (IsLocalController())
	{
		SetWorldRenderingDisabled(false);
	}
	Super::EndPlay(EndPlayReason);
}

void ANoclipWaitingPlayerController::SetWorldRenderingDisabled(bool bDisabled)
{
	const ULocalPlayer* LP = GetLocalPlayer();
	if (UGameViewportClient* Viewport = LP ? LP->ViewportClient.Get() : nullptr)
	{
		Viewport->bDisableWorldRendering = bDisabled;
	}
}
