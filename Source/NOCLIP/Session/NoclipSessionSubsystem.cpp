// Copyright Fuly Games. All Rights Reserved.

#include "Session/NoclipSessionSubsystem.h"
#include "NOCLIP.h"
#include "OnlineSubsystem.h"
#include "OnlineSubsystemUtils.h"
#include "Online/OnlineSessionNames.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Engine/Engine.h"
#include "Interfaces/OnlineIdentityInterface.h"
#include "Kismet/GameplayStatics.h"
#include "GeneralProjectSettings.h"

#define LOCTEXT_NAMESPACE "NoclipSession"

#include UE_INLINE_GENERATED_CPP_BY_NAME(NoclipSessionSubsystem)

static const FName NoclipBuildKeyName(TEXT("NOCLIP_BUILD"));
static const FName NoclipRoomKeyName(TEXT("NOCLIP_ROOM"));
static const FName NoclipCodeKeyName(TEXT("NOCLIP_CODE"));

void UNoclipSessionSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	// Salas so se encontram na mesma versao do jogo (Project Settings > Project Version, que o PublishRelease.bat
	// atualiza). Versoes diferentes nem aparecem na lista: ninguem entra numa sala incompativel.
	if (const UGeneralProjectSettings* Project = GetDefault<UGeneralProjectSettings>(); Project && !Project->ProjectVersion.IsEmpty())
	{
		BuildKey = FString::Printf(TEXT("NOCLIP_%s"), *Project->ProjectVersion);
	}

	if (GEngine)
	{
		NetFailureHandle = GEngine->OnNetworkFailure().AddUObject(this, &UNoclipSessionSubsystem::HandleNetworkFailure);
		TravelFailureHandle = GEngine->OnTravelFailure().AddUObject(this, &UNoclipSessionSubsystem::HandleTravelFailure);
	}
}

void UNoclipSessionSubsystem::Deinitialize()
{
	if (GEngine)
	{
		GEngine->OnNetworkFailure().Remove(NetFailureHandle);
		GEngine->OnTravelFailure().Remove(TravelFailureHandle);
	}
	if (IOnlineSessionPtr Sessions = BoundSessions.Pin())
	{
		Sessions->ClearOnCreateSessionCompleteDelegate_Handle(CreateHandle);
		Sessions->ClearOnDestroySessionCompleteDelegate_Handle(DestroyHandle);
		Sessions->ClearOnFindSessionsCompleteDelegate_Handle(FindHandle);
		Sessions->ClearOnJoinSessionCompleteDelegate_Handle(JoinHandle);
	}
	Super::Deinitialize();
}

IOnlineSessionPtr UNoclipSessionSubsystem::GetSessionInterface()
{
	const IOnlineSubsystem* OSS = Online::GetSubsystem(GetWorld());
	IOnlineSessionPtr Sessions = OSS ? OSS->GetSessionInterface() : nullptr;

	// Registra os delegates na interface deste mundo (uma vez por interface).
	if (Sessions && BoundSessions.Pin() != Sessions)
	{
		if (IOnlineSessionPtr Old = BoundSessions.Pin())
		{
			Old->ClearOnCreateSessionCompleteDelegate_Handle(CreateHandle);
			Old->ClearOnDestroySessionCompleteDelegate_Handle(DestroyHandle);
			Old->ClearOnFindSessionsCompleteDelegate_Handle(FindHandle);
			Old->ClearOnJoinSessionCompleteDelegate_Handle(JoinHandle);
			Old->ClearOnSessionUserInviteAcceptedDelegate_Handle(InviteHandle);
		}
		CreateHandle = Sessions->AddOnCreateSessionCompleteDelegate_Handle(FOnCreateSessionCompleteDelegate::CreateUObject(this, &UNoclipSessionSubsystem::HandleCreateComplete));
		DestroyHandle = Sessions->AddOnDestroySessionCompleteDelegate_Handle(FOnDestroySessionCompleteDelegate::CreateUObject(this, &UNoclipSessionSubsystem::HandleDestroyComplete));
		FindHandle = Sessions->AddOnFindSessionsCompleteDelegate_Handle(FOnFindSessionsCompleteDelegate::CreateUObject(this, &UNoclipSessionSubsystem::HandleFindComplete));
		JoinHandle = Sessions->AddOnJoinSessionCompleteDelegate_Handle(FOnJoinSessionCompleteDelegate::CreateUObject(this, &UNoclipSessionSubsystem::HandleJoinComplete));
		InviteHandle = Sessions->AddOnSessionUserInviteAcceptedDelegate_Handle(FOnSessionUserInviteAcceptedDelegate::CreateUObject(this, &UNoclipSessionSubsystem::HandleInviteAccepted));
		BoundSessions = Sessions;
		UE_LOG(LogNoclip, Log, TEXT("Sessoes usando o subsystem: %s"), *GetOnlineSubsystemName());
	}
	return Sessions;
}

FString UNoclipSessionSubsystem::GetOnlineSubsystemName() const
{
	const IOnlineSubsystem* OSS = Online::GetSubsystem(GetWorld());
	return OSS ? OSS->GetSubsystemName().ToString() : TEXT("None");
}

FText UNoclipSessionSubsystem::ConsumePendingError()
{
	FText Error = PendingError;
	PendingError = FText::GetEmpty();
	return Error;
}

void UNoclipSessionSubsystem::HandleNetworkFailure(UWorld* World, UNetDriver* NetDriver, ENetworkFailure::Type FailureType, const FString& ErrorString)
{
	if (World && World->GetGameInstance() != GetGameInstance())
	{
		return;
	}
	UE_LOG(LogNoclip, Warning, TEXT("Falha de rede: %s (%s)"), ENetworkFailure::ToString(FailureType), *ErrorString);
	// A sala acabou para esta maquina: sai da sessao da Steam ja (senao a proxima tentativa de entrar falha).
	LeaveSession();
	PendingError = FailureType == ENetworkFailure::ConnectionLost || FailureType == ENetworkFailure::ConnectionTimeout
		? LOCTEXT("ConnLost", "A conexão com o host caiu. Você voltou ao menu.")
		: LOCTEXT("NetFail", "Erro de rede. Você voltou ao menu.");
}

void UNoclipSessionSubsystem::HandleTravelFailure(UWorld* World, ETravelFailure::Type FailureType, const FString& ErrorString)
{
	if (World && World->GetGameInstance() != GetGameInstance())
	{
		return;
	}
	UE_LOG(LogNoclip, Warning, TEXT("Falha ao viajar: %s (%s)"), ETravelFailure::ToString(FailureType), *ErrorString);
	PendingError = LOCTEXT("TravelFail", "Não foi possível entrar na sala.");
}

void UNoclipSessionSubsystem::ReturnToMainMenu(APlayerController* PC)
{
	LeaveSession();
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	if (World->GetNetMode() == NM_Client && PC)
	{
		PC->ClientTravel(MainMenuMap, TRAVEL_Absolute);
	}
	else
	{
		// Host (ou standalone): abre o menu sem ?listen; os clientes caem e voltam ao menu deles.
		UGameplayStatics::OpenLevel(World, FName(*MainMenuMap), true);
	}
}

bool UNoclipSessionSubsystem::IsLAN() const
{
	return GetOnlineSubsystemName() == TEXT("NULL");
}

FString UNoclipSessionSubsystem::MakeInviteCode()
{
	// Sem letras/numeros ambiguos (0/O, 1/I).
	static const TCHAR* Alphabet = TEXT("ABCDEFGHJKLMNPQRSTUVWXYZ23456789");
	FString Code;
	for (int32 i = 0; i < 6; ++i)
	{
		if (i == 3)
		{
			Code.AppendChar(TEXT('-'));
		}
		Code.AppendChar(Alphabet[FMath::RandRange(0, 31)]);
	}
	return Code;
}

FString UNoclipSessionSubsystem::SanitizeOption(const FString& In)
{
	FString Out;
	for (const TCHAR C : In)
	{
		if (C == TEXT('?') || C == TEXT('=') || C == TEXT('#') || C == TEXT('&') || C == TEXT('/') || C == TEXT('\\'))
		{
			continue;
		}
		Out.AppendChar(C == TEXT(' ') ? TEXT('+') : C);
	}
	return Out.Left(32);
}

FString UNoclipSessionSubsystem::GetLocalPlayerName() const
{
	if (const IOnlineSubsystem* OSS = Online::GetSubsystem(GetWorld()))
	{
		if (IOnlineIdentityPtr Identity = OSS->GetIdentityInterface())
		{
			const FString Nick = Identity->GetPlayerNickname(0);
			if (!Nick.IsEmpty())
			{
				return Nick;
			}
		}
	}
	return FPlatformProcess::ComputerName();
}

FString UNoclipSessionSubsystem::GetDefaultRoomName() const
{
	return FString::Printf(TEXT("Expediente de %s"), *GetLocalPlayerName()).Left(32);
}

void UNoclipSessionSubsystem::HostRoom(const FNoclipHostSettings& Settings)
{
	PendingHost = Settings;
	if (PendingHost.RoomName.TrimStartAndEnd().IsEmpty())
	{
		PendingHost.RoomName = GetDefaultRoomName();
	}
	PendingInviteCode = MakeInviteCode();
	// A sala vai para a sala de espera, nao direto ao Level 0 (o OSS NULL completa a criacao na mesma chamada).
	bHostToWaitingRoom = !WaitingRoomMap.IsEmpty();
	HostSession(Settings.MaxPlayers, FString());
}

void UNoclipSessionSubsystem::HostSession(int32 MaxPlayers, const FString& MapPath)
{
	IOnlineSessionPtr Sessions = GetSessionInterface();
	if (!Sessions)
	{
		UE_LOG(LogNoclip, Error, TEXT("HostSession: sem interface de sessao."));
		OnHostFinished.Broadcast(false);
		return;
	}

	PendingMaxPlayers = FMath::Clamp(MaxPlayers, 1, 6);
	if (PendingInviteCode.IsEmpty())
	{
		PendingInviteCode = MakeInviteCode();
	}
	if (PendingHost.RoomName.IsEmpty())
	{
		PendingHost.RoomName = GetDefaultRoomName();
	}
	PendingMap = MapPath.IsEmpty() ? DefaultTravelMap : MapPath;
	if (!MapPath.IsEmpty())
	{
		bHostToWaitingRoom = false;
	}

	if (Sessions->GetNamedSession(NAME_GameSession))
	{
		bCreateAfterDestroy = true;
		Sessions->DestroySession(NAME_GameSession);
		return;
	}

	CreateSessionNow();
}

void UNoclipSessionSubsystem::CreateSessionNow()
{
	IOnlineSessionPtr Sessions = GetSessionInterface();
	const ULocalPlayer* LocalPlayer = GetGameInstance()->GetFirstGamePlayer();
	if (!Sessions || !LocalPlayer)
	{
		OnHostFinished.Broadcast(false);
		return;
	}

	FOnlineSessionSettings Settings;
	Settings.bIsLANMatch = IsLAN();
	Settings.NumPublicConnections = PendingMaxPlayers;
	Settings.NumPrivateConnections = 0;
	Settings.bShouldAdvertise = PendingHost.Privacy != ENoclipRoomPrivacy::InviteOnly;
	Settings.bAllowJoinInProgress = PendingHost.bJoinInProgress;
	Settings.bAllowJoinViaPresence = true;
	Settings.bAllowJoinViaPresenceFriendsOnly = PendingHost.Privacy == ENoclipRoomPrivacy::FriendsOnly;
	Settings.bAllowInvites = true;
	Settings.bUsesPresence = true;
	Settings.bUseLobbiesIfAvailable = true;
	Settings.Set(NoclipBuildKeyName, BuildKey, EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);
	Settings.Set(NoclipRoomKeyName, PendingHost.RoomName, EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);
	Settings.Set(NoclipCodeKeyName, PendingInviteCode, EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);

	UE_LOG(LogNoclip, Log, TEXT("Criando sessao (%s, %d jogadores, chave %s)"), *GetOnlineSubsystemName(), PendingMaxPlayers, *BuildKey);
	if (!Sessions->CreateSession(*LocalPlayer->GetPreferredUniqueNetId(), NAME_GameSession, Settings))
	{
		OnHostFinished.Broadcast(false);
	}
}

void UNoclipSessionSubsystem::HandleDestroyComplete(FName SessionName, bool bWasSuccessful)
{
	if (bCreateAfterDestroy)
	{
		bCreateAfterDestroy = false;
		CreateSessionNow();
	}
	if (PendingJoin.IsValid())
	{
		const TSharedPtr<FOnlineSessionSearchResult> Join = PendingJoin;
		PendingJoin.Reset();
		JoinOrQueue(*Join);
	}
}

void UNoclipSessionSubsystem::JoinOrQueue(const FOnlineSessionSearchResult& InResult)
{
	IOnlineSessionPtr Sessions = GetSessionInterface();
	const ULocalPlayer* LocalPlayer = GetGameInstance()->GetFirstGamePlayer();
	if (!Sessions || !LocalPlayer)
	{
		OnJoinFinished.Broadcast(false);
		return;
	}
	FOnlineSessionSearchResult Result = InResult;
	// Exigido pelo Steam no UE5: precisa bater com as flags de quem criou.
	Result.Session.SessionSettings.bUseLobbiesIfAvailable = true;
	Result.Session.SessionSettings.bUsesPresence = true;

	// Sobrou a sessao de antes (hospedou e voltou, ou a rede caiu): a Steam recusa entrar com ela aberta
	// ("Session already exists, can't join twice"). Destroi e entra quando a Steam confirmar.
	if (Sessions->GetNamedSession(NAME_GameSession))
	{
		UE_LOG(LogNoclip, Log, TEXT("Saindo da sessao antiga antes de entrar na nova..."));
		PendingJoin = MakeShared<FOnlineSessionSearchResult>(Result);
		if (!Sessions->DestroySession(NAME_GameSession))
		{
			PendingJoin.Reset();
			Sessions->JoinSession(*LocalPlayer->GetPreferredUniqueNetId(), NAME_GameSession, Result);
		}
		return;
	}
	Sessions->JoinSession(*LocalPlayer->GetPreferredUniqueNetId(), NAME_GameSession, Result);
}

void UNoclipSessionSubsystem::HandleCreateComplete(FName SessionName, bool bWasSuccessful)
{
	UE_LOG(LogNoclip, Log, TEXT("Sessao criada: %s"), bWasSuccessful ? TEXT("ok") : TEXT("FALHOU"));
	OnHostFinished.Broadcast(bWasSuccessful);

	if (!bWasSuccessful)
	{
		bHostToWaitingRoom = false;
	}
	if (bWasSuccessful)
	{
		const FString Options = FString::Printf(TEXT("?listen?Room=%s?Code=%s?JIP=%d?Max=%d?Priv=%d"), *SanitizeOption(PendingHost.RoomName), *PendingInviteCode,
			PendingHost.bJoinInProgress ? 1 : 0, PendingMaxPlayers, static_cast<int32>(PendingHost.Privacy));
		if (bHostToWaitingRoom)
		{
			bHostToWaitingRoom = false;
			GetWorld()->ServerTravel(WaitingRoomMap + Options + TEXT("?game=") + WaitingRoomGameMode);
		}
		else
		{
			GetWorld()->ServerTravel(PendingMap + Options);
		}
	}
}

void UNoclipSessionSubsystem::FindSessions()
{
	IOnlineSessionPtr Sessions = GetSessionInterface();
	const ULocalPlayer* LocalPlayer = GetGameInstance()->GetFirstGamePlayer();
	if (!Sessions || !LocalPlayer)
	{
		bLastFindFailed = true;
		OnSessionsFound.Broadcast({});
		return;
	}

	LastSearch = MakeShared<FOnlineSessionSearch>();
	LastSearch->bIsLanQuery = IsLAN();
	LastSearch->MaxSearchResults = 10000; // o AppID 480 tem muitas sessoes de outros jogos
	LastSearch->QuerySettings.Set(SEARCH_LOBBIES, true, EOnlineComparisonOp::Equals);
	if (!LastSearch->bIsLanQuery)
	{
		// Filtro na propria Steam: no AppID 480 ha milhares de lobbies de outros jogos e a Steam devolve so ~50
		// por busca. Sem isto as salas do NOCLIP quase nunca apareciam (e o codigo da sala nao achava ninguem).
		LastSearch->QuerySettings.Set(NoclipBuildKeyName, BuildKey, EOnlineComparisonOp::Equals);
	}

	UE_LOG(LogNoclip, Log, TEXT("Procurando sessoes (%s)..."), *GetOnlineSubsystemName());
	Sessions->FindSessions(*LocalPlayer->GetPreferredUniqueNetId(), LastSearch.ToSharedRef());
}

void UNoclipSessionSubsystem::HandleFindComplete(bool bWasSuccessful)
{
	TArray<FNoclipSessionInfo> Found;
	FilteredResultIndices.Reset();
	bLastFindFailed = !bWasSuccessful;

	if (bWasSuccessful && LastSearch.IsValid())
	{
		for (int32 i = 0; i < LastSearch->SearchResults.Num(); ++i)
		{
			const FOnlineSessionSearchResult& Result = LastSearch->SearchResults[i];
			FString Key;
			if (!Result.Session.SessionSettings.Get(NoclipBuildKeyName, Key) || Key != BuildKey)
			{
				continue;
			}

			FNoclipSessionInfo Info;
			Info.Index = FilteredResultIndices.Num();
			Info.OwnerName = Result.Session.OwningUserName;
			Info.MaxPlayers = Result.Session.SessionSettings.NumPublicConnections;
			Info.CurrentPlayers = Info.MaxPlayers - Result.Session.NumOpenPublicConnections;
			Info.PingMs = Result.PingInMs;
			Result.Session.SessionSettings.Get(NoclipRoomKeyName, Info.RoomName);
			Result.Session.SessionSettings.Get(NoclipCodeKeyName, Info.InviteCode);
			if (Info.RoomName.IsEmpty())
			{
				Info.RoomName = FString::Printf(TEXT("Expediente de %s"), *Info.OwnerName);
			}
			Found.Add(Info);
			FilteredResultIndices.Add(i);

			UE_LOG(LogNoclip, Log, TEXT("  [%d] %s  %d/%d  %d ms"), Info.Index, *Info.OwnerName, Info.CurrentPlayers, Info.MaxPlayers, Info.PingMs);
		}
	}

	UE_LOG(LogNoclip, Log, TEXT("Busca terminou: %d sessoes do NOCLIP (de %d no total)."), Found.Num(), LastSearch.IsValid() ? LastSearch->SearchResults.Num() : 0);

	// Entrar por codigo: procura a sala com o codigo e entra direto.
	if (!PendingJoinCode.IsEmpty())
	{
		const FString Code = PendingJoinCode;
		PendingJoinCode.Reset();
		for (const FNoclipSessionInfo& Info : Found)
		{
			if (Info.InviteCode.Equals(Code, ESearchCase::IgnoreCase))
			{
				JoinSessionByIndex(Info.Index);
				OnSessionsFound.Broadcast(Found);
				return;
			}
		}
		PendingError = FText::Format(NSLOCTEXT("NoclipSession", "CodeNotFound", "Nenhuma sala aberta com o código {0}."), FText::FromString(Code));
		OnJoinFinished.Broadcast(false);
	}
	OnSessionsFound.Broadcast(Found);
}

void UNoclipSessionSubsystem::JoinByCode(const FString& Code)
{
	FString Clean = Code.TrimStartAndEnd().ToUpper();
	if (Clean.Len() == 6 && !Clean.Contains(TEXT("-")))
	{
		Clean.InsertAt(3, TEXT('-'));
	}
	PendingJoinCode = Clean;
	FindSessions();
}

void UNoclipSessionSubsystem::HandleInviteAccepted(const bool bWasSuccessful, const int32 ControllerId, FUniqueNetIdPtr UserId, const FOnlineSessionSearchResult& InviteResult)
{
	UE_LOG(LogNoclip, Log, TEXT("Convite aceito pelo overlay (ok=%d)."), bWasSuccessful ? 1 : 0);
	if (bWasSuccessful && InviteResult.IsValid())
	{
		FOnlineSessionSearchResult Result = InviteResult;
		JoinResult(Result);
	}
}

void UNoclipSessionSubsystem::JoinResult(FOnlineSessionSearchResult& Result)
{
	JoinOrQueue(Result);
}

void UNoclipSessionSubsystem::JoinSessionByIndex(int32 Index)
{
	IOnlineSessionPtr Sessions = GetSessionInterface();
	const ULocalPlayer* LocalPlayer = GetGameInstance()->GetFirstGamePlayer();
	if (!Sessions || !LocalPlayer || !LastSearch.IsValid() || !FilteredResultIndices.IsValidIndex(Index))
	{
		UE_LOG(LogNoclip, Warning, TEXT("JoinSessionByIndex(%d): rode NoclipFind antes."), Index);
		OnJoinFinished.Broadcast(false);
		return;
	}

	JoinOrQueue(LastSearch->SearchResults[FilteredResultIndices[Index]]);
}

void UNoclipSessionSubsystem::HandleJoinComplete(FName SessionName, EOnJoinSessionCompleteResult::Type Result)
{
	const bool bSuccess = Result == EOnJoinSessionCompleteResult::Success;
	UE_LOG(LogNoclip, Log, TEXT("Entrar na sessao: %s (%d)"), bSuccess ? TEXT("ok") : TEXT("FALHOU"), static_cast<int32>(Result));

	if (bSuccess)
	{
		IOnlineSessionPtr Sessions = GetSessionInterface();
		FString ConnectString;
		if (Sessions && Sessions->GetResolvedConnectString(SessionName, ConnectString))
		{
			if (APlayerController* PC = GetGameInstance()->GetFirstLocalPlayerController())
			{
				PC->ClientTravel(ConnectString, TRAVEL_Absolute);
			}
		}
	}

	OnJoinFinished.Broadcast(bSuccess);
}

void UNoclipSessionSubsystem::LeaveSession()
{
	if (IOnlineSessionPtr Sessions = GetSessionInterface())
	{
		Sessions->DestroySession(NAME_GameSession);
	}
}

#undef LOCTEXT_NAMESPACE
