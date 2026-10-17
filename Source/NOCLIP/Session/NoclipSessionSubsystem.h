// Copyright Fuly Games. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Interfaces/OnlineSessionInterface.h"
#include "OnlineSessionSettings.h"
#include "Engine/EngineBaseTypes.h"
#include "NoclipSessionSubsystem.generated.h"

USTRUCT(BlueprintType)
struct FNoclipSessionInfo
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Noclip|Session")
	int32 Index = INDEX_NONE;

	UPROPERTY(BlueprintReadOnly, Category = "Noclip|Session")
	FString OwnerName;

	UPROPERTY(BlueprintReadOnly, Category = "Noclip|Session")
	int32 CurrentPlayers = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Noclip|Session")
	int32 MaxPlayers = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Noclip|Session")
	int32 PingMs = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Noclip|Session")
	FString RoomName;

	UPROPERTY(BlueprintReadOnly, Category = "Noclip|Session")
	FString InviteCode;
};

UENUM(BlueprintType)
enum class ENoclipRoomPrivacy : uint8
{
	FriendsOnly,
	InviteOnly,
	Public
};

/** Configuracao da sala (tela Hospedar). */
USTRUCT(BlueprintType)
struct FNoclipHostSettings
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "Noclip|Session")
	FString RoomName;

	UPROPERTY(BlueprintReadWrite, Category = "Noclip|Session")
	ENoclipRoomPrivacy Privacy = ENoclipRoomPrivacy::FriendsOnly;

	UPROPERTY(BlueprintReadWrite, Category = "Noclip|Session")
	int32 MaxPlayers = 4;

	UPROPERTY(BlueprintReadWrite, Category = "Noclip|Session")
	bool bJoinInProgress = true;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FNoclipSessionsFound, const TArray<FNoclipSessionInfo>&, Sessions);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FNoclipSessionResult, bool, bSuccess);

/**
 * Sessoes via Online Subsystem (Steam no AppID 480 durante o desenvolvimento, NULL no PIE).
 * Como o 480 e compartilhado, toda sessao grava a chave NOCLIP_BUILD e a busca filtra por ela.
 */
UCLASS(Config = Game)
class NOCLIP_API UNoclipSessionSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	/** Cria a sessao e viaja para o mapa como listen server. Mapa vazio = DefaultTravelMap. */
	UFUNCTION(BlueprintCallable, Category = "Noclip|Session")
	void HostSession(int32 MaxPlayers = 6, const FString& MapPath = TEXT(""));

	/** Cria a sala com nome, privacidade, vagas e entrada no meio do turno. */
	void HostRoom(const FNoclipHostSettings& Settings);

	UFUNCTION(BlueprintCallable, Category = "Noclip|Session")
	void FindSessions();

	/** Procura e entra na sala com este codigo de convite (ex.: K7Q-2MX). */
	void JoinByCode(const FString& Code);

	/** Nome sugerido para a sala ("Expediente de <nome>"). */
	FString GetDefaultRoomName() const;

	/** Nome do jogador local no Online Subsystem (Steam) ou do computador. */
	FString GetLocalPlayerName() const;

	UFUNCTION(BlueprintCallable, Category = "Noclip|Session")
	void JoinSessionByIndex(int32 Index);

	UFUNCTION(BlueprintCallable, Category = "Noclip|Session")
	void LeaveSession();

	UFUNCTION(BlueprintPure, Category = "Noclip|Session")
	FString GetOnlineSubsystemName() const;

	/** Sai da sessao e volta para o menu principal (host fecha a sala para todos). */
	void ReturnToMainMenu(APlayerController* PC);

	/** A ultima busca falhou (rede), diferente de "nenhuma sala encontrada". */
	bool DidLastFindFail() const { return bLastFindFailed; }

	/** Mensagem de erro pendente (conexao perdida, viagem falhou...) para o menu mostrar uma vez. */
	FText ConsumePendingError();

	/** Host saiu da sala de espera por "Voltar as opcoes da sala": o menu reabre a tela Hospedar (uma vez). */
	void RequestReopenHostMenu() { bReopenHostMenu = true; }
	bool ConsumeReopenHostMenu() { const bool b = bReopenHostMenu; bReopenHostMenu = false; return b; }

	/** Ultima configuracao usada em Hospedar (para reabrir a tela com os mesmos valores). */
	const FNoclipHostSettings& GetLastHostSettings() const { return PendingHost; }

	UPROPERTY(BlueprintAssignable, Category = "Noclip|Session")
	FNoclipSessionsFound OnSessionsFound;

	UPROPERTY(BlueprintAssignable, Category = "Noclip|Session")
	FNoclipSessionResult OnHostFinished;

	UPROPERTY(BlueprintAssignable, Category = "Noclip|Session")
	FNoclipSessionResult OnJoinFinished;

	/** Chave gravada na sessao e exigida na busca. Vem da ProjectVersion (DefaultGame.ini); este valor so vale sem ela. */
	UPROPERTY(Config, EditAnywhere, Category = "Noclip|Session")
	FString BuildKey = TEXT("NOCLIP_0.1");

	UPROPERTY(Config, EditAnywhere, Category = "Noclip|Session")
	FString DefaultTravelMap = TEXT("/Game/NOCLIP/Maps/L_Level0_Test");

	UPROPERTY(Config, EditAnywhere, Category = "Noclip|Session")
	FString MainMenuMap = TEXT("/Game/NOCLIP/Maps/L_MainMenu");

	/**
	 * Mapa da sala de espera depois de hospedar. O GameMode vai na URL (?game=) e a sala e montada pelo
	 * ANoclipWaitingRoom (criado sozinho se o mapa nao tiver um), entao o proprio mapa do menu serve.
	 * Para um mapa dedicado (L_WaitingRoom), mude aqui ou em [/Script/NOCLIP.NoclipSessionSubsystem] no DefaultGame.ini.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Noclip|Session")
	FString WaitingRoomMap = TEXT("/Game/NOCLIP/Maps/L_MainMenu");

	UPROPERTY(Config, EditAnywhere, Category = "Noclip|Session")
	FString WaitingRoomGameMode = TEXT("/Script/NOCLIP.NoclipWaitingRoomGameMode");

private:
	/** Interface de sessao do mundo atual (no PIE cada instancia tem o seu Online Subsystem). */
	IOnlineSessionPtr GetSessionInterface();
	void HandleNetworkFailure(UWorld* World, class UNetDriver* NetDriver, ENetworkFailure::Type FailureType, const FString& ErrorString);
	void HandleTravelFailure(UWorld* World, ETravelFailure::Type FailureType, const FString& ErrorString);
	bool IsLAN() const;
	void CreateSessionNow();

	void HandleCreateComplete(FName SessionName, bool bWasSuccessful);
	void HandleDestroyComplete(FName SessionName, bool bWasSuccessful);
	void HandleFindComplete(bool bWasSuccessful);
	void HandleJoinComplete(FName SessionName, EOnJoinSessionCompleteResult::Type Result);

	TSharedPtr<FOnlineSessionSearch> LastSearch;
	TArray<int32> FilteredResultIndices;

	int32 PendingMaxPlayers = 6;
	FString PendingMap;
	bool bCreateAfterDestroy = false;

	FDelegateHandle CreateHandle;
	FDelegateHandle DestroyHandle;
	FDelegateHandle FindHandle;
	FDelegateHandle JoinHandle;
	FDelegateHandle NetFailureHandle;
	FDelegateHandle TravelFailureHandle;

	/** Interface em que os delegates acima estao registrados. */
	TWeakPtr<IOnlineSession, ESPMode::ThreadSafe> BoundSessions;

	FText PendingError;
	bool bLastFindFailed = false;

	FNoclipHostSettings PendingHost;
	bool bHostToWaitingRoom = false;
	bool bReopenHostMenu = false;
	FString PendingInviteCode;
	FString PendingJoinCode;
	FDelegateHandle InviteHandle;

	void HandleInviteAccepted(const bool bWasSuccessful, const int32 ControllerId, FUniqueNetIdPtr UserId, const FOnlineSessionSearchResult& InviteResult);
	void JoinResult(FOnlineSessionSearchResult& Result);
	/** Entra agora, ou (se ainda houver uma sessao antiga) destroi ela e entra quando a Steam confirmar. */
	void JoinOrQueue(const FOnlineSessionSearchResult& Result);
	/** Sessao a entrar depois que a antiga terminar de ser destruida (destruir e assincrono na Steam). */
	TSharedPtr<FOnlineSessionSearchResult> PendingJoin;
	static FString MakeInviteCode();
	static FString SanitizeOption(const FString& In);
};
