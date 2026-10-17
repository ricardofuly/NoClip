// Copyright Fuly Games. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UI/NoclipActivatableWidget.h"
#include "NoclipPauseMenu.generated.h"

class UNoclipButton;
class UTextBlock;
class UVerticalBox;

/**
 * Pausa (Pause.dc.html). O expediente continua rodando para todos. Continuar, opcoes, convidar amigos
 * (overlay da Steam) e sair para o menu com confirmacao. Ao lado: equipe, quota e codigo de convite.
 */
UCLASS()
class NOCLIP_API UNoclipPauseMenu : public UNoclipActivatableWidget
{
	GENERATED_BODY()

protected:
	virtual void BuildTree() override;
	virtual void NativeOnActivated() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
	void RefreshDynamic();
	void HandleLeave();

	UNoclipButton* ContinueButton = nullptr;
	UTextBlock* CrewTitle = nullptr;
	UVerticalBox* CrewList = nullptr;
	UTextBlock* QuotaLine = nullptr;
	UTextBlock* CodeText = nullptr;
	FString CrewSignature;
	float RefreshAccumulator = 1.f;
};
