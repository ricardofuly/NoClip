// Copyright Fuly Games. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameStateBase.h"
#include "Game/NoclipShiftTypes.h"
#include "NoclipGameState.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FNoclipLevelSeedReady, int32, Seed);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FNoclipPhaseChanged, ENoclipShiftPhase, OldPhase, ENoclipShiftPhase, NewPhase);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FNoclipShiftEnded, const FNoclipShiftResult&, Result);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FNoclipEconomyChanged);

/**
 * Estado compartilhado da sessao: seed do nivel, fase do turno, relogio, quota, dinheiro e resultado.
 * O servidor escreve; todos leem (HUD e UI se ligam nos delegates).
 */
UCLASS()
class NOCLIP_API ANoclipGameState : public AGameStateBase
{
	GENERATED_BODY()

public:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// ---------- Seed ----------
	/** So o servidor chama. */
	void SetLevelSeed(int32 NewSeed);

	UFUNCTION(BlueprintPure, Category = "Noclip")
	int32 GetLevelSeed() const { return LevelSeed; }

	UFUNCTION(BlueprintPure, Category = "Noclip")
	bool HasLevelSeed() const { return bHasLevelSeed; }

	UPROPERTY(BlueprintAssignable, Category = "Noclip")
	FNoclipLevelSeedReady OnLevelSeedReady;

	// ---------- Turno ----------
	UFUNCTION(BlueprintPure, Category = "Noclip|Shift")
	ENoclipShiftPhase GetPhase() const { return Phase; }

	/** Segundos restantes na fase atual (contagem, turno ou relatorio). */
	UFUNCTION(BlueprintPure, Category = "Noclip|Shift")
	float GetPhaseTimeRemaining() const;

	/** 0 no comeco da fase, 1 no fim (fases sem prazo: 0). */
	UFUNCTION(BlueprintPure, Category = "Noclip|Shift")
	float GetPhaseProgress() const;

	UFUNCTION(BlueprintPure, Category = "Noclip|Shift")
	int32 GetShiftIndex() const { return ShiftIndex; }

	UFUNCTION(BlueprintPure, Category = "Noclip|Shift")
	int32 GetQuota() const { return Quota; }

	/** Valor dos achados que estao agora dentro da Sala Manila. */
	UFUNCTION(BlueprintPure, Category = "Noclip|Shift")
	int32 GetValueInExtraction() const { return ValueInExtraction; }

	/** 0..1: quanto falta para o elevador subir (todos dentro da sala). */
	UFUNCTION(BlueprintPure, Category = "Noclip|Shift")
	float GetExtractionProgress() const { return ExtractionProgress; }

	UFUNCTION(BlueprintPure, Category = "Noclip|Shift")
	int32 GetPlayersInExtraction() const { return PlayersInExtraction; }

	UFUNCTION(BlueprintPure, Category = "Noclip|Shift")
	int32 GetMoney() const { return Money; }

	UFUNCTION(BlueprintPure, Category = "Noclip|Shift")
	const FNoclipShiftResult& GetLastResult() const { return LastResult; }

	UPROPERTY(BlueprintAssignable, Category = "Noclip|Shift")
	FNoclipPhaseChanged OnPhaseChanged;

	UPROPERTY(BlueprintAssignable, Category = "Noclip|Shift")
	FNoclipShiftEnded OnShiftEnded;

	UPROPERTY(BlueprintAssignable, Category = "Noclip|Shift")
	FNoclipEconomyChanged OnEconomyChanged;

	/**
	 * Lider do grupo: quem inicia o expediente. No listen server e o host;
	 * em servidor dedicado (ou PIE "Play As Client") e o primeiro jogador que entrou.
	 */
	UFUNCTION(BlueprintPure, Category = "Noclip|Shift")
	APlayerState* GetLeader() const { return Leader; }

	// ---------- Sala ----------
	UFUNCTION(BlueprintPure, Category = "Noclip|Session")
	const FString& GetRoomName() const { return RoomName; }

	UFUNCTION(BlueprintPure, Category = "Noclip|Session")
	const FString& GetInviteCode() const { return InviteCode; }

	UFUNCTION(BlueprintPure, Category = "Noclip|Session")
	bool AllowsJoinInProgress() const { return bJoinInProgress; }

	/** Vagas da sala (configurado em Hospedar). */
	UFUNCTION(BlueprintPure, Category = "Noclip|Session")
	int32 GetMaxPlayers() const { return MaxPlayers; }

	/** 0 = so amigos, 1 = so convite, 2 = publica (ENoclipRoomPrivacy). */
	uint8 GetRoomPrivacy() const { return RoomPrivacy; }

	/** A equipe acabou de descer da sala de espera e o elevador ainda esta chegando (o turno 1 comeca sozinho). */
	UFUNCTION(BlueprintPure, Category = "Noclip|Session")
	bool IsArrivalPending() const { return bArrivalPending; }

	/** Quantos estagiarios o elevador esta trazendo (para "aguardando equipe 2/3"). */
	int32 GetExpectedArrivals() const { return ExpectedArrivals; }

	// ---------- Apagao e salas que mudam ----------
	/** A energia caiu (as luzes do nivel apagam; a equipe precisa religar o gerador). */
	bool IsPowerOut() const { return bPowerOut; }
	/** Tempo de servidor da ultima troca de energia (as lampadas piscam no primeiro segundo e meio). */
	float GetPowerChangeTime() const { return static_cast<float>(PowerChangeServerTime); }
	/** Estado das divisorias que mudam (bit por divisoria). */
	uint32 GetLayoutShiftMask() const { return LayoutShiftMask; }

	/** Servidor. */
	void SetPowerOut(bool bOut);
	void SetLayoutShiftMask(uint32 Mask);

	// ---------- Almoxarifado ----------
	/** Quantos de cada item do almoxarifado a equipe tem (indice = NoclipShop::Items()). */
	int32 GetOwnedCount(int32 ItemIndex) const { return ShopOwned.IsValidIndex(ItemIndex) ? ShopOwned[ItemIndex] : 0; }

	// ---------- Escrita (servidor) ----------
	void SetLeader(APlayerState* InLeader);
	void SetRoomInfo(const FString& InRoomName, const FString& InInviteCode, bool bInJoinInProgress);
	void SetRoomExtras(int32 InMaxPlayers, uint8 InPrivacy);
	void SetArrival(bool bPending, int32 InExpected);

	/** Lider = host local (listen server) ou, sem host local, o primeiro jogador. Chamado no Tick dos GameModes. */
	void RefreshLeader();
	void AddOwned(int32 ItemIndex);
	void ResetOwned();
	void SetPhase(ENoclipShiftPhase NewPhase, float DurationSeconds);
	void SetShiftInfo(int32 InShiftIndex, int32 InQuota);
	void SetExtractionStatus(int32 InValue, float InProgress, int32 InPlayers);
	void SetMoney(int32 InMoney);
	void PublishResult(const FNoclipShiftResult& Result);

protected:
	UFUNCTION()
	void OnRep_LevelSeed();

	UFUNCTION()
	void OnRep_Phase(ENoclipShiftPhase OldPhase);

	UFUNCTION()
	void OnRep_LastResult();

	UFUNCTION()
	void OnRep_Economy();

	UFUNCTION()
	void OnRep_LayoutShift();

	UPROPERTY(Replicated)
	bool bPowerOut = false;

	UPROPERTY(Replicated)
	double PowerChangeServerTime = -100.0;

	UPROPERTY(ReplicatedUsing = OnRep_LayoutShift)
	uint32 LayoutShiftMask = 0;

	UPROPERTY(ReplicatedUsing = OnRep_LevelSeed)
	int32 LevelSeed = 0;

	UPROPERTY(Replicated)
	bool bHasLevelSeed = false;

	UPROPERTY(ReplicatedUsing = OnRep_Phase)
	ENoclipShiftPhase Phase = ENoclipShiftPhase::Lobby;

	/** Tempo de servidor (GetServerWorldTimeSeconds) em que a fase atual termina. 0 = sem prazo. */
	UPROPERTY(Replicated)
	double PhaseEndServerTime = 0.0;

	/** Duracao da fase atual (para a barra do colapso e as luzes desligando). */
	UPROPERTY(Replicated)
	float PhaseDurationSeconds = 0.f;

	UPROPERTY(ReplicatedUsing = OnRep_Economy)
	int32 ShiftIndex = 1;

	UPROPERTY(ReplicatedUsing = OnRep_Economy)
	int32 Quota = 0;

	UPROPERTY(ReplicatedUsing = OnRep_Economy)
	int32 Money = 0;

	UPROPERTY(Replicated)
	int32 ValueInExtraction = 0;

	UPROPERTY(Replicated)
	float ExtractionProgress = 0.f;

	UPROPERTY(Replicated)
	int32 PlayersInExtraction = 0;

	UPROPERTY(ReplicatedUsing = OnRep_LastResult)
	FNoclipShiftResult LastResult;

	UPROPERTY(Replicated)
	TObjectPtr<APlayerState> Leader;

	UPROPERTY(Replicated)
	FString RoomName;

	UPROPERTY(Replicated)
	FString InviteCode;

	UPROPERTY(Replicated)
	bool bJoinInProgress = true;

	UPROPERTY(ReplicatedUsing = OnRep_Economy)
	TArray<int32> ShopOwned;

	UPROPERTY(Replicated)
	int32 MaxPlayers = 4;

	UPROPERTY(Replicated)
	uint8 RoomPrivacy = 0;

	UPROPERTY(Replicated)
	bool bArrivalPending = false;

	UPROPERTY(Replicated)
	int32 ExpectedArrivals = 0;
};
