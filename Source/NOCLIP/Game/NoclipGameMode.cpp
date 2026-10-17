// Copyright Fuly Games. All Rights Reserved.

#include "Game/NoclipGameMode.h"
#include "Game/NoclipGameState.h"
#include "Game/NoclipPlayerController.h"
#include "Character/NoclipCharacter.h"
#include "World/NoclipLevelGenerator.h"
#include "World/NoclipExtractionZone.h"
#include "UI/NoclipDebugHUD.h"
#include "NOCLIP.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerState.h"
#include "Game/NoclipPlayerState.h"
#include "Game/NoclipShop.h"
#include "World/NoclipLootActor.h"
#include "World/NoclipElevator.h"
#include "Character/NoclipGrabComponent.h"
#include "Character/NoclipToolComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Enemy/NoclipEnemyDirector.h"
#include "World/NoclipCorpse.h"
#include "World/NoclipPowerGenerator.h"
#include "Enemy/NoclipEnemy.h"
#include "HAL/IConsoleManager.h"
#include "Movement/NoclipMoverComponent.h"
#include "Components/SkeletalMeshComponent.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(NoclipGameMode)

ANoclipGameMode::ANoclipGameMode()
{
	GameStateClass = ANoclipGameState::StaticClass();
	PlayerControllerClass = ANoclipPlayerController::StaticClass();
	DefaultPawnClass = ANoclipCharacter::StaticClass();
	HUDClass = ANoclipDebugHUD::StaticClass();
	PlayerStateClass = ANoclipPlayerState::StaticClass();
	bUseSeamlessTravel = true;

	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 0.1f;
}

void ANoclipGameMode::InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage)
{
	Super::InitGame(MapName, Options, ErrorMessage);

	if (UGameplayStatics::HasOption(Options, TEXT("seed")))
	{
		PendingSeed = UGameplayStatics::GetIntOption(Options, TEXT("seed"), 0);
	}
	else if (FixedSeed != 0)
	{
		PendingSeed = FixedSeed;
	}
	else
	{
		PendingSeed = FMath::RandRange(1, MAX_int32 - 1);
	}

	RoomName = UGameplayStatics::ParseOption(Options, TEXT("Room"));
	RoomName.ReplaceInline(TEXT("+"), TEXT(" "));
	InviteCode = UGameplayStatics::ParseOption(Options, TEXT("Code"));
	bJoinInProgress = !UGameplayStatics::HasOption(Options, TEXT("JIP")) || UGameplayStatics::GetIntOption(Options, TEXT("JIP"), 1) != 0;
	if (RoomName.IsEmpty())
	{
		RoomName = TEXT("Expediente");
	}
	MaxPlayers = FMath::Clamp(UGameplayStatics::GetIntOption(Options, TEXT("Max"), 4), 1, 6);
	Privacy = static_cast<uint8>(FMath::Clamp(UGameplayStatics::GetIntOption(Options, TEXT("Priv"), 0), 0, 2));
	bArrivalPending = UGameplayStatics::GetIntOption(Options, TEXT("Arrive"), 0) != 0;
	ExpectedArrivals = FMath::Max(1, UGameplayStatics::GetIntOption(Options, TEXT("Expect"), 1));
}

void ANoclipGameMode::PreLogin(const FString& Options, const FString& Address, const FUniqueNetIdRepl& UniqueId, FString& ErrorMessage)
{
	Super::PreLogin(Options, Address, UniqueId, ErrorMessage);
	// Quem vem no elevador da sala de espera (?Arrive=1) sempre entra, mesmo se o turno ja comecou sem ele.
	const bool bArrivingCrew = UGameplayStatics::HasOption(Options, TEXT("Arrive"));
	if (ErrorMessage.IsEmpty() && !bJoinInProgress && !bArrivingCrew && CurrentPhase != ENoclipShiftPhase::Lobby)
	{
		ErrorMessage = TEXT("O expediente ja comecou e esta sala nao aceita entrada no meio do turno.");
	}
}

bool ANoclipGameMode::IsLeader(const AController* Controller) const
{
	const ANoclipGameState* GS = GetNoclipGameState();
	return Controller && GS && GS->GetLeader() && Controller->PlayerState == GS->GetLeader();
}

bool ANoclipGameMode::RequestSkipToLobby(AController* Requester)
{
	if (CurrentPhase != ENoclipShiftPhase::Ended || !IsLeader(Requester))
	{
		return false;
	}
	ReturnToLobby();
	return true;
}

bool ANoclipGameMode::RequestBuyItem(AController* Requester, int32 ItemIndex)
{
	ANoclipGameState* GS = GetNoclipGameState();
	if (!GS || !Requester || CurrentPhase != ENoclipShiftPhase::Lobby || !NoclipShop::Items().IsValidIndex(ItemIndex))
	{
		return false;
	}
	const int32 Price = NoclipShop::Items()[ItemIndex].Price;
	if (TeamMoney < Price)
	{
		return false;
	}
	// Ferramenta: vai para o cinto de quem comprou (sem repetir lanterna; no maximo 3 kits).
	const ENoclipTool Tool = NoclipShop::ToolFor(ItemIndex);
	if (Tool != ENoclipTool::None)
	{
		ANoclipPlayerState* BuyerPS = Requester->GetPlayerState<ANoclipPlayerState>();
		const bool bFull = !BuyerPS || (Tool == ENoclipTool::Medkit ? BuyerPS->GetMedkitCount() >= NoclipTools::MaxMedkits : BuyerPS->HasTool(Tool));
		if (bFull)
		{
			return false;
		}
		BuyerPS->GiveTool(Tool);
	}
	TeamMoney -= Price;
	GS->SetMoney(TeamMoney);
	GS->AddOwned(ItemIndex);
	UE_LOG(LogNoclip, Log, TEXT("Almoxarifado: %s comprou %s por %d (caixa %d)"), *GetNameSafe(Requester->PlayerState), NoclipShop::Items()[ItemIndex].Id, Price, TeamMoney);
	return true;
}

void ANoclipGameMode::SetPlayerReady(AController* Requester, bool bReady)
{
	if (ANoclipPlayerState* PS = Requester ? Requester->GetPlayerState<ANoclipPlayerState>() : nullptr)
	{
		PS->SetReady(bReady && CurrentPhase == ENoclipShiftPhase::Lobby);
	}
}

void ANoclipGameMode::UpdateTimeAlone(float DeltaSeconds)
{
	TArray<APawn*> Pawns;
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		if (APawn* P = It->Get() ? It->Get()->GetPawn() : nullptr)
		{
			Pawns.Add(P);
		}
	}
	if (Pawns.Num() < 2)
	{
		return;
	}
	for (APawn* P : Pawns)
	{
		float Nearest = TNumericLimits<float>::Max();
		for (const APawn* Other : Pawns)
		{
			if (Other != P)
			{
				Nearest = FMath::Min(Nearest, FVector::Dist(P->GetActorLocation(), Other->GetActorLocation()));
			}
		}
		if (Nearest > 1500.f)
		{
			if (ANoclipPlayerState* PS = P->GetPlayerState<ANoclipPlayerState>())
			{
				PS->TimeAlone += DeltaSeconds;
			}
		}
	}
}

void ANoclipGameMode::InitGameState()
{
	Super::InitGameState();

	if (ANoclipGameState* GS = GetGameState<ANoclipGameState>())
	{
		GS->SetLevelSeed(PendingSeed);
		GS->SetShiftInfo(CurrentShift, GetQuotaForShift(CurrentShift));
		GS->SetMoney(TeamMoney);
		GS->SetRoomInfo(RoomName, InviteCode, bJoinInProgress);
		GS->SetRoomExtras(MaxPlayers, Privacy);
		GS->SetArrival(bArrivalPending, ExpectedArrivals);
		UE_LOG(LogNoclip, Log, TEXT("Seed do nivel: %d"), PendingSeed);
	}
}

void ANoclipGameMode::StartPlay()
{
	SpawnElevator();
	Super::StartPlay();
	ArrivalDeadline = GetWorld()->GetTimeSeconds() + ArrivalTimeoutSeconds;
	EnterPhase(ENoclipShiftPhase::Lobby);
	if (bAutoStartFirstShift)
	{
		PhaseEndTime = GetWorld()->GetTimeSeconds() + AutoStartDelay;
	}
}

void ANoclipGameMode::SpawnElevator()
{
	if (IsValid(Elevator))
	{
		return;
	}
	const ANoclipLevelGenerator* Gen = FindGenerator();
	const FTransform Base = Gen ? FTransform(FRotator(0.f, Gen->GetActorRotation().Yaw, 0.f), Gen->GetActorLocation()) : FTransform::Identity;
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	Elevator = GetWorld()->SpawnActor<ANoclipElevator>(ANoclipElevator::StaticClass(), FTransform(ElevatorOffset) * Base, Params);
	if (Elevator)
	{
		Elevator->SetFloorLabel(TEXT("0"));
		Elevator->SetDoorsOpen(false, true);
	}
}

void ANoclipGameMode::RestartPlayer(AController* NewPlayer)
{
	// O host vem da sala de espera por seamless travel e ganha o pawn ANTES do StartPlay (quando o elevador
	// ainda nao existia) — nascia no PlayerStart, fora da cabine. Cria o elevador aqui se preciso.
	if (NewPlayer && !IsValid(Elevator))
	{
		SpawnElevator();
	}
	// Nasceu de novo (saguao / turno novo): nao esta mais morto.
	if (ANoclipPlayerState* NPS = NewPlayer ? NewPlayer->GetPlayerState<ANoclipPlayerState>() : nullptr)
	{
		NPS->SetDead(false);
	}
	if (NewPlayer && IsValid(Elevator))
	{
		const int32 Index = GetCabinIndex(NewPlayer);
		const FTransform Slot = Elevator->GetCabinSlot(Index);
		RestartPlayerAtTransform(NewPlayer, Slot);
		const APawn* Pawn = NewPlayer->GetPawn();
		UE_LOG(LogNoclip, Log, TEXT("[Elevador] %s nasceu no lugar %d (%s) -> pawn em %s%s"), *GetNameSafe(NewPlayer->PlayerState), Index,
			*Slot.GetLocation().ToCompactString(), Pawn ? *Pawn->GetActorLocation().ToCompactString() : TEXT("(sem pawn)"),
			GetWorld()->HasBegunPlay() ? TEXT("") : TEXT(" [antes do BeginPlay]"));
		return;
	}
	Super::RestartPlayer(NewPlayer);
}

void ANoclipGameMode::TickArrival()
{
	if (!bArrivalPending || CurrentPhase != ENoclipShiftPhase::Lobby)
	{
		return;
	}
	const double Now = GetWorld()->GetTimeSeconds();
	// Na viagem seamless os controllers dos clientes ja existem antes de eles terminarem de carregar:
	// so conta quem ja carregou o Level 0 e esta na cabine (com pawn).
	int32 N = 0;
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		APlayerController* PC = It->Get(); // HasClientLoadedCurrentWorld nao e const
		if (PC && PC->GetPawn() && (PC->IsLocalController() || PC->HasClientLoadedCurrentWorld()))
		{
			++N;
		}
	}
	if (N <= 0)
	{
		return;
	}
	if (ArrivalStartAt < 0.0)
	{
		if (N >= ExpectedArrivals || Now >= ArrivalDeadline)
		{
			ArrivalStartAt = Now + ArrivalSettleSeconds;
		}
		return;
	}
	if (Now >= ArrivalStartAt)
	{
		bArrivalPending = false;
		if (ANoclipGameState* GS = GetNoclipGameState())
		{
			GS->SetArrival(false, ExpectedArrivals);
		}
		UE_LOG(LogNoclip, Log, TEXT("Elevador chegou ao Level 0 com %d/%d estagiarios: turno 1."), N, ExpectedArrivals);
		bSkipNextRespawn = true; // ja estao na cabine
		KeepCrewInCabin(TEXT("chegada"));
		RequestStartShift(nullptr);
	}
}

int32 ANoclipGameMode::GetCabinIndex(const AController* Controller) const
{
	// Lugar fixo por jogador: a ordem de entrada na sala (o host e sempre o 0).
	const ANoclipGameState* GS = GetNoclipGameState();
	const int32 Index = (GS && Controller) ? GS->PlayerArray.IndexOfByKey(Controller->PlayerState) : INDEX_NONE;
	if (Index != INDEX_NONE)
	{
		return Index;
	}
	return (Controller && Controller->IsLocalController()) ? 0 : 1;
}

void ANoclipGameMode::KeepCrewInCabin(const TCHAR* Reason)
{
	if (!IsValid(Elevator))
	{
		return;
	}
	// Portas fechadas (saguao / descendo): ninguem pode estar fora da cabine. Quem estiver, volta para o lugar.
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		APlayerController* PC = It->Get();
		APawn* Pawn = PC ? PC->GetPawn() : nullptr;
		if (!Pawn)
		{
			continue;
		}
		const int32 Index = GetCabinIndex(PC);
		const FVector Slot = Elevator->GetCabinSlot(Index).GetLocation();
		const FVector Local = Elevator->GetActorTransform().InverseTransformPosition(Pawn->GetActorLocation());
		// Dentro da cabine: X de -300 a 0 e Y de -180 a 180 no espaco do elevador (com folga).
		const bool bInside = Local.X > -320.f && Local.X < 20.f && FMath::Abs(Local.Y) < 200.f && FMath::Abs(Local.Z - Slot.Z + Elevator->GetActorLocation().Z) < 400.f;
		if (!bInside)
		{
			UE_LOG(LogNoclip, Warning, TEXT("[Elevador] (%s) %s estava fora da cabine em %s (local %s): volta para o lugar %d."),
				Reason, *GetNameSafe(PC->PlayerState), *Pawn->GetActorLocation().ToCompactString(), *Local.ToCompactString(), Index);
			PC->UnPossess();
			Pawn->Destroy();
			RestartPlayer(PC);
		}
	}
}

ANoclipGameState* ANoclipGameMode::GetNoclipGameState() const
{
	return GetGameState<ANoclipGameState>();
}

ANoclipLevelGenerator* ANoclipGameMode::FindGenerator() const
{
	for (TActorIterator<ANoclipLevelGenerator> It(GetWorld()); It; ++It)
	{
		return *It;
	}
	return nullptr;
}

int32 ANoclipGameMode::GetQuotaForShift(int32 ShiftIndex) const
{
	if (QuotaSchedule.Num() == 0)
	{
		return 300 * FMath::Max(1, ShiftIndex);
	}
	const int32 Idx = FMath::Max(0, ShiftIndex - 1);
	if (QuotaSchedule.IsValidIndex(Idx))
	{
		return QuotaSchedule[Idx];
	}
	const int32 Extra = Idx - (QuotaSchedule.Num() - 1);
	return FMath::RoundToInt(QuotaSchedule.Last() * FMath::Pow(1.f + QuotaGrowthAfterSchedule, static_cast<float>(Extra)));
}

void ANoclipGameMode::EnterPhase(ENoclipShiftPhase NewPhase)
{
	CurrentPhase = NewPhase;
	if (NewPhase != ENoclipShiftPhase::Collapse)
	{
		WipeEndAt = -1.0;
		bWipeEnding = false;
	}

	float Duration = 0.f;
	switch (NewPhase)
	{
	case ENoclipShiftPhase::Preparing: Duration = PrepareSeconds; break;
	case ENoclipShiftPhase::Active:    Duration = FMath::Max(1.f, ShiftDurationSeconds - CollapseSeconds); break;
	case ENoclipShiftPhase::Collapse:  Duration = CollapseSeconds; break;
	case ENoclipShiftPhase::Ended:     Duration = ResultSeconds; break;
	default: break;
	}

	PhaseEndTime = Duration > 0.f ? GetWorld()->GetTimeSeconds() + Duration : 0.0;

	// Elevador: fechado no saguao e na contagem (descendo), abre quando o expediente comeca.
	if (IsValid(Elevator))
	{
		switch (NewPhase)
		{
		case ENoclipShiftPhase::Lobby:     Elevator->SetDoorsOpen(false, true); break;
		case ENoclipShiftPhase::Preparing: Elevator->SetDoorsOpen(false); break;
		case ENoclipShiftPhase::Active:    Elevator->SetDoorsOpen(true); break;
		default: break;
		}
	}

	if (ANoclipGameState* GS = GetNoclipGameState())
	{
		// O relogio do cliente mostra o tempo ate o fim do TURNO durante Ativo (inclui o colapso).
		const float ShownDuration = NewPhase == ENoclipShiftPhase::Active ? ShiftDurationSeconds : Duration;
		GS->SetPhase(NewPhase, ShownDuration);
	}

	// Apagao e salas que mudam: so durante o expediente.
	{
		ANoclipGameState* PowerGS = GetNoclipGameState();
		const double PhaseNow = GetWorld()->GetTimeSeconds();
		if (NewPhase == ENoclipShiftPhase::Active)
		{
			NextBlackoutTime = PhaseNow + FMath::FRandRange(FirstBlackoutDelay.X, FirstBlackoutDelay.Y);
			NextRoomShiftTime = PhaseNow + FMath::FRandRange(RoomShiftInterval.X, RoomShiftInterval.Y);
			NoclipCooldown.Reset();
		}
		else
		{
			NextBlackoutTime = -1.0;
			NextRoomShiftTime = -1.0;
			// O colapso assume as luzes; fora do turno a energia sempre volta.
			if (PowerGS)
			{
				PowerGS->SetPowerOut(false);
			}
		}
	}

	// Entidades: soltas quando o expediente comeca; frenesi no colapso; somem no fim do turno.
	switch (NewPhase)
	{
	case ENoclipShiftPhase::Active:   GetEnemyDirector()->StartShift(CurrentShift); break;
	case ENoclipShiftPhase::Collapse: GetEnemyDirector()->SetCollapse(true); break;
	case ENoclipShiftPhase::Ended:
	case ENoclipShiftPhase::Lobby:
		if (IsValid(EnemyDirector))
		{
			EnemyDirector->StopShift();
		}
		break;
	default: break;
	}

	UE_LOG(LogNoclip, Log, TEXT("[Turno %d] Fase: %s (%.0f s)"), CurrentShift, *UEnum::GetValueAsString(NewPhase), Duration);
}

ANoclipEnemyDirector* ANoclipGameMode::GetEnemyDirector()
{
	if (!IsValid(EnemyDirector))
	{
		FActorSpawnParameters Params;
		Params.Owner = this;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		EnemyDirector = GetWorld()->SpawnActor<ANoclipEnemyDirector>(ANoclipEnemyDirector::StaticClass(), FTransform::Identity, Params);
	}
	return EnemyDirector;
}

void ANoclipGameMode::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	// Lider: o host local (listen server) ou, sem host local, o primeiro jogador conectado.
	if (ANoclipGameState* GS = GetNoclipGameState())
	{
		GS->RefreshLeader();
	}

	if (CurrentPhase == ENoclipShiftPhase::Active || CurrentPhase == ENoclipShiftPhase::Collapse)
	{
		UpdateTimeAlone(DeltaSeconds);

		TickGlitches(GetWorld()->GetTimeSeconds());
		const double EventNow = GetWorld()->GetTimeSeconds();
		if (CurrentPhase == ENoclipShiftPhase::Active)
		{
			const ANoclipGameState* EventGS = GetNoclipGameState();
			if (EventGS && !EventGS->IsPowerOut() && NextBlackoutTime > 0.0 && EventNow >= NextBlackoutTime && PhaseEndTime - EventNow > 75.0)
			{
				StartBlackout();
			}
			if (NextRoomShiftTime > 0.0 && EventNow >= NextRoomShiftTime)
			{
				NextRoomShiftTime = EventNow + FMath::FRandRange(RoomShiftInterval.X, RoomShiftInterval.Y);
				TryShiftRooms(false);
			}
		}

		// Equipe inteira morta: o turno acaba sozinho.
		const double WipeNow = GetWorld()->GetTimeSeconds();
		if (GetNumPlayers() > 0 && !AnyoneAlive())
		{
			if (WipeEndAt < 0.0)
			{
				WipeEndAt = WipeNow + TeamWipeDelay;
				UE_LOG(LogNoclip, Log, TEXT("[Turno %d] Ninguem de pe: o turno acaba em %.0f s."), CurrentShift, TeamWipeDelay);
			}
			else if (WipeNow >= WipeEndAt)
			{
				bWipeEnding = true;
				EndShift(false);
				return;
			}
		}
		else
		{
			WipeEndAt = -1.0;
		}
	}

	if (bDebugExtractNow)
	{
		bDebugExtractNow = false;
		DebugExtract();
	}
	if (bDebugCollapseNow)
	{
		bDebugCollapseNow = false;
		if (CurrentPhase == ENoclipShiftPhase::Active)
		{
			UE_LOG(LogNoclip, Warning, TEXT("[Debug] Pulando para o colapso."));
			EnterPhase(ENoclipShiftPhase::Collapse);
		}
	}

	const double Now = GetWorld()->GetTimeSeconds();
	const bool bTimeUp = PhaseEndTime > 0.0 && Now >= PhaseEndTime;

	// Portas fechadas: confere a cabine a cada meio segundo.
	if ((CurrentPhase == ENoclipShiftPhase::Lobby || CurrentPhase == ENoclipShiftPhase::Preparing) && Now >= NextCabinCheckTime)
	{
		NextCabinCheckTime = Now + 0.5;
		KeepCrewInCabin(CurrentPhase == ENoclipShiftPhase::Lobby ? TEXT("saguao") : TEXT("descendo"));
	}

	switch (CurrentPhase)
	{
	case ENoclipShiftPhase::Lobby:
		TickArrival();
		if (bAutoStartFirstShift && !bFirstShiftStarted && bTimeUp && GetNumPlayers() > 0)
		{
			RequestStartShift(nullptr);
		}
		break;
	case ENoclipShiftPhase::Preparing:
		if (bTimeUp) { EnterPhase(ENoclipShiftPhase::Active); }
		break;
	case ENoclipShiftPhase::Active:
		if (bTimeUp)
		{
			if (CollapseSeconds > 0.f) { EnterPhase(ENoclipShiftPhase::Collapse); }
			else { EndShift(false); }
		}
		break;
	case ENoclipShiftPhase::Collapse:
		if (bTimeUp) { EndShift(false); }
		break;
	case ENoclipShiftPhase::Ended:
		if (bTimeUp) { ReturnToLobby(); }
		break;
	}
}

bool ANoclipGameMode::RequestStartShift(AController* Requester)
{
	if (CurrentPhase != ENoclipShiftPhase::Lobby)
	{
		return false;
	}
	// So o lider do grupo inicia; nullptr = inicio automatico.
	const ANoclipGameState* GS = GetNoclipGameState();
	if (Requester && (!GS || !GS->GetLeader() || Requester->PlayerState != GS->GetLeader()))
	{
		UE_LOG(LogNoclip, Log, TEXT("Pedido de inicio ignorado: so o lider do grupo inicia o turno."));
		return false;
	}
	BeginShift();
	return true;
}

void ANoclipGameMode::BeginShift()
{
	// O primeiro turno usa a seed gerada no InitGameState; os seguintes sorteiam outra.
	const bool bNeedNewSeed = bFirstShiftStarted;
	bFirstShiftStarted = true;

	ANoclipGameState* GS = GetNoclipGameState();
	if (!GS)
	{
		return;
	}

	GS->SetShiftInfo(CurrentShift, GetQuotaForShift(CurrentShift));
	GS->SetExtractionStatus(0, 0.f, 0);

	for (APlayerState* PS : GS->PlayerArray)
	{
		if (ANoclipPlayerState* NPS = Cast<ANoclipPlayerState>(PS))
		{
			NPS->ResetShiftStats();
			NPS->SetReady(false);
		}
	}

	if (bNeedNewSeed || GS->GetLevelSeed() == 0)
	{
		const int32 NewSeed = FixedSeed != 0 ? FixedSeed + CurrentShift : FMath::RandRange(1, MAX_int32 - 1);
		GS->SetLevelSeed(NewSeed); // o gerador regenera em todas as maquinas
		UE_LOG(LogNoclip, Log, TEXT("[Turno %d] Seed: %d"), CurrentShift, NewSeed);
	}

	ClearCorpses();
	GS->SetLayoutShiftMask(0);
	if (ANoclipLevelGenerator* Gen = FindGenerator())
	{
		Gen->SpawnShiftContent();
	}

	if (bSkipNextRespawn)
	{
		bSkipNextRespawn = false;
	}
	else
	{
		RespawnAllPlayers();
	}
	EnterPhase(ENoclipShiftPhase::Preparing);
}

void ANoclipGameMode::RespawnAllPlayers()
{
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		APlayerController* PC = It->Get();
		if (!PC)
		{
			continue;
		}
		if (APawn* Pawn = PC->GetPawn())
		{
			PC->UnPossess();
			Pawn->Destroy();
		}
		RestartPlayer(PC);
	}
}

void ANoclipGameMode::NotifyExtractionComplete()
{
	if (CurrentPhase == ENoclipShiftPhase::Active || CurrentPhase == ENoclipShiftPhase::Collapse)
	{
		EndShift(true);
	}
}

void ANoclipGameMode::EndShift(bool bExtractedEarly)
{
	ANoclipGameState* GS = GetNoclipGameState();
	ANoclipLevelGenerator* Gen = FindGenerator();
	ANoclipExtractionZone* Zone = Gen ? Gen->GetExtractionZone() : nullptr;

	FNoclipShiftResult Result;
	Result.ShiftIndex = CurrentShift;
	Result.Quota = GetQuotaForShift(CurrentShift);
	Result.bExtractedEarly = bExtractedEarly;
	Result.PlayersTotal = GetNumPlayers();
	Result.bTeamWiped = bWipeEnding;
	bWipeEnding = false;
	WipeEndAt = -1.0;

	ApplyLeftBehind(Zone, Result, bExtractedEarly);

	if (Zone)
	{
		Zone->ComputeContents(Result.Extracted, Result.ItemsExtracted, Result.PlayersExtracted);
		Zone->SetElevatorLocked(true);

		// "Mais carregou": credita o valor de cada achado na sala a quem segurou por ultimo.
		for (TActorIterator<ANoclipLootActor> It(GetWorld()); It; ++It)
		{
			if (IsValid(*It) && Zone->IsInside(It->GetActorLocation()))
			{
				if (ANoclipPlayerState* Carrier = It->GetLastHolder())
				{
					Carrier->ValueCarried += It->GetCurrentValue();
				}
			}
		}
	}

	Result.bQuotaMet = Result.Extracted >= Result.Quota;
	Result.bFired = !Result.bQuotaMet;
	Result.MoneyEarned = Result.bQuotaMet ? (Result.Extracted - Result.Quota) + QuotaMetBonus : 0;
	Result.BonusPaid = Result.bQuotaMet ? QuotaMetBonus : 0;

	if (Result.bQuotaMet)
	{
		TeamMoney += Result.MoneyEarned;
		CurrentShift += 1;
	}
	else
	{
		// Demissao: a corrida recomeca do zero (caixa e almoxarifado).
		Result.ShiftsCompleted = CurrentShift - 1;
		Result.MoneyLost = TeamMoney;
		TeamMoney = 0;
		CurrentShift = 1;
		if (GS)
		{
			GS->ResetOwned();
			// Corrida nova: ninguem comeca "sem itens".
			for (APlayerState* PS : GS->PlayerArray)
			{
				if (ANoclipPlayerState* NPS = Cast<ANoclipPlayerState>(PS))
				{
					NPS->SetLeftBehind(false);
				}
			}
		}
	}
	Result.NextQuota = GetQuotaForShift(CurrentShift);

	if (GS)
	{
		GS->PublishResult(Result);
		GS->SetMoney(TeamMoney);
	}

	UE_LOG(LogNoclip, Log, TEXT("[Turno %d] Fim: extraido %d / quota %d -> %s, +%d (caixa %d), %d/%d na sala, %d para tras (-%d carregados)"),
		Result.ShiftIndex, Result.Extracted, Result.Quota, Result.bQuotaMet ? TEXT("QUOTA BATIDA") : TEXT("DEMITIDOS"), Result.MoneyEarned, TeamMoney,
		Result.PlayersExtracted, Result.PlayersTotal, Result.PlayersLeftBehind, Result.ValueLostCarried);

	EnterPhase(ENoclipShiftPhase::Ended);
}

void ANoclipGameMode::ApplyLeftBehind(ANoclipExtractionZone* Zone, FNoclipShiftResult& Result, bool bExtractedEarly)
{
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		APlayerController* PC = It->Get();
		ANoclipPlayerState* PS = PC ? PC->GetPlayerState<ANoclipPlayerState>() : nullptr;
		if (!PS)
		{
			continue;
		}
		if (PS->IsDead())
		{
			// Morreu: ja largou tudo e perdeu o cinto; volta no proximo turno sem itens.
			++Result.PlayersDied;
			PS->SetLeftBehind(true);
			continue;
		}
		ANoclipCharacter* Pawn = Cast<ANoclipCharacter>(PC->GetPawn());
		// Saida pelo elevador exige todos dentro; sem sala (ou sem pawn) ninguem e punido.
		const bool bOutside = !bExtractedEarly && Zone && Pawn && !Zone->IsInside(Pawn->GetActorLocation());
		PS->SetLeftBehind(bOutside);
		if (!bOutside)
		{
			continue;
		}
		++Result.PlayersLeftBehind;
		PS->StripShopTools(); // GDD: volta no proximo turno sem itens (o celular e pessoal)
		if (UNoclipToolComponent* Tools = Pawn->GetToolComponent())
		{
			Tools->ServerHolster();
		}

		// Perde o achado que estava carregando: some junto com o nivel.
		UNoclipGrabComponent* Grab = Pawn->GetGrabComponent();
		if (ANoclipLootActor* Held = Grab ? Cast<ANoclipLootActor>(Grab->GetHeldActor()) : nullptr)
		{
			Result.ValueLostCarried += Held->GetCurrentValue();
			Grab->ForceDrop();
			Held->Destroy();
		}
		UE_LOG(LogNoclip, Log, TEXT("[Turno %d] %s ficou para tras."), CurrentShift, *GetNameSafe(PS));
	}
}

// ===================================================================== Apagao

void ANoclipGameMode::StartBlackout()
{
	ANoclipGameState* GS = GetNoclipGameState();
	ANoclipLevelGenerator* Gen = FindGenerator();
	ANoclipPowerGenerator* Power = Gen ? Gen->GetPowerGenerator() : nullptr;
	if (!GS || GS->IsPowerOut() || !Power)
	{
		NextBlackoutTime = GetWorld()->GetTimeSeconds() + 30.0;
		return;
	}
	Power->BeginOutage();
	GS->SetPowerOut(true);
	NextBlackoutTime = -1.0;
	UE_LOG(LogNoclip, Log, TEXT("[Turno %d] Apagao: gerador em %s."), CurrentShift, *Power->GetActorLocation().ToCompactString());
}

void ANoclipGameMode::RestorePower(ANoclipPowerGenerator* Generator)
{
	ANoclipGameState* GS = GetNoclipGameState();
	if (!GS || !GS->IsPowerOut())
	{
		return;
	}
	GS->SetPowerOut(false);
	if (CurrentPhase == ENoclipShiftPhase::Active)
	{
		NextBlackoutTime = GetWorld()->GetTimeSeconds() + FMath::FRandRange(BlackoutInterval.X, BlackoutInterval.Y);
	}
	UE_LOG(LogNoclip, Log, TEXT("[Turno %d] Energia de volta (%d no gerador)."), CurrentShift, Generator ? Generator->GetWorkers() : 0);
}

// ===================================================================== Noclip e salas que mudam

void ANoclipGameMode::TickGlitches(double Now)
{
	ANoclipLevelGenerator* Gen = FindGenerator();
	if (!Gen || Gen->GetGlitches().Num() == 0)
	{
		return;
	}
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		ANoclipCharacter* Pawn = It->Get() ? Cast<ANoclipCharacter>(It->Get()->GetPawn()) : nullptr;
		if (!Pawn || Pawn->IsDying())
		{
			continue;
		}
		double& Cooldown = NoclipCooldown.FindOrAdd(Pawn);
		if (Now < Cooldown)
		{
			continue;
		}
		const int32 Glitch = Gen->FindGlitchAt(Pawn->GetActorLocation());
		if (Glitch == INDEX_NONE)
		{
			continue;
		}
		FVector Dest;
		if (!Gen->ResolveGlitchDestination(Glitch, Pawn->GetActorLocation(), Dest))
		{
			Cooldown = Now + 1.0;
			continue;
		}
		const ENoclipGlitchKind Kind = Gen->GetGlitches()[Glitch].Kind;
		Pawn->ServerNoclipTo(Dest, static_cast<uint8>(Kind));
		Cooldown = Now + 2.5;
		UE_LOG(LogNoclip, Log, TEXT("[Noclip] %s atravessou (%d) -> %s"), *GetNameSafe(Pawn->GetPlayerState()), static_cast<int32>(Kind), *Dest.ToCompactString());
	}
	for (auto It = NoclipCooldown.CreateIterator(); It; ++It)
	{
		if (!It.Key().IsValid())
		{
			It.RemoveCurrent();
		}
	}
}

void ANoclipGameMode::TryShiftRooms(bool bForce)
{
	ANoclipGameState* GS = GetNoclipGameState();
	ANoclipLevelGenerator* Gen = FindGenerator();
	if (!GS || !Gen)
	{
		return;
	}
	const int32 Count = FMath::Min(Gen->GetNumShifters(), 32);
	if (Count == 0)
	{
		return;
	}
	TArray<FVector> Busy;
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		if (const APawn* P = It->Get() ? It->Get()->GetPawn() : nullptr)
		{
			Busy.Add(P->GetActorLocation());
		}
	}
	TArray<FVector> Enemies;
	for (TActorIterator<ANoclipEnemy> It(GetWorld()); It; ++It)
	{
		Enemies.Add(It->GetActorLocation());
	}
	TArray<FVector> Loot;
	for (TActorIterator<ANoclipLootActor> It(GetWorld()); It; ++It)
	{
		Loot.Add(It->GetActorLocation());
	}

	uint32 Mask = GS->GetLayoutShiftMask();
	int32 Toggled = 0;
	for (int32 i = 0; i < Count; ++i)
	{
		const FNoclipShifter& S = Gen->GetShifter(i);
		const bool bAtB = ((Mask >> i) & 1u) != 0;
		const FVector Closing = bAtB ? S.CenterA : S.CenterB; // a porta que vai fechar
		// So muda onde ninguem esta olhando de perto (as Backrooms mudam quando voce vira as costas).
		auto Near = [](const TArray<FVector>& List, const FVector& At, float Radius)
		{
			return List.ContainsByPredicate([&](const FVector& L) { return FVector::Dist2D(L, At) < Radius && FMath::Abs(L.Z - At.Z) < 400.f; });
		};
		if (Near(Busy, S.CenterA, 900.f) || Near(Busy, S.CenterB, 900.f) || Near(Enemies, Closing, 320.f) || Near(Loot, Closing, 180.f))
		{
			continue;
		}
		if (bForce || FMath::FRand() < 0.5f)
		{
			Mask ^= (1u << i);
			++Toggled;
		}
	}
	if (Toggled > 0)
	{
		GS->SetLayoutShiftMask(Mask);
		UE_LOG(LogNoclip, Log, TEXT("[Turno %d] %d divisoria(s) mudaram de porta."), CurrentShift, Toggled);
	}
}

void ANoclipGameMode::DebugBlackout()
{
	const ANoclipGameState* GS = GetNoclipGameState();
	if (GS && GS->IsPowerOut())
	{
		RestorePower(nullptr);
	}
	else
	{
		StartBlackout();
	}
}

void ANoclipGameMode::DebugShiftRooms()
{
	TryShiftRooms(true);
}

void ANoclipGameMode::DebugNoclip(APlayerController* PC, bool bDown)
{
	ANoclipLevelGenerator* Gen = FindGenerator();
	ANoclipCharacter* Pawn = PC ? Cast<ANoclipCharacter>(PC->GetPawn()) : nullptr;
	if (!Gen || !Pawn)
	{
		return;
	}
	FVector Dest;
	if (Gen->FindWalkable(bDown ? 1 : 0, FVector2D(Pawn->GetActorLocation()), 1500.f, Dest))
	{
		Pawn->ServerNoclipTo(Dest, static_cast<uint8>(bDown ? ENoclipGlitchKind::FakeWall : ENoclipGlitchKind::ServiceExit));
	}
}

namespace
{
	ANoclipGameMode* DebugGameMode(UWorld* World)
	{
		return World ? World->GetAuthGameMode<ANoclipGameMode>() : nullptr;
	}

	FAutoConsoleCommandWithWorld GNoclipBlackoutCmd(TEXT("noclip.Blackout"), TEXT("Derruba (ou devolve) a energia agora."),
		FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World) { if (ANoclipGameMode* GM = DebugGameMode(World)) { GM->DebugBlackout(); } }));

	FAutoConsoleCommandWithWorld GNoclipShiftCmd(TEXT("noclip.ShiftRooms"), TEXT("Muda agora as divisorias que estao longe de todo mundo."),
		FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World) { if (ANoclipGameMode* GM = DebugGameMode(World)) { GM->DebugShiftRooms(); } }));

	FAutoConsoleCommandWithWorldAndArgs GNoclipGoDownCmd(TEXT("noclip.GoDown"), TEXT("Leva o host para o subsolo (0 = volta ao Level 0)."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			if (ANoclipGameMode* GM = DebugGameMode(World))
			{
				GM->DebugNoclip(World->GetFirstPlayerController(), Args.Num() == 0 || Args[0] != TEXT("0"));
			}
		}));
}

bool ANoclipGameMode::AnyoneAlive() const
{
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		const APlayerController* PC = It->Get();
		const ANoclipCharacter* Pawn = PC ? Cast<ANoclipCharacter>(PC->GetPawn()) : nullptr;
		if (Pawn && !Pawn->IsDying())
		{
			return true;
		}
	}
	return false;
}

void ANoclipGameMode::ClearCorpses()
{
	for (TActorIterator<ANoclipCorpse> It(GetWorld()); It; ++It)
	{
		It->Destroy();
	}
}

void ANoclipGameMode::NotifyPlayerDied(ANoclipCharacter* Pawn, ENoclipDeathCause Cause, const FVector& HitFrom)
{
	if (!IsValid(Pawn) || Pawn->IsActorBeingDestroyed())
	{
		return;
	}
	AController* Controller = Pawn->GetController();
	ANoclipPlayerState* PS = Pawn->GetPlayerState<ANoclipPlayerState>();

	// Larga o achado (fica no chao para os colegas buscarem) e guarda a ferramenta.
	if (UNoclipToolComponent* Tools = Pawn->GetToolComponent())
	{
		Tools->ServerHolster();
	}
	if (UNoclipGrabComponent* Grab = Pawn->GetGrabComponent())
	{
		Grab->ForceDrop();
	}

	// Corpo: o modelo do estagiario, empurrado para longe de quem bateu.
	const USkeletalMeshComponent* Body = Pawn->GetCharacterMesh();
	const FTransform BodyTransform = Body ? Body->GetComponentTransform() : Pawn->GetActorTransform();
	FVector Push = Pawn->GetNoclipMover() ? Pawn->GetNoclipMover()->GetVelocity() : FVector::ZeroVector;
	if (!HitFrom.IsNearlyZero())
	{
		Push += (Pawn->GetActorLocation() - HitFrom).GetSafeNormal2D() * 380.f + FVector(0.f, 0.f, 120.f);
	}
	ANoclipCorpse* Corpse = GetWorld()->SpawnActorDeferred<ANoclipCorpse>(ANoclipCorpse::StaticClass(), BodyTransform, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (Corpse)
	{
		Corpse->Setup(PS, Pawn->GetAppliedCharacter(), Push);
		Corpse->FinishSpawning(BodyTransform);
	}

	if (PS)
	{
		PS->SetDead(true, Cause);
		PS->StripShopTools();
	}
	UE_LOG(LogNoclip, Log, TEXT("[Turno %d] %s morreu (%s)."), CurrentShift, *GetNameSafe(PS), *UEnum::GetValueAsString(Cause));

	if (Controller)
	{
		Controller->UnPossess();
	}
	Pawn->Destroy();
}

void ANoclipGameMode::DebugExtract()
{
	ANoclipLevelGenerator* Gen = FindGenerator();
	ANoclipExtractionZone* Zone = Gen ? Gen->GetExtractionZone() : nullptr;
	if (!Zone)
	{
		UE_LOG(LogNoclip, Warning, TEXT("[Debug] Sem Sala Manila (o turno ja comecou?)."));
		return;
	}
	const FVector Center = Zone->GetActorLocation();
	const float FloorZ = Center.Z - Gen->WallHeight * 0.5f;

	int32 Index = 0;
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It, ++Index)
	{
		APlayerController* PC = It->Get();
		if (!PC)
		{
			continue;
		}
		if (APawn* Old = PC->GetPawn())
		{
			if (ANoclipCharacter* Char = Cast<ANoclipCharacter>(Old))
			{
				if (UNoclipGrabComponent* Grab = Char->GetGrabComponent())
				{
					Grab->ForceDrop();
				}
			}
			PC->UnPossess();
			Old->Destroy();
		}
		const FVector Loc(Center.X + (Index % 3 - 1) * 90.f, Center.Y + (Index / 3) * 90.f, FloorZ + 96.f);
		RestartPlayerAtTransform(PC, FTransform(FRotator::ZeroRotator, Loc));
	}

	int32 Moved = 0;
	for (TActorIterator<ANoclipLootActor> It(GetWorld()); It && Moved < 6; ++It)
	{
		if (!IsValid(*It) || Zone->IsInside(It->GetActorLocation()))
		{
			continue;
		}
		const FVector Loc(Center.X - 120.f + (Moved % 3) * 120.f, Center.Y - 160.f + (Moved / 3) * 320.f, FloorZ + 60.f);
		It->SetActorLocation(Loc, false, nullptr, ETeleportType::ResetPhysics);
		++Moved;
	}
	UE_LOG(LogNoclip, Warning, TEXT("[Debug] Equipe e %d achados levados para a Sala Manila."), Moved);
}

void ANoclipGameMode::ReturnToLobby()
{
	if (ANoclipGameState* GS = GetNoclipGameState())
	{
		GS->SetShiftInfo(CurrentShift, GetQuotaForShift(CurrentShift));
		GS->SetExtractionStatus(0, 0.f, 0);
	}
	ClearCorpses();
	if (ANoclipGameState* MaskGS = GetNoclipGameState())
	{
		MaskGS->SetLayoutShiftMask(0);
	}
	if (ANoclipLevelGenerator* Gen = FindGenerator())
	{
		Gen->ClearShiftContent();
	}
	if (ANoclipGameState* GS = GetNoclipGameState())
	{
		for (APlayerState* PS : GS->PlayerArray)
		{
			if (ANoclipPlayerState* NPS = Cast<ANoclipPlayerState>(PS))
			{
				NPS->SetReady(false);
			}
		}
	}
	RespawnAllPlayers();
	EnterPhase(ENoclipShiftPhase::Lobby);
}
