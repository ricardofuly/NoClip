// Copyright Fuly Games. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "NoclipPlayerController.generated.h"

class UInputAction;
class UInputMappingContext;
class UNoclipPrimaryLayout;
struct FInputActionValue;

/**
 * PlayerController: cria a UI (CommonUI) do jogador local, abre a pausa e cuida do
 * "segure para iniciar o expediente" do host. Tambem tem comandos de console para testar sessoes.
 */
UCLASS()
class NOCLIP_API ANoclipPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	ANoclipPlayerController();

	/** NoclipHost [mapa] — cria a sessao e abre o mapa como listen server. */
	UFUNCTION(Exec)
	void NoclipHost(const FString& MapPath);

	/** NoclipFind — procura sessoes do NOCLIP e lista no log. */
	UFUNCTION(Exec)
	void NoclipFind();

	/** NoclipJoin N — entra na sessao N da ultima busca. */
	UFUNCTION(Exec)
	void NoclipJoin(int32 Index);

	/** NoclipSeed N — servidor regera o nivel com a seed N. */
	UFUNCTION(Exec)
	void NoclipSeed(int32 Seed);

	/** NoclipStart — host inicia o proximo turno (no saguao). */
	UFUNCTION(Exec, BlueprintCallable, Category = "Noclip|Shift")
	void NoclipStart();

	/** NoclipPause — abre/fecha a pausa (atalho de console). */
	UFUNCTION(Exec)
	void NoclipPause();

	UNoclipPrimaryLayout* GetUILayout() const { return UILayout; }

	/** Alguma tela de menu (pausa, relatorio, dialogo) esta aberta? */
	bool IsUIMenuOpen() const;

	/** 0..1 enquanto o host segura o botao de iniciar; 0 caso contrario. */
	float GetReadyHoldProgress() const;

	/** Este jogador pode iniciar o expediente agora (host, no saguao)? */
	bool CanStartShift() const;

	void QuitToDesktop();

	/** Pronto no saguao. */
	/** Veste o cracha de outro estagiario (indice em NoclipCharacters). Salva nesta maquina e avisa o servidor. */
	void RequestSetCharacter(int32 CharacterIndex);

	void RequestSetReady(bool bReady);

	/** Compra um item do almoxarifado com o caixa da equipe. */
	void RequestBuyItem(int32 ItemIndex);

	/** Lider: "Ir para o saguao agora" / "Novo estagio". */
	void RequestSkipToLobby();

	/** Abre o convite de amigos do overlay da Steam (se houver). Retorna false se nao deu. */
	bool ShowInviteOverlay();

	/** Sai da sala e volta ao menu principal. */
	void LeaveToMainMenu();

	// ---------- Espectador (morreu no turno) ----------
	/** Morto e sem corpo: a camera segue um colega vivo. */
	bool IsSpectating() const;
	/** Colega que a camera esta seguindo (nulo = ninguem de pe, a camera fica no lugar da morte). */
	class ANoclipCharacter* GetSpectateTarget() const;

	virtual void CalcCamera(float DeltaTime, struct FMinimalViewInfo& OutResult) override;

	/** Mapa de menu: UI de menu principal em vez de HUD. */
	virtual bool IsFrontend() const { return false; }

	/** Sala de espera (depois de hospedar): UI da camera de seguranca, sem HUD. */
	virtual bool IsWaitingRoom() const { return false; }

	// ---------- Input ----------
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputMappingContext> DefaultMappingContext;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> PauseAction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> ReadyAction;

	/** Quanto tempo segurar para iniciar o expediente (evita comecar sem querer). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input", meta = (ClampMin = "0"))
	float ReadyHoldSeconds = 1.f;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void SetupInputComponent() override;
	virtual void PlayerTick(float DeltaTime) override;

	UFUNCTION(Server, Reliable)
	void Server_SetSeed(int32 Seed);

	UFUNCTION(Server, Reliable)
	void Server_RequestStartShift();

	UFUNCTION(Server, Reliable)
	void Server_SetReady(bool bReady);

	UFUNCTION(Server, Reliable)
	void Server_BuyItem(int32 ItemIndex);

	UFUNCTION(Server, Reliable)
	void Server_SkipToLobby();

	UFUNCTION(Server, Reliable)
	void Server_SetCharacter(int32 CharacterIndex);

private:
	void CreateUI();
	void OnPauseStarted(const FInputActionValue& Value);
	void OnReadyStarted(const FInputActionValue& Value);
	void OnReadyCompleted(const FInputActionValue& Value);

	UPROPERTY(Transient)
	TObjectPtr<UNoclipPrimaryLayout> UILayout;

	TWeakObjectPtr<class ANoclipLevelGenerator> CachedGenerator;
	double ReadyHoldStartTime = -1.0;
	int32 LastVisualSeed = 0;
	float VisualApplyDelay = -1.f;
	bool bReadyFired = false;

	void TickSpectate(float DeltaTime);
	void CycleSpectate(int32 Direction);
	TWeakObjectPtr<class ANoclipCharacter> SpectateTarget;
	FVector LastAliveLocation = FVector::ZeroVector;
	FVector SpectatePivot = FVector::ZeroVector;
	bool bSpectatePivotValid = false;
	float SpectateYaw = 0.f;
	float SpectatePitch = -18.f;
	bool bWasSpectating = false;
};
