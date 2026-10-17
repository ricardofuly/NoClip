// Copyright Fuly Games. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "Game/NoclipShiftTypes.h"
#include "NoclipGameMode.generated.h"

class ANoclipGameState;
class ANoclipLevelGenerator;
class ANoclipElevator;
class ANoclipEnemyDirector;
class ANoclipCharacter;
class ANoclipPowerGenerator;

/**
 * GameMode do expediente. Tudo acontece no mesmo mapa:
 * Lobby -> (host inicia) -> Preparando -> Ativo -> Colapso -> Encerrado -> Lobby (ou demissao).
 * Entre turnos a seed troca e o nivel se regenera em todas as maquinas.
 */
UCLASS()
class NOCLIP_API ANoclipGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	ANoclipGameMode();

	virtual void InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage) override;
	virtual void InitGameState() override;
	virtual void StartPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void PreLogin(const FString& Options, const FString& Address, const FUniqueNetIdRepl& UniqueId, FString& ErrorMessage) override;

	/** Todo mundo nasce dentro do elevador (um lugar por jogador). */
	virtual void RestartPlayer(AController* NewPlayer) override;

	ANoclipElevator* GetElevator() const { return Elevator; }

	/** Diretor das entidades (criado na primeira vez que alguem pede). */
	ANoclipEnemyDirector* GetEnemyDirector();

	/** Host pede para comecar o proximo turno (so vale no Lobby). */
	UFUNCTION(BlueprintCallable, Category = "Noclip|Shift")
	bool RequestStartShift(AController* Requester);

	/** Lider pula a contagem do relatorio e volta ao saguao agora ("Ir para o saguao agora" / "Novo estagio"). */
	bool RequestSkipToLobby(AController* Requester);

	/** Compra no almoxarifado (so no saguao, com o caixa da equipe). */
	bool RequestBuyItem(AController* Requester, int32 ItemIndex);

	/** Marca/desmarca pronto no saguao. */
	void SetPlayerReady(AController* Requester, bool bReady);

	/** Vida de um estagiario zerou no turno: larga tudo, vira corpo e o jogador passa a assistir aos colegas. */
	void NotifyPlayerDied(ANoclipCharacter* Pawn, ENoclipDeathCause Cause, const FVector& HitFrom);

	/** Segundos entre a equipe inteira morrer e o turno acabar (da tempo de ver o ultimo cair). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Noclip|Shift", meta = (ClampMin = "0"))
	float TeamWipeDelay = 4.f;

	/** O gerador chegou a 100%: a energia volta. */
	void RestorePower(ANoclipPowerGenerator* Generator);

	// ---------- Apagao / salas que mudam (ajustes) ----------
	/** Primeiro apagao do turno: entre Min e Max segundos depois de comecar. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Noclip|Power")
	FVector2D FirstBlackoutDelay = FVector2D(110.f, 190.f);

	/** Intervalo entre a energia voltar e o proximo apagao. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Noclip|Power")
	FVector2D BlackoutInterval = FVector2D(150.f, 260.f);

	/** Intervalo entre mudancas das divisorias. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Noclip|Shifting")
	FVector2D RoomShiftInterval = FVector2D(40.f, 70.f);

	// Depuracao (console: noclip.Blackout, noclip.ShiftRooms, noclip.GoDown)
	void DebugBlackout();
	void DebugShiftRooms();
	void DebugNoclip(APlayerController* PC, bool bDown);

	/** Chamado pela Sala Manila quando o elevador sobe. */
	void NotifyExtractionComplete();

	/** Quota do turno N (1 = primeiro). */
	UFUNCTION(BlueprintPure, Category = "Noclip|Shift")
	int32 GetQuotaForShift(int32 ShiftIndex) const;

	// ---------- Ajustes ----------
	/** Se diferente de 0, sempre usa esta seed (util para depurar). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Noclip")
	int32 FixedSeed = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Noclip|Shift", meta = (ClampMin = "30"))
	float ShiftDurationSeconds = 780.f;

	/** Os ultimos N segundos do turno sao o colapso. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Noclip|Shift", meta = (ClampMin = "0"))
	float CollapseSeconds = 60.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Noclip|Shift", meta = (ClampMin = "0"))
	float PrepareSeconds = 5.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Noclip|Shift", meta = (ClampMin = "1"))
	float ResultSeconds = 12.f;

	/** Quotas dos primeiros turnos; depois do ultimo, cresce QuotaGrowthAfterSchedule por turno. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Noclip|Shift")
	TArray<int32> QuotaSchedule = { 300, 450, 700, 1000, 1400 };

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Noclip|Shift", meta = (ClampMin = "0"))
	float QuotaGrowthAfterSchedule = 0.4f;

	/** Bonus fixo por bater a quota. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Noclip|Shift", meta = (ClampMin = "0"))
	int32 QuotaMetBonus = 50;

	/** Comeca o primeiro turno sozinho (testes). Com a HUD, o host segura Enter / Y / Triangulo no saguao. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Noclip|Shift")
	bool bAutoStartFirstShift = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Noclip|Shift", meta = (ClampMin = "0"))
	float AutoStartDelay = 4.f;

	/** Vindo da sala de espera: espera no maximo isso pela equipe antes de comecar o turno 1 assim mesmo. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Noclip|Elevator", meta = (ClampMin = "1"))
	float ArrivalTimeoutSeconds = 15.f;

	/** Pausa depois que todos chegaram, antes de a contagem do turno comecar (o elevador "termina de descer"). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Noclip|Elevator", meta = (ClampMin = "0"))
	float ArrivalSettleSeconds = 1.5f;

	// ---------- Depuracao (marque no Details do GameMode durante o PIE) ----------
	/** Leva todo mundo e ate 6 achados para dentro da Sala Manila (testar a extracao). */
	UPROPERTY(EditAnywhere, Transient, Category = "Noclip|Debug")
	bool bDebugExtractNow = false;

	/** Pula direto para o colapso (testar luzes desligando e quem fica para tras). */
	UPROPERTY(EditAnywhere, Transient, Category = "Noclip|Debug")
	bool bDebugCollapseNow = false;

	/** Onde fica a porta do elevador em relacao ao gerador (a cabine se estende para tras, -X). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Noclip|Elevator")
	FVector ElevatorOffset = FVector(150.f, 0.f, 0.f);

protected:
	void EnterPhase(ENoclipShiftPhase NewPhase);
	void BeginShift();
	void EndShift(bool bExtractedEarly);
	void ReturnToLobby();
	void RespawnAllPlayers();

	ANoclipGameState* GetNoclipGameState() const;
	ANoclipLevelGenerator* FindGenerator() const;

private:
	int32 PendingSeed = 0;
	int32 CurrentShift = 1;
	int32 TeamMoney = 0;
	double PhaseEndTime = 0.0;
	ENoclipShiftPhase CurrentPhase = ENoclipShiftPhase::Lobby;
	bool bFirstShiftStarted = false;

	// Sala (vem das opcoes de viagem do host: ?Room=...?Code=...?JIP=0/1)
	FString RoomName;
	FString InviteCode;
	bool bJoinInProgress = true;

	// Chegada pelo elevador (?Arrive=1?Expect=N, vindo da sala de espera)
	bool bArrivalPending = false;
	int32 ExpectedArrivals = 0;
	double ArrivalDeadline = 0.0;
	double ArrivalStartAt = -1.0;
	bool bSkipNextRespawn = false;
	int32 MaxPlayers = 4;
	uint8 Privacy = 0;

	UPROPERTY(Transient)
	TObjectPtr<ANoclipElevator> Elevator;

	UPROPERTY(Transient)
	TObjectPtr<ANoclipEnemyDirector> EnemyDirector;

	void SpawnElevator();
	void DebugExtract();
	/** Fim por tempo: quem esta fora da Sala Manila fica para tras e perde o que carrega (GDD, Colapso). */
	void ApplyLeftBehind(class ANoclipExtractionZone* Zone, FNoclipShiftResult& Result, bool bExtractedEarly);
	void TickArrival();
	/** Lugar do jogador na cabine (ordem de entrada; o host e o 0). */
	int32 GetCabinIndex(const AController* Controller) const;
	/** Com as portas fechadas, devolve para a cabine quem estiver fora dela. */
	void KeepCrewInCabin(const TCHAR* Reason);
	double NextCabinCheckTime = 0.0;

	bool IsLeader(const AController* Controller) const;
	void UpdateTimeAlone(float DeltaSeconds);
	/** Alguem de pe (com pawn) entre os jogadores conectados? */
	bool AnyoneAlive() const;
	void ClearCorpses();
	double WipeEndAt = -1.0;
	bool bWipeEnding = false;

	void StartBlackout();
	void TickGlitches(double Now);
	void TryShiftRooms(bool bForce);
	double NextBlackoutTime = -1.0;
	double NextRoomShiftTime = -1.0;
	TMap<TWeakObjectPtr<AActor>, double> NoclipCooldown;
};
