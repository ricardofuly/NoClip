// Copyright Fuly Games. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "NoclipWaitingRoomGameMode.generated.h"

class ANoclipGameState;
class ANoclipWaitingRoom;

/**
 * Sala de espera depois de hospedar (L_WaitingRoom, ou qualquer mapa com ?game=/Script/NOCLIP.NoclipWaitingRoomGameMode).
 * Sem pawn: cada um ve a sala pela camera de seguranca na UI.
 *
 * Todos prontos (ou o host manda descer) ->
 *   portas abrem -> os bonecos entram no elevador -> portas fecham -> "descendo" -> ServerTravel para o Level 0
 *   com ?Arrive=1, e la a equipe chega dentro de um elevador e o turno 1 comeca sozinho.
 */
UCLASS()
class NOCLIP_API ANoclipWaitingRoomGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	ANoclipWaitingRoomGameMode();

	virtual void InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage) override;
	virtual void InitGameState() override;
	virtual void StartPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void PreLogin(const FString& Options, const FString& Address, const FUniqueNetIdRepl& UniqueId, FString& ErrorMessage) override;
	virtual void Logout(AController* Exiting) override;

	/** Pronto / nao pronto (so enquanto o elevador esta parado). */
	void SetPlayerReady(AController* Requester, bool bReady);

	/** Host: desce agora, sem esperar todo mundo marcar pronto. */
	bool RequestDepart(AController* Requester);

	/** Quantos segundos depois das portas fecharem a viagem acontece. */
	UPROPERTY(EditAnywhere, Category = "Noclip|Waiting")
	float DepartSeconds = 1.8f;

	/** Folga entre o ultimo entrar e as portas fecharem. */
	UPROPERTY(EditAnywhere, Category = "Noclip|Waiting")
	float CloseDelay = 0.5f;

	/** Pausa curta depois do ultimo pronto, antes das portas abrirem (da tempo de desmarcar). */
	UPROPERTY(EditAnywhere, Category = "Noclip|Waiting")
	float AllReadyDelay = 1.2f;

protected:
	virtual void HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer) override;

	void BeginBoarding();
	void TravelToLevel();
	ANoclipWaitingRoom* GetRoom();
	bool AreAllReady() const;

private:
	UPROPERTY(Transient)
	TObjectPtr<ANoclipWaitingRoom> Room;

	FString RoomName;
	FString InviteCode;
	bool bJoinInProgress = true;
	int32 MaxPlayers = 4;
	uint8 Privacy = 0;

	double AllReadySince = -1.0;
	double DoorsCloseAt = -1.0;
	double TravelAt = -1.0;
	bool bTraveling = false;
};
