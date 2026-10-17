// Copyright Fuly Games. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "NoclipDebugHUD.generated.h"

/**
 * HUD provisorio desenhado em canvas, so para testar o loop.
 * A UI final (UMG/CommonUI) segue o prototipo feito no Claude Design.
 */
UCLASS()
class NOCLIP_API ANoclipDebugHUD : public AHUD
{
	GENERATED_BODY()

public:
	virtual void DrawHUD() override;

private:
	void DrawLine(const FString& Text, float X, float& Y, const FLinearColor& Color, float Scale = 1.f);
};
