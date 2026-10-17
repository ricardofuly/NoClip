// Copyright Fuly Games. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UI/NoclipActivatableWidget.h"
#include "NoclipLobbyScreen.generated.h"

class UNoclipButton;
class UTextBlock;
class UVerticalBox;
class UUniformGridPanel;

/**
 * Saguao entre turnos (Lobby.dc.html): equipe e prontos, ordem de servico do proximo turno,
 * iniciar (lider) e o almoxarifado com o caixa da equipe. Voltar abre a pausa.
 */
UCLASS()
class NOCLIP_API UNoclipLobbyScreen : public UNoclipActivatableWidget
{
	GENERATED_BODY()

protected:
	virtual void BuildTree() override;
	virtual void NativeOnActivated() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual bool HandleBack() override;

private:
	void Refresh();
	void RefreshCrew();
	void RefreshShop();

	UTextBlock* RoomLabel = nullptr;
	UTextBlock* Title = nullptr;
	UTextBlock* CrewTitle = nullptr;
	UVerticalBox* CrewList = nullptr;
	UTextBlock* OrderTitle = nullptr;
	UTextBlock* QuotaValue = nullptr;
	UTextBlock* DurationValue = nullptr;
	UNoclipButton* ReadyButton = nullptr;
	UNoclipButton* StartButton = nullptr;
	UNoclipButton* MenuButton = nullptr;
	UTextBlock* StartNote = nullptr;
	UTextBlock* MoneyText = nullptr;
	UTextBlock* RadioKey = nullptr;
	TArray<UNoclipButton*> BuyButtons;
	TArray<UTextBlock*> PriceTexts;

	FString CrewSignature;
	float RefreshAccumulator = 1.f;
};
