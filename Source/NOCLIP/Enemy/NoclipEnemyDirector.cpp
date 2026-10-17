// Copyright Fuly Games. All Rights Reserved.

#include "Enemy/NoclipEnemyDirector.h"
#include "Enemy/NoclipEnemy.h"
#include "Enemy/NoclipNoiseSubsystem.h"
#include "Character/NoclipCharacter.h"
#include "Character/NoclipGrabComponent.h"
#include "Movement/NoclipMoverComponent.h"
#include "World/NoclipLevelGenerator.h"
#include "World/NoclipElevator.h"
#include "Game/NoclipGameMode.h"
#include "NOCLIP.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Controller.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "GameFramework/PlayerState.h"
#include "Interfaces/VoiceInterface.h"
#include "OnlineSubsystemUtils.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(NoclipEnemyDirector)

TAutoConsoleVariable<int32> CVarNoclipDebugEnemies(TEXT("noclip.DebugEnemies"), 0,
	TEXT("Desenha estado, caminho e barulhos das entidades (1), mais alcance de visao (2), mais a grade de navegacao (3)."));

static TAutoConsoleVariable<int32> CVarNoclipEnemies(TEXT("noclip.Enemies"), 1,
	TEXT("0 = nenhuma entidade nasce sozinha no turno (noclip.SpawnEnemy continua funcionando)."));

namespace
{
	void SpawnEnemyCommand(const TArray<FString>& Args, UWorld* World)
	{
		if (!World || World->GetNetMode() == NM_Client)
		{
			return;
		}
		ANoclipGameMode* GM = World->GetAuthGameMode<ANoclipGameMode>();
		ANoclipEnemyDirector* Dir = GM ? GM->GetEnemyDirector() : nullptr;
		APlayerController* PC = World->GetFirstPlayerController();
		ANoclipCharacter* Pawn = PC ? Cast<ANoclipCharacter>(PC->GetPawn()) : nullptr;
		if (!Dir || !Pawn)
		{
			UE_LOG(LogNoclip, Warning, TEXT("noclip.SpawnEnemy: sem diretor ou sem jogador."));
			return;
		}
		const FString KindArg = Args.Num() > 0 ? Args[0].ToLower() : TEXT("hound");
		const ENoclipEnemyKind Kind = KindArg.StartsWith(TEXT("s")) ? ENoclipEnemyKind::Shambler : ENoclipEnemyKind::Hound;
		const float Dist = Args.Num() > 1 ? FCString::Atof(*Args[1]) : 900.f;
		const bool bHunt = Args.ContainsByPredicate([](const FString& A) { return A.Equals(TEXT("hunt"), ESearchCase::IgnoreCase); });
		const FVector Near = Pawn->GetActorLocation() + FRotator(0.f, PC->GetControlRotation().Yaw, 0.f).Vector() * Dist;
		ANoclipEnemy* Enemy = Dir->SpawnEnemy(Kind, &Near);
		if (Enemy)
		{
			// Nasce olhando para o jogador.
			Enemy->SetActorRotation(FRotator(0.f, (Pawn->GetActorLocation() - Enemy->GetActorLocation()).Rotation().Yaw, 0.f));
			if (bHunt)
			{
				Enemy->ForceTarget(Pawn);
			}
		}
	}

	FAutoConsoleCommandWithWorldAndArgs GNoclipSpawnEnemyCommand(
		TEXT("noclip.SpawnEnemy"),
		TEXT("Solta uma entidade na frente do jogador: noclip.SpawnEnemy hound|shambler [distancia cm] [hunt]"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&SpawnEnemyCommand));

	void KillEnemiesCommand(const TArray<FString>& Args, UWorld* World)
	{
		ANoclipGameMode* GM = World ? World->GetAuthGameMode<ANoclipGameMode>() : nullptr;
		if (ANoclipEnemyDirector* Dir = GM ? GM->GetEnemyDirector() : nullptr)
		{
			Dir->DespawnAll();
		}
	}

	FAutoConsoleCommandWithWorldAndArgs GNoclipKillEnemiesCommand(
		TEXT("noclip.KillEnemies"),
		TEXT("Remove todas as entidades do turno."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&KillEnemiesCommand));
}

ANoclipEnemyDirector::ANoclipEnemyDirector()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = false;
	SetCanBeDamaged(false);
}

void ANoclipEnemyDirector::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	DespawnAll();
	Super::EndPlay(EndPlayReason);
}

bool ANoclipEnemyDirector::IsSafeLocation(const FVector& Location) const
{
	const FVector2D P(Location);
	for (const FBox2D& R : SafeRects)
	{
		if (R.bIsValid && R.IsInside(P))
		{
			return true;
		}
	}
	return false;
}

bool ANoclipEnemyDirector::EnsureGrid()
{
	ANoclipLevelGenerator* Gen = nullptr;
	for (TActorIterator<ANoclipLevelGenerator> It(GetWorld()); It; ++It)
	{
		Gen = *It;
		break;
	}
	if (!Gen || Gen->GetGeneratedSeed() == INDEX_NONE)
	{
		return false;
	}
	// Nivel novo ou divisoria mudou de porta: refaz a grade (cabe alguns ms no servidor).
	if (!Grid.IsValid() || GridSeed != Gen->GetGeneratedSeed() || GridLayoutVersion != Gen->GetLayoutVersion())
	{
		BuildGrid(Gen);
	}
	return Grid.IsValid();
}

void ANoclipEnemyDirector::BuildGrid(ANoclipLevelGenerator* Generator)
{
	SafeRects.Reset();
	const FBox2D Extract = Generator->GetExtractionRect();
	if (Extract.bIsValid)
	{
		SafeRects.Add(Extract.ExpandBy(40.f));
	}
	for (TActorIterator<ANoclipElevator> It(GetWorld()); It; ++It)
	{
		const FBox B = It->GetComponentsBoundingBox(true);
		if (B.IsValid)
		{
			SafeRects.Add(FBox2D(FVector2D(B.Min), FVector2D(B.Max)).ExpandBy(40.f));
		}
	}
	Grid.Build(GetWorld(), Generator->GetGeneratedBounds(), Generator->GetActorLocation().Z, NavCellSize, SafeRects);
	GridSeed = Generator->GetGeneratedSeed();
	GridLayoutVersion = Generator->GetLayoutVersion();
}

void ANoclipEnemyDirector::StartShift(int32 ShiftIndex)
{
	DespawnAll();
	Pending.Reset();
	Shift = FMath::Max(ShiftIndex, 1);
	bActive = true;
	bCollapse = false;
	if (!EnsureGrid())
	{
		UE_LOG(LogNoclip, Warning, TEXT("[Entidades] Sem nivel gerado: nenhuma entidade neste turno."));
		return;
	}
	Stream.Initialize(GridSeed * 31 + Shift);
	if (CVarNoclipEnemies.GetValueOnGameThread() == 0)
	{
		UE_LOG(LogNoclip, Log, TEXT("[Entidades] Desligadas (noclip.Enemies 0)."));
		return;
	}

	// Mais entidades a cada turno (1 + 1 no primeiro; ate 3 + 3). Elas chegam aos poucos.
	const int32 Shamblers = FMath::Clamp(1 + (Shift - 1) / 2, 1, 3);
	const int32 Hounds = FMath::Clamp(1 + (Shift - 1) / 2, 1, 3);
	const float Interval = SpawnInterval * (Shift >= 3 ? 0.75f : 1.f);
	double At = GetWorld()->GetTimeSeconds() + FirstSpawnDelay;
	int32 S = 0, H = 0;
	bool bShamblerNext = true;
	while (S < Shamblers || H < Hounds)
	{
		const bool bShambler = (bShamblerNext && S < Shamblers) || H >= Hounds;
		Pending.Add({ bShambler ? ENoclipEnemyKind::Shambler : ENoclipEnemyKind::Hound, At });
		(bShambler ? S : H)++;
		bShamblerNext = !bShamblerNext;
		At += Interval;
	}
	UE_LOG(LogNoclip, Log, TEXT("[Entidades] Turno %d: %d shambler(s) e %d cao(es), o primeiro em %.0f s."), Shift, Shamblers, Hounds, FirstSpawnDelay);
}

void ANoclipEnemyDirector::SetCollapse(bool bInCollapse)
{
	bCollapse = bInCollapse;
	for (ANoclipEnemy* E : Enemies)
	{
		if (IsValid(E))
		{
			E->SetFrenzy(bCollapse);
		}
	}
	if (bCollapse && bActive && CVarNoclipEnemies.GetValueOnGameThread() != 0)
	{
		// O escuro traz mais um cao.
		Pending.Add({ ENoclipEnemyKind::Hound, GetWorld()->GetTimeSeconds() + 4.0 });
	}
}

void ANoclipEnemyDirector::StopShift()
{
	bActive = false;
	bCollapse = false;
	Pending.Reset();
	DespawnAll();
}

void ANoclipEnemyDirector::DespawnAll()
{
	for (ANoclipEnemy* E : Enemies)
	{
		if (!IsValid(E))
		{
			continue;
		}
		if (AController* C = E->GetController())
		{
			C->UnPossess();
			C->Destroy();
		}
		E->Destroy();
	}
	Enemies.Reset();
}

bool ANoclipEnemyDirector::FindSpawnLocation(float Clearance, FVector& OutLocation)
{
	TArray<FVector, TInlineAllocator<8>> Eyes;
	for (TActorIterator<ANoclipCharacter> It(GetWorld()); It; ++It)
	{
		if (IsValid(*It))
		{
			Eyes.Add(It->GetEyeLocation());
		}
	}
	FCollisionQueryParams Params(SCENE_QUERY_STAT(NoclipEnemySpawn), false);
	for (int32 Attempt = 0; Attempt < 80; ++Attempt)
	{
		FVector P;
		if (!Grid.RandomWalkable(Stream, Clearance, FVector::ZeroVector, 0.f, 0.f, P))
		{
			continue;
		}
		// Primeiro tenta longe e fora da vista; depois aceita mais perto.
		const bool bStrict = Attempt < 50;
		const float MinD = bStrict ? MinSpawnDistance : MinSpawnDistance * 0.6f;
		bool bOk = !IsSafeLocation(P);
		for (const FVector& Eye : Eyes)
		{
			if (!bOk)
			{
				break;
			}
			if (FVector::Dist2D(Eye, P) < MinD)
			{
				bOk = false;
			}
			else if (bStrict && !GetWorld()->LineTraceTestByChannel(Eye, P + FVector(0.f, 0.f, 100.f), ECC_Visibility, Params))
			{
				bOk = false; // alguem veria nascer
			}
		}
		if (bOk)
		{
			OutLocation = P;
			return true;
		}
	}
	return false;
}

ANoclipEnemy* ANoclipEnemyDirector::SpawnEnemy(ENoclipEnemyKind Kind, const FVector* Near)
{
	if (!EnsureGrid())
	{
		return nullptr;
	}
	const FNoclipEnemyDef& D = NoclipEnemies::Get(Kind);
	FVector Loc;
	if (Near)
	{
		if (!Grid.ProjectToWalkable(*Near, D.NavClearance, Loc))
		{
			UE_LOG(LogNoclip, Warning, TEXT("[Entidades] Sem lugar livre perto de %s."), *Near->ToCompactString());
			return nullptr;
		}
	}
	else if (!FindSpawnLocation(D.NavClearance, Loc))
	{
		UE_LOG(LogNoclip, Warning, TEXT("[Entidades] Sem lugar para nascer um %s."), *D.Name.ToString());
		return nullptr;
	}
	Loc.Z = Grid.GetFloorZ() + D.CapsuleHalfHeight + 4.f;

	const FTransform Xf(FRotator(0.f, Stream.FRandRange(0.f, 360.f), 0.f), Loc);
	ANoclipEnemy* Enemy = GetWorld()->SpawnActorDeferred<ANoclipEnemy>(ANoclipEnemy::StaticClass(), Xf, this, nullptr,
		ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn);
	if (!Enemy)
	{
		return nullptr;
	}
	Enemy->InitEnemy(Kind, Stream.RandRange(0, FMath::Max(NoclipEnemies::NumVariants(Kind) - 1, 0)), this, Stream.RandRange(1, MAX_int32 - 1));
	Enemy->SetFrenzy(bCollapse);
	Enemy->FinishSpawning(Xf);
	Enemies.Add(Enemy);
	UE_LOG(LogNoclip, Log, TEXT("[Entidades] %s nasceu em %s."), *Enemy->GetName(), *Loc.ToCompactString());
	return Enemy;
}

void ANoclipEnemyDirector::ReportPlayerNoises()
{
	// Voz (GDD: "o colega que nao para de falar vira a isca"). No listen server o host recebe a voz de todos.
	const IOnlineVoicePtr Voice = Online::GetVoiceInterface(GetWorld());
	for (TActorIterator<ANoclipCharacter> It(GetWorld()); It; ++It)
	{
		ANoclipCharacter* P = *It;
		const APlayerState* PS = P ? P->GetPlayerState() : nullptr;
		if (!Voice.IsValid() || !PS)
		{
			continue;
		}
		bool bTalking = false;
		float Amplitude = 0.35f;
		if (P->IsLocallyControlled())
		{
			bTalking = Voice->IsLocalPlayerTalking(0);
		}
		else if (PS->GetUniqueId().IsValid())
		{
			bTalking = Voice->IsRemotePlayerTalking(*PS->GetUniqueId());
			Amplitude = bTalking ? Voice->GetAmplitudeOfRemoteTalker(*PS->GetUniqueId()) : 0.f;
		}
		if (bTalking)
		{
			// Falar normal: ~8 m. Gritar: ate ~20 m.
			const float Radius = 650.f + 1350.f * FMath::Clamp(Amplitude * 3.f, 0.f, 1.f);
			UNoclipNoiseSubsystem::Report(this, P->GetActorLocation(), Radius, P, true);
		}
	}

	for (TActorIterator<ANoclipCharacter> It(GetWorld()); It; ++It)
	{
		ANoclipCharacter* P = *It;
		const UNoclipMoverComponent* Mover = P ? P->GetNoclipMover() : nullptr;
		if (!IsValid(P) || !Mover || !Mover->IsOnGround())
		{
			continue;
		}
		// Passos: correr se ouve de longe; andar, so de perto. Parado nao faz barulho.
		const float Speed = Mover->GetVelocity().Size2D();
		float Radius = 0.f;
		if (Speed > 560.f)
		{
			Radius = 1500.f;
		}
		else if (Speed > 150.f)
		{
			Radius = 450.f * FMath::Clamp(Speed / 420.f, 0.5f, 1.2f);
		}
		if (Radius <= 0.f)
		{
			continue;
		}
		// Arrastando achado: mais barulho.
		if (const UNoclipGrabComponent* Grab = P->GetGrabComponent(); Grab && Grab->IsHolding())
		{
			Radius += Grab->IsHoldingTwoHanded() ? 450.f : 200.f;
		}
		UNoclipNoiseSubsystem::Report(this, P->GetActorLocation(), Radius, P, true);
	}
}

void ANoclipEnemyDirector::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	Enemies.RemoveAll([](const TObjectPtr<ANoclipEnemy>& E) { return !IsValid(E); });

	const double T = GetWorld()->GetTimeSeconds();
	if (bActive)
	{
		// Divisorias mudaram de porta: a grade acompanha (EnsureGrid so refaz se a versao mudou).
		if (HasAuthority() && Grid.IsValid())
		{
			EnsureGrid();
		}
		for (int32 i = 0; i < Pending.Num(); ++i)
		{
			if (T >= Pending[i].At)
			{
				const ENoclipEnemyKind Kind = Pending[i].Kind;
				Pending.RemoveAt(i--);
				SpawnEnemy(Kind);
			}
		}
	}
	if (T >= NextNoiseTime)
	{
		NextNoiseTime = T + 0.2;
		ReportPlayerNoises();
	}

	if (CVarNoclipDebugEnemies.GetValueOnGameThread() > 2 && Grid.IsValid())
	{
		if (APlayerController* PC = GetWorld()->GetFirstPlayerController())
		{
			if (const APawn* Pawn = PC->GetPawn())
			{
				Grid.DebugDraw(GetWorld(), Pawn->GetActorLocation(), 700.f, 45.f);
			}
		}
	}
}
