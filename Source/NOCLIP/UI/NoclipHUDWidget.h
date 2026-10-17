// Copyright Fuly Games. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UI/NoclipActivatableWidget.h"
#include "UI/NoclipUIStyle.h"
#include "NoclipHUDWidget.generated.h"

class UCanvasPanel;
class UTextBlock;
class UBorder;
class UVerticalBox;
class ANoclipExtractionZone;

/**
 * HUD do expediente (HUD.dc.html), camada Game com input de jogo. Le o GameState replicado e o pawn local:
 * relogio e fase, quota x valor na Sala Manila, dica de agarrar, item segurado com folego, elevador e equipe.
 * As teclas trocam entre teclado+mouse, Xbox e PlayStation conforme o ultimo dispositivo usado.
 */
UCLASS()
class NOCLIP_API UNoclipHUDWidget : public UNoclipActivatableWidget
{
	GENERATED_BODY()

public:
	UNoclipHUDWidget(const FObjectInitializer& ObjectInitializer);

protected:
	virtual void BuildTree() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
	struct FHint
	{
		UWidget* Root = nullptr;
		UTextBlock* Key = nullptr;
		UTextBlock* Label = nullptr;
	};

	struct FCrewRow
	{
		UWidget* Root = nullptr;
		UBorder* Dot = nullptr;
		UTextBlock* Name = nullptr;
		UTextBlock* State = nullptr;
	};

	FHint MakeHint(const FText& Label, float FontPx);
	void Refresh();
	void RefreshTimer(const class ANoclipGameState* GS);
	void RefreshQuota(const class ANoclipGameState* GS);
	void RefreshCenter(const class ANoclipGameState* GS, const class ANoclipCharacter* Pawn);
	void RefreshHeld(const class ANoclipCharacter* Pawn);
	void RefreshExtraction(const class ANoclipGameState* GS);
	void RefreshCrew(const class ANoclipGameState* GS);
	void RefreshTools(const class ANoclipGameState* GS, const class ANoclipCharacter* Pawn);
	void RefreshDeny(const class ANoclipCharacter* Pawn);
	void RefreshHurt(const class ANoclipCharacter* Pawn);
	void RefreshMic();
	void RefreshPhone(const class ANoclipGameState* GS, const class ANoclipCharacter* Pawn);
	void RefreshDeath(const class ANoclipGameState* GS);
	void RefreshPower(const class ANoclipGameState* GS, const class ANoclipCharacter* Pawn);
	void RefreshNoclip(const class ANoclipCharacter* Pawn);
	ANoclipExtractionZone* GetZone();

	UCanvasPanel* Canvas = nullptr;
	UBorder* CollapseTint = nullptr;
	/** Flash vermelho ao levar dano. */
	UBorder* HurtTint = nullptr;
	/** Microfone aberto / falando. */
	UWidget* MicChip = nullptr;
	UTextBlock* MicText = nullptr;

	// Topo esquerdo
	UTextBlock* PhaseText = nullptr;
	UTextBlock* ClockText = nullptr;
	UWidget* CollapseBanner = nullptr;

	// Contagem (preparando)
	UWidget* CountdownPanel = nullptr;
	UTextBlock* CountdownText = nullptr;
	UTextBlock* CountdownTitle = nullptr;
	UTextBlock* CountdownInfo = nullptr;

	// Topo direito
	UWidget* QuotaChip = nullptr;
	UTextBlock* InRoomText = nullptr;
	UTextBlock* QuotaText = nullptr;
	UTextBlock* QuotaHintText = nullptr;
	NoclipUI::FBar QuotaBar;

	// Centro
	UWidget* PromptPanel = nullptr;
	FHint GrabHint;
	UTextBlock* PromptDetail = nullptr;
	NoclipUI::FBar ThrowBar;

	// Elevador
	UWidget* ElevatorPanel = nullptr;
	UTextBlock* ElevatorText = nullptr;
	NoclipUI::FBar ElevatorBar;

	// Item segurado + folego
	UWidget* HeldChip = nullptr;
	UWidget* HeldTop = nullptr;
	UWidget* HeldHints = nullptr;
	UTextBlock* HeldName = nullptr;
	UTextBlock* HeldValue = nullptr;
	UTextBlock* HeldLost = nullptr;
	/** Linha de humor do achado (catalogo do escritorio). */
	UTextBlock* HeldDesc = nullptr;
	FHint DropHint;
	FHint ThrowHint;
	NoclipUI::FBar StaminaBar;
	UWidget* HealthRow = nullptr;
	NoclipUI::FBar HealthBar;

	// Cinto de ferramentas (centro embaixo): 1 lanterna, 2 kit medico, 3 celular
	struct FToolSlot
	{
		UBorder* Box = nullptr;
		UTextBlock* Key = nullptr;
		UTextBlock* Name = nullptr;
		UTextBlock* Extra = nullptr;
		NoclipUI::FBar Battery;
	};
	UWidget* ToolBelt = nullptr;
	TArray<FToolSlot> ToolSlots;
	UWidget* ToolHintRow = nullptr;
	FHint UseHint;
	FHint ToolDropHint;
	UWidget* UsePanel = nullptr;
	UTextBlock* UseText = nullptr;
	NoclipUI::FBar UseBar;

	// Aviso de maos ocupadas ("nao pode")
	UWidget* DenyPanel = nullptr;
	UTextBlock* DenyTitle = nullptr;
	UTextBlock* DenyDetail = nullptr;

	// Tela do celular levantado
	UWidget* PhonePanel = nullptr;
	UTextBlock* PhoneClock = nullptr;
	UTextBlock* PhoneShift = nullptr;
	UTextBlock* PhoneArrow = nullptr;
	UTextBlock* PhoneDistance = nullptr;
	UTextBlock* PhoneQuota = nullptr;
	UTextBlock* PhoneSignal = nullptr;
	UTextBlock* PhoneFooter = nullptr;

	// Equipe
	TArray<FCrewRow> CrewRows;

	UTextBlock* PauseHint = nullptr;

	// Apagao: aviso no topo e QTE do gerador
	UWidget* PowerBanner = nullptr;
	UTextBlock* PowerTitle = nullptr;
	UTextBlock* PowerDetail = nullptr;
	UWidget* QTEPanel = nullptr;
	UTextBlock* QTEText = nullptr;
	UWidget* QTETrack = nullptr;
	class USizeBox* QTEZone = nullptr;
	UBorder* QTEZoneBox = nullptr;
	UWidget* QTENeedle = nullptr;
	UTextBlock* QTEHint = nullptr;

	// Noclip: imagem falhando e aviso de andar
	UBorder* GlitchTint = nullptr;
	UTextBlock* FloorNotice = nullptr;
	/** Rotulo do alvo no celular (Sala Manila / gerador / saida de servico). */
	UTextBlock* PhoneTarget = nullptr;

	// Morte / espectador
	UWidget* Crosshair = nullptr;
	UWidget* DeathPanel = nullptr;
	UTextBlock* DeathCause = nullptr;
	UTextBlock* DeathWatching = nullptr;
	UTextBlock* DeathHint = nullptr;
	UTextBlock* DeathNote = nullptr;

	TWeakObjectPtr<ANoclipExtractionZone> CachedZone;
	float RefreshAccumulator = 0.f;
};
