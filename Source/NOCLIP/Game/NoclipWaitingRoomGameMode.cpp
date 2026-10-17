// Copyright Fuly Games. All Rights Reserved.

#include "Game/NoclipWaitingRoomGameMode.h"
#include "Game/NoclipGameState.h"
#include "Game/NoclipPlayerState.h"
#include "Game/NoclipWaitingPlayerController.h"
#include "Session/NoclipSessionSubsystem.h"
#include "World/NoclipWaitingRoom.h"
#include "World/NoclipElevator.h"
#include "NOCLIP.h"
#include "EngineUtils.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/HUD.h"
#include "Kismet/GameplayStatics.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(NoclipWaitingRoomGameMode)

namespace
{
	FString TravelOption(const FString& In)
	{
		FString Out = In;
		Out.ReplaceInline(TEXT("?"), TEXT(""));
		Out.ReplaceInline(TEXT("="), TEXT(""));
		Out.ReplaceInline(TEXT("#"), TEXT(""));
		Out.ReplaceInline(TEXT(" "), TEXT("+"));
		return Out;
	}
}

ANoclipWaitingRoomGameMode::ANoclipWaitingRoomGameMode()
{
	DefaultPawnClass = nullptr;
	PlayerControllerClass = ANoclipWaitingPlayerController::StaticClass();
	PlayerStateClass = ANoclipPlayerState::StaticClass();
	GameStateClass = ANoclipGameState::StaticClass();
	HUDClass = AHUD::StaticClass();
	// Seamless: a conexao (e o driver de rede da Steam) continua viva na descida para o Level 0.
	// Sem isso o host fechava a conexao de todo mundo, o cliente tentava reconectar antes de o mapa novo
	// carregar ("Server L_MainMenu refused") e o socket da Steam nem voltava a escutar (NetDriverListenFailure).
	// O PlayerController troca de classe sozinho (sala de espera -> expediente) e o PlayerState passa pelo CopyProperties.
	bUseSeamlessTravel = true;

	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 0.05f;
}

void ANoclipWaitingRoomGameMode::InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage)
{
	Super::InitGame(MapName, Options, ErrorMessage);

	RoomName = UGameplayStatics::ParseOption(Options, TEXT("Room"));
	RoomName.ReplaceInline(TEXT("+"), TEXT(" "));
	if (RoomName.IsEmpty())
	{
		RoomName = TEXT("Expediente");
	}
	InviteCode = UGameplayStatics::ParseOption(Options, TEXT("Code"));
	bJoinInProgress = !UGameplayStatics::HasOption(Options, TEXT("JIP")) || UGameplayStatics::GetIntOption(Options, TEXT("JIP"), 1) != 0;
	MaxPlayers = FMath::Clamp(UGameplayStatics::GetIntOption(Options, TEXT("Max"), 4), 1, 6);
	Privacy = static_cast<uint8>(FMath::Clamp(UGameplayStatics::GetIntOption(Options, TEXT("Priv"), 0), 0, 2));
}

void ANoclipWaitingRoomGameMode::InitGameState()
{
	Super::InitGameState();
	if (ANoclipGameState* GS = GetGameState<ANoclipGameState>())
	{
		GS->SetRoomInfo(RoomName, InviteCode, bJoinInProgress);
		GS->SetRoomExtras(MaxPlayers, Privacy);
	}
}

void ANoclipWaitingRoomGameMode::StartPlay()
{
	Super::StartPlay();
	GetRoom();
}

ANoclipWaitingRoom* ANoclipWaitingRoomGameMode::GetRoom()
{
	if (IsValid(Room))
	{
		return Room;
	}
	for (TActorIterator<ANoclipWaitingRoom> It(GetWorld()); It; ++It)
	{
		Room = *It;
		return Room;
	}
	// O mapa nao tem sala montada: cria uma longe de qualquer cenario que o mapa tenha.
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	Room = GetWorld()->SpawnActor<ANoclipWaitingRoom>(ANoclipWaitingRoom::StaticClass(), FTransform(FVector(0.f, 0.f, -20000.f)), Params);
	return Room;
}

void ANoclipWaitingRoomGameMode::PreLogin(const FString& Options, const FString& Address, const FUniqueNetIdRepl& UniqueId, FString& ErrorMessage)
{
	Super::PreLogin(Options, Address, UniqueId, ErrorMessage);
	if (!ErrorMessage.IsEmpty())
	{
		return;
	}
	const ANoclipWaitingRoom* R = IsValid(Room) ? Room.Get() : nullptr;
	if (bTraveling || (R && R->GetPhase() != ENoclipWaitingPhase::Gathering))
	{
		ErrorMessage = TEXT("O elevador já desceu. Espere o próximo turno ou peça um novo convite.");
	}
	else if (GetNumPlayers() >= MaxPlayers)
	{
		ErrorMessage = TEXT("A sala está cheia.");
	}
}

void ANoclipWaitingRoomGameMode::HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer)
{
	// Sem pawn: o estagiario aparece como boneco na camera de seguranca.
	if (ANoclipWaitingRoom* R = GetRoom())
	{
		R->AddAvatar(NewPlayer ? NewPlayer->PlayerState : nullptr);
	}
	if (ANoclipPlayerState* PS = NewPlayer ? NewPlayer->GetPlayerState<ANoclipPlayerState>() : nullptr)
	{
		PS->SetReady(false);
	}
	AllReadySince = -1.0;
}

void ANoclipWaitingRoomGameMode::Logout(AController* Exiting)
{
	if (ANoclipWaitingRoom* R = GetRoom())
	{
		R->RemoveAvatar(Exiting ? Exiting->PlayerState : nullptr);
	}
	Super::Logout(Exiting);
}

void ANoclipWaitingRoomGameMode::SetPlayerReady(AController* Requester, bool bReady)
{
	const ANoclipWaitingRoom* R = GetRoom();
	if (!R || R->GetPhase() != ENoclipWaitingPhase::Gathering)
	{
		return;
	}
	if (ANoclipPlayerState* PS = Requester ? Requester->GetPlayerState<ANoclipPlayerState>() : nullptr)
	{
		PS->SetReady(bReady);
	}
}

bool ANoclipWaitingRoomGameMode::RequestDepart(AController* Requester)
{
	const ANoclipGameState* GS = GetGameState<ANoclipGameState>();
	const ANoclipWaitingRoom* R = GetRoom();
	if (!GS || !R || R->GetPhase() != ENoclipWaitingPhase::Gathering || !Requester || Requester->PlayerState != GS->GetLeader())
	{
		return false;
	}
	BeginBoarding();
	return true;
}

bool ANoclipWaitingRoomGameMode::AreAllReady() const
{
	const ANoclipGameState* GS = GetGameState<ANoclipGameState>();
	if (!GS || GS->PlayerArray.Num() == 0)
	{
		return false;
	}
	for (const APlayerState* PS : GS->PlayerArray)
	{
		const ANoclipPlayerState* NPS = Cast<ANoclipPlayerState>(PS);
		if (NPS && !NPS->IsOnlyASpectator() && !NPS->IsReady())
		{
			return false;
		}
	}
	return true;
}

void ANoclipWaitingRoomGameMode::BeginBoarding()
{
	ANoclipWaitingRoom* R = GetRoom();
	if (!R || R->GetPhase() != ENoclipWaitingPhase::Gathering)
	{
		return;
	}
	R->SetPhase(ENoclipWaitingPhase::Boarding);
	if (ANoclipElevator* E = R->GetElevator())
	{
		E->SetDoorsOpen(true);
	}
	const double Now = GetWorld()->GetTimeSeconds();
	DoorsCloseAt = Now + R->GetBoardingFinishSeconds() + CloseDelay;
	TravelAt = -1.0;
	UE_LOG(LogNoclip, Log, TEXT("Sala de espera: embarque (%d estagiarios)"), GetNumPlayers());
}

void ANoclipWaitingRoomGameMode::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (ANoclipGameState* GS = GetGameState<ANoclipGameState>())
	{
		GS->RefreshLeader();
	}

	ANoclipWaitingRoom* R = GetRoom();
	if (!R || bTraveling)
	{
		return;
	}
	const double Now = GetWorld()->GetTimeSeconds();

	switch (R->GetPhase())
	{
	case ENoclipWaitingPhase::Gathering:
		if (AreAllReady())
		{
			if (AllReadySince < 0.0)
			{
				AllReadySince = Now;
			}
			else if (Now - AllReadySince >= AllReadyDelay)
			{
				BeginBoarding();
			}
		}
		else
		{
			AllReadySince = -1.0;
		}
		break;

	case ENoclipWaitingPhase::Boarding:
		if (DoorsCloseAt > 0.0 && Now >= DoorsCloseAt)
		{
			DoorsCloseAt = -1.0;
			if (ANoclipElevator* E = R->GetElevator())
			{
				E->SetDoorsOpen(false);
				TravelAt = Now + E->DoorTravelSeconds + DepartSeconds;
			}
			else
			{
				TravelAt = Now + DepartSeconds;
			}
			R->SetPhase(ENoclipWaitingPhase::Departing);
		}
		break;

	case ENoclipWaitingPhase::Departing:
		if (TravelAt > 0.0 && Now >= TravelAt)
		{
			TravelToLevel();
		}
		break;
	}
}

void ANoclipWaitingRoomGameMode::TravelToLevel()
{
	bTraveling = true;

	FString Map = TEXT("/Game/NOCLIP/Maps/L_Level0_Test");
	if (const UNoclipSessionSubsystem* Sessions = GetGameInstance() ? GetGameInstance()->GetSubsystem<UNoclipSessionSubsystem>() : nullptr)
	{
		Map = Sessions->DefaultTravelMap;
	}
	const FString URL = FString::Printf(TEXT("%s?listen?Room=%s?Code=%s?JIP=%d?Max=%d?Priv=%d?Arrive=1?Expect=%d"),
		*Map, *TravelOption(RoomName), *InviteCode, bJoinInProgress ? 1 : 0, MaxPlayers, Privacy, GetNumPlayers());
	UE_LOG(LogNoclip, Log, TEXT("Sala de espera: elevador desceu -> %s"), *URL);
	// Absoluta: uma viagem relativa herdaria o ?game= da sala de espera e o Level 0 abriria com este GameMode.
	GetWorld()->ServerTravel(URL, true);
}
