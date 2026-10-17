// Copyright Fuly Games. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "CommonUserWidget.h"
#include "Game/NoclipShiftTypes.h"
#include "NoclipPrimaryLayout.generated.h"

class UCommonActivatableWidget;
class UCommonActivatableWidgetStack;
class UNoclipHUDWidget;
class ANoclipGameState;
class UBorder;
class UTextBlock;
class UImage;
class UMaterialInstanceDynamic;

/**
 * Raiz da UI de cada jogador local (um por viewport). Camadas de CommonUI:
 *  - Game: a HUD (input de jogo)
 *  - Menu: saguao, pausa, relatorio, demissao, opcoes (input de UI)
 *  - Modal: dialogos de confirmacao (por cima do menu, que continua visivel)
 * Por cima de tudo: "aguarde", avisos e as linhas de varredura do prototipo.
 * No jogo, abre o saguao/relatorio sozinho conforme a fase do turno.
 */
UCLASS()
class NOCLIP_API UNoclipPrimaryLayout : public UCommonUserWidget
{
	GENERATED_BODY()

public:
	void ShowHUD();

	/** Menu principal (mapa de menu): sem HUD e sem pausa. */
	void ShowMainMenu();

	bool IsFrontend() const { return bFrontend; }

	/** Sala de espera (camera de seguranca): sem HUD, sem pausa, sem telas de turno. */
	void ShowWaitingRoom();
	bool IsWaitingRoom() const { return bWaitingRoom; }

	/** Abre/fecha a pausa (por cima do saguao ou da HUD). */
	void TogglePauseMenu();

	bool IsMenuOpen() const;

	/** A tela que deve ter o foco agora (dialogo por cima da pausa, por exemplo). */
	UCommonActivatableWidget* GetTopMenu() const;

	/** Empilha uma tela na camada de menu. */
	UCommonActivatableWidget* PushMenu(TSubclassOf<UCommonActivatableWidget> WidgetClass);

	/**
	 * Dialogo de confirmacao (memorando manila). O foco comeca em Cancelar (acao segura); Voltar tambem cancela.
	 * Eyebrow = texto pequeno acima do titulo.
	 */
	void PushConfirm(const FText& Title, const FText& Body, const FText& ConfirmLabel, TFunction<void()> OnConfirm, const FText& Eyebrow = FText::GetEmpty());

	/** Painel "aguarde" por cima de tudo (criando sala, entrando...). Texto vazio esconde. */
	void SetBusy(const FText& Message);
	bool IsBusy() const { return bBusy; }

	/** Aviso curto no rodape (erro de rede...). Some sozinho. */
	void ShowToast(const FText& Message, bool bError = true);

protected:
	virtual void InitializeNativeClassData() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual void NativeDestruct() override;

	void TryBindGameState();
	void RefreshCursor();
	void RefreshScanlines();
	void SyncPhaseScreens();
	void ShowShiftReport();

	UFUNCTION()
	void HandleShiftEnded(const FNoclipShiftResult& Result);

	UFUNCTION()
	void HandlePhaseChanged(ENoclipShiftPhase OldPhase, ENoclipShiftPhase NewPhase);

	UPROPERTY(Transient) TObjectPtr<UCommonActivatableWidgetStack> GameStack;
	UPROPERTY(Transient) TObjectPtr<UCommonActivatableWidgetStack> MenuStack;
	UPROPERTY(Transient) TObjectPtr<UCommonActivatableWidgetStack> ModalStack;
	UPROPERTY(Transient) TObjectPtr<UNoclipHUDWidget> HUD;
	UPROPERTY(Transient) TObjectPtr<UBorder> BusyPanel;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> BusyText;
	UPROPERTY(Transient) TObjectPtr<UBorder> ToastPanel;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> ToastText;
	UPROPERTY(Transient) TObjectPtr<UImage> Scanlines;
	UPROPERTY(Transient) TObjectPtr<UMaterialInstanceDynamic> ScanlinesMID;

	TWeakObjectPtr<ANoclipGameState> BoundGameState;
	TWeakObjectPtr<UCommonActivatableWidget> ActiveReport;
	TWeakObjectPtr<UCommonActivatableWidget> ActiveLobby;
	bool bReportShownThisEnd = false;
	bool bFrontend = false;
	bool bWaitingRoom = false;
	bool bBusy = false;
	float ToastRemaining = 0.f;
	float LastScanScale = -1.f;
};
