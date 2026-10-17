// Copyright Fuly Games. All Rights Reserved.

#include "Tests/NoclipSelfTest.h"

#if !UE_BUILD_SHIPPING

#include "Tests/NoclipTestAccess.h"
#include "Audio/NoclipAudioSubsystem.h"
#include "Character/NoclipCharacter.h"
#include "Character/NoclipGrabComponent.h"
#include "Enemy/NoclipEnemyDirector.h"
#include "Game/NoclipGameMode.h"
#include "Game/NoclipGameState.h"
#include "Game/NoclipPlayerController.h"
#include "Game/NoclipPlayerState.h"
#include "UI/NoclipConsoleSubsystem.h"
#include "Voice/NoclipAudioDevices.h"
#include "World/NoclipCorpse.h"
#include "World/NoclipExtractionZone.h"
#include "World/NoclipLevelGenerator.h"
#include "World/NoclipLootActor.h"
#include "World/NoclipLootCatalog.h"
#include "World/NoclipPowerGenerator.h"
#include "Components/PrimitiveComponent.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/InputSettings.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformMisc.h"
#include "Math/RandomStream.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

DEFINE_LOG_CATEGORY_STATIC(LogNoclipTest, Log, All);

namespace NoclipSelfTestPrivate
{
	TWeakPtr<FNoclipSelfTest> GRunning;

	constexpr float StandZ = 94.f; // centro da capsula de quem esta em pe (igual ao FindWalkable)

	int32 GlitchFloor(ENoclipGlitchKind Kind) { return (Kind == ENoclipGlitchKind::FakeWall || Kind == ENoclipGlitchKind::FakeFloor) ? 0 : 1; }

	FString MusicName(uint8 State) { return UEnum::GetValueAsString(static_cast<ENoclipMusicState>(State)).Replace(TEXT("ENoclipMusicState::"), TEXT("")); }
}

// =====================================================================================================
// Estado do teste
// =====================================================================================================

struct FNoclipSelfTest::FImpl
{
	/** Contexto da etapa atual (relogio, sub-passo e onde anotar). */
	struct FCtx
	{
		double Now = 0.0;
		double StepStart = 0.0;
		double StageStart = 0.0;
		int32 Stage = 0;
		FNoclipSelfTestStepResult* R = nullptr;

		float T() const { return static_cast<float>(Now - StepStart); }
		float ST() const { return static_cast<float>(Now - StageStart); }
		void Next() { ++Stage; StageStart = Now; }
		bool Expect(bool bCondition, const FString& What)
		{
			if (!bCondition)
			{
				R->bPassed = false;
				R->Failures.Add(What);
			}
			return bCondition;
		}
		void Fail(const FString& What) { Expect(false, What); }
		void Note(const FString& What) { R->Notes.Add(What); }
	};

	struct FStep
	{
		FString Name;
		float Timeout = 30.f;
		bool bCritical = false;
		TFunction<bool(FCtx&)> Run;
	};

	TWeakObjectPtr<UWorld> World;
	TArray<FStep> Steps;
	int32 Current = -1;
	bool bStepDone = true;
	bool bSkipRest = false;
	FCtx Ctx;

	// Atores criados pela etapa (bonecos de teste, achados): somem no fim dela.
	TArray<TWeakObjectPtr<AActor>> StepActors;
	// Estados de musica vistos durante o teste inteiro.
	TSet<uint8> SeenMusic;
	double LastGlitchTime = -100.0;

	// Rascunho das etapas
	TWeakObjectPtr<ANoclipLootActor> Loot;
	TArray<TWeakObjectPtr<ANoclipCharacter>> Bots;
	float Value0 = 0.f;
	float Value1 = 0.f;
	int32 Int0 = 0;
	int32 Int1 = 0;
	uint32 Mask0 = 0;
	int32 GlitchIndex = INDEX_NONE;

	// ---------------------------------------------------------------- acesso
	UWorld* W() const { return World.Get(); }
	ANoclipGameMode* GM() const { return W() ? W()->GetAuthGameMode<ANoclipGameMode>() : nullptr; }
	ANoclipGameState* GS() const { return W() ? W()->GetGameState<ANoclipGameState>() : nullptr; }
	ANoclipLevelGenerator* Gen() const
	{
		if (UWorld* World_ = W())
		{
			for (TActorIterator<ANoclipLevelGenerator> It(World_); It; ++It)
			{
				return *It;
			}
		}
		return nullptr;
	}
	APlayerController* HostPC() const { return W() ? W()->GetFirstPlayerController() : nullptr; }
	ANoclipCharacter* HostChar() const { return HostPC() ? Cast<ANoclipCharacter>(HostPC()->GetPawn()) : nullptr; }
	bool IsHostLocal() const { return HostPC() && HostPC()->IsLocalController(); }
	ENoclipShiftPhase Phase() const { return GS() ? GS()->GetPhase() : ENoclipShiftPhase::Lobby; }

	TArray<APlayerController*> Players() const
	{
		TArray<APlayerController*> Out;
		if (UWorld* World_ = W())
		{
			for (FConstPlayerControllerIterator It = World_->GetPlayerControllerIterator(); It; ++It)
			{
				if (APlayerController* PC = It->Get())
				{
					Out.Add(PC);
				}
			}
		}
		return Out;
	}

	template <class T>
	int32 CountActors() const
	{
		int32 N = 0;
		if (UWorld* World_ = W())
		{
			for (TActorIterator<T> It(World_); It; ++It)
			{
				N += (IsValid(*It) && !It->IsActorBeingDestroyed()) ? 1 : 0;
			}
		}
		return N;
	}

	FVector Standing(int32 Floor, const FVector2D& XY) const
	{
		return FVector(XY.X, XY.Y, Gen()->GetFloorZ(Floor) + NoclipSelfTestPrivate::StandZ);
	}

	FVector2D ManilaCenter() const { return Gen()->GetExtractionRect().GetCenter(); }

	/** Leva o estagiario para um lugar (pelo Mover, como o noclip faz). */
	void Teleport(ANoclipCharacter* Char, const FVector& Location) const
	{
		if (Char)
		{
			Char->ServerNoclipTo(Location, static_cast<uint8>(ENoclipGlitchKind::ServiceExit));
		}
	}

	/** Lugar livre perto de XY no andar (ou o proprio ponto, se nao achar). */
	FVector Walkable(int32 Floor, const FVector2D& XY, float Radius = 400.f) const
	{
		FVector Out;
		return Gen()->FindWalkable(Floor, XY, Radius, Out) ? Out : Standing(Floor, XY);
	}

	/** Estagiario de teste sem jogador (parado, olhando para +X): serve de colega para carregar e para o gerador. */
	ANoclipCharacter* SpawnBot(const FVector& Location, const FRotator& Rotation = FRotator::ZeroRotator)
	{
		UClass* PawnClass = GM() && HostPC() ? GM()->GetDefaultPawnClassForController(HostPC()) : nullptr;
		if (!PawnClass || !PawnClass->IsChildOf(ANoclipCharacter::StaticClass()))
		{
			PawnClass = ANoclipCharacter::StaticClass();
		}
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		ANoclipCharacter* Bot = W()->SpawnActor<ANoclipCharacter>(PawnClass, Location, Rotation, Params);
		if (Bot)
		{
			StepActors.Add(Bot);
		}
		return Bot;
	}

	/** Achado do catalogo apoiado no chao, centrado em XY. */
	ANoclipLootActor* SpawnCatalogLoot(int32 Index, const FVector2D& XY, int32 Floor)
	{
		const FNoclipLootDef* Def = NoclipLoot::Get(Index);
		UStaticMesh* Mesh = NoclipLoot::LoadMesh(Index);
		if (!Def || !Mesh)
		{
			return nullptr;
		}
		const FBoxSphereBounds B = Mesh->GetBounds();
		const float BottomZ = (B.Origin.Z - B.BoxExtent.Z) * Def->Scale;
		const FTransform Xf(FRotator::ZeroRotator, FVector(XY.X - B.Origin.X * Def->Scale, XY.Y - B.Origin.Y * Def->Scale, Gen()->GetFloorZ(Floor) + 3.f - BottomZ));
		ANoclipLootActor* L = W()->SpawnActorDeferred<ANoclipLootActor>(ANoclipLootActor::StaticClass(), Xf, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
		if (!L)
		{
			return nullptr;
		}
		FRandomStream Stream(Index + 11);
		L->SetupFromCatalog(Index, Stream);
		L->FinishSpawning(Xf);
		StepActors.Add(L);
		return L;
	}

	static UPrimitiveComponent* Body(const ANoclipLootActor* L) { return L ? Cast<UPrimitiveComponent>(L->GetRootComponent()) : nullptr; }

	/** O boneco agarra o achado (mesmo codigo do servidor, sem depender da mira do boneco). */
	bool BotGrab(ANoclipCharacter* Bot, ANoclipLootActor* L) const
	{
		UPrimitiveComponent* Prim = Body(L);
		if (!Bot || !Prim || !Bot->GetGrabComponent())
		{
			return false;
		}
		const FVector Start = Bot->GetEyeLocation();
		const FVector COM = Prim->GetCenterOfMass();
		FHitResult Hit;
		if (!Prim->LineTraceComponent(Hit, Start, COM + (COM - Start) * 0.25f, FCollisionQueryParams(SCENE_QUERY_STAT(NoclipSelfTestGrab), false)))
		{
			Hit = FHitResult(L, Prim, COM, (Start - COM).GetSafeNormal());
		}
		Hit.Distance = FVector::Dist(Start, Hit.ImpactPoint);
		FNoclipTestAccess::GrabFromHit(Bot->GetGrabComponent(), Hit);
		return Bot->GetGrabComponent()->IsHolding();
	}

	/** Indice do catalogo: o achado de equipe que pede mais gente / um achado leve de uma mao. */
	static int32 FindTeamItem()
	{
		int32 Best = INDEX_NONE;
		for (int32 i = 0; i < NoclipLoot::Num(); ++i)
		{
			const FNoclipLootDef* D = NoclipLoot::Get(i);
			if (D->Carriers > 1 && (Best == INDEX_NONE || D->Carriers > NoclipLoot::Get(Best)->Carriers) && NoclipLoot::LoadMesh(i))
			{
				Best = i;
			}
		}
		return Best;
	}

	static int32 FindSoloItem()
	{
		for (int32 i = 0; i < NoclipLoot::Num(); ++i)
		{
			const FNoclipLootDef* D = NoclipLoot::Get(i);
			UStaticMesh* M = D->Carriers == 1 && D->MassKg >= 1.f && D->MassKg < NoclipTools::TwoHandMassKg ? NoclipLoot::LoadMesh(i) : nullptr;
			if (M && M->GetBounds().BoxExtent.GetMax() * D->Scale > 12.f)
			{
				return i;
			}
		}
		return INDEX_NONE;
	}

	void DestroyStepActors()
	{
		for (const TWeakObjectPtr<AActor>& A : StepActors)
		{
			if (ANoclipCharacter* Bot = Cast<ANoclipCharacter>(A.Get()))
			{
				if (Bot->GetGrabComponent())
				{
					Bot->GetGrabComponent()->ForceDrop();
				}
			}
		}
		for (const TWeakObjectPtr<AActor>& A : StepActors)
		{
			if (A.IsValid())
			{
				A->Destroy();
			}
		}
		StepActors.Reset();
		Bots.Reset();
		Loot.Reset();
	}

	/** Entidades desligadas: o teste nao depende de um cao aparecer na hora errada. */
	void QuietEnemies() const
	{
		if (ANoclipGameMode* Mode = GM())
		{
			Mode->GetEnemyDirector()->StopShift();
		}
	}

	void Add(const TCHAR* Name, float Timeout, bool bCritical, TFunction<bool(FCtx&)> Run)
	{
		FStep& S = Steps.AddDefaulted_GetRef();
		S.Name = Name;
		S.Timeout = Timeout;
		S.bCritical = bCritical;
		S.Run = MoveTemp(Run);
	}

	void BuildSteps();
	void AddStartShiftStep(const TCHAR* Name);
	void AddGlitchStep(const TCHAR* Name, ENoclipGlitchKind Kind, bool bRequired);
	void AddLobbyStep(const TCHAR* Name, bool bAfterWipe);
};

// =====================================================================================================
// Etapas
// =====================================================================================================

void FNoclipSelfTest::FImpl::BuildSteps()
{
	using namespace NoclipSelfTestPrivate;

	// ------------------------------------------------------------------ 1. Preparacao
	Add(TEXT("Preparacao (mapa do expediente)"), 20.f, true, [this](FCtx& C)
	{
		if (C.Stage == 0)
		{
			if (!C.Expect(GM() != nullptr, TEXT("O GameMode nao e o ANoclipGameMode: abra o L_Level0_Test (ou rode no host).")) ||
				!C.Expect(GS() != nullptr, TEXT("Sem ANoclipGameState.")) ||
				!C.Expect(Gen() != nullptr, TEXT("Sem ANoclipLevelGenerator no mapa.")) ||
				!C.Expect(HostPC() != nullptr, TEXT("Sem jogador no servidor (o teste roda no host ou standalone).")))
			{
				return true;
			}
			C.Note(FString::Printf(TEXT("%d jogador(es), seed %d, modo %s"), Players().Num(), GS()->GetLevelSeed(), *UEnum::GetValueAsString(Gen()->GetGeneratedMode())));
			if (Phase() == ENoclipShiftPhase::Ended)
			{
				GM()->RequestSkipToLobby(HostPC());
			}
			else if (Phase() == ENoclipShiftPhase::Active || Phase() == ENoclipShiftPhase::Collapse)
			{
				C.Note(TEXT("O turno ja estava em andamento: o teste continua dele."));
			}
			C.Next();
		}
		if (Phase() == ENoclipShiftPhase::Ended)
		{
			return false;
		}
		C.Expect(Gen()->GetGeneratedSeed() == GS()->GetLevelSeed(), FString::Printf(TEXT("Nivel gerado com a seed %d, GameState tem %d."), Gen()->GetGeneratedSeed(), GS()->GetLevelSeed()));
		C.Expect(HostChar() != nullptr, TEXT("O host nao tem estagiario (pawn)."));
		C.Expect(GS()->GetLeader() != nullptr, TEXT("Sem lider do grupo."));
		return true;
	});

	// ------------------------------------------------------------------ 2. Turno 1
	AddStartShiftStep(TEXT("Turno 1 comeca (saguao -> descendo -> expediente)"));

	// ------------------------------------------------------------------ 3. Conteudo do nivel
	Add(TEXT("Nivel do turno (achados, gerador, falhas)"), 10.f, false, [this](FCtx& C)
	{
		ANoclipLevelGenerator* G = Gen();
		C.Expect(G->GetExtractionZone() != nullptr, TEXT("Sem Sala Manila."));
		C.Expect(G->HasSubFloor(), TEXT("Sem subsolo."));
		const ANoclipPowerGenerator* Power = G->GetPowerGenerator();
		if (C.Expect(Power != nullptr, TEXT("Sem gerador de energia no nivel.")))
		{
			C.Expect(G->GetFloorAt(Power->GetActorLocation() + FVector(0.f, 0.f, 50.f)) == 0, TEXT("Gerador fora do Level 0."));
		}
		int32 OnFloor[2] = { 0, 0 }, Team = 0, WrongFloor = 0;
		for (TActorIterator<ANoclipLootActor> It(W()); It; ++It)
		{
			const int32 Floor = G->GetFloorAt(It->GetActorLocation());
			++OnFloor[Floor];
			Team += It->IsTeamCarry() ? 1 : 0;
			const FNoclipLootDef* Def = NoclipLoot::Get(FNoclipTestAccess::CatalogIndex(*It));
			if (Def && !(Def->FloorMask & (1 << Floor)))
			{
				++WrongFloor;
				C.Fail(FString::Printf(TEXT("%s esta no andar %d, mas o catalogo nao deixa."), *Def->Id.ToString(), Floor));
			}
		}
		C.Note(FString::Printf(TEXT("Achados: %d no Level 0, %d no subsolo, %d de equipe."), OnFloor[0], OnFloor[1], Team));
		C.Expect(OnFloor[0] * 2 >= G->LootCount, FString::Printf(TEXT("Poucos achados no Level 0 (%d de %d)."), OnFloor[0], G->LootCount));
		C.Expect(OnFloor[1] > 0, TEXT("Nenhum achado no subsolo."));
		if (Team == 0)
		{
			C.Note(TEXT("Nenhum achado de equipe nesta seed (o teste de equipe spawna um)."));
		}
		int32 Kinds[4] = { 0, 0, 0, 0 };
		for (const FNoclipGlitch& Gl : G->GetGlitches())
		{
			++Kinds[static_cast<int32>(Gl.Kind)];
		}
		C.Note(FString::Printf(TEXT("Falhas: %d paredes, %d pisos, %d saidas, %d paredes do subsolo; %d divisorias que mudam."), Kinds[0], Kinds[1], Kinds[2], Kinds[3], G->GetNumShifters()));
		C.Expect(GS()->IsPowerOut() == false, TEXT("O turno comecou sem energia."));
		C.Expect(GM()->GetEnemyDirector()->EnsureGrid(), TEXT("A grade de navegacao das entidades nao montou."));
		return true;
	});

	// ------------------------------------------------------------------ 4..8. Noclip
	AddGlitchStep(TEXT("Noclip: parede falsa -> subsolo"), ENoclipGlitchKind::FakeWall, true);
	AddGlitchStep(TEXT("Noclip: saida de servico -> perto da Sala Manila"), ENoclipGlitchKind::ServiceExit, true);
	AddGlitchStep(TEXT("Noclip: piso falso -> subsolo"), ENoclipGlitchKind::FakeFloor, true);
	AddGlitchStep(TEXT("Noclip: parede falsa do subsolo -> Level 0"), ENoclipGlitchKind::UpWall, false);

	Add(TEXT("Console: noclip.GoDown / noclip.GoDown 0"), 12.f, false, [this](FCtx& C)
	{
		ANoclipCharacter* Char = HostChar();
		if (!C.Expect(Char != nullptr, TEXT("Host sem estagiario.")))
		{
			return true;
		}
		switch (C.Stage)
		{
		case 0:
			if (C.ST() < 2.7f) { return false; } // espera do noclip anterior
			GEngine->Exec(W(), TEXT("noclip.GoDown"));
			C.Next();
			return false;
		case 1:
			if (Gen()->GetFloorAt(Char->GetActorLocation()) != 1)
			{
				if (C.ST() > 3.f) { C.Fail(TEXT("noclip.GoDown nao levou o host para o subsolo.")); return true; }
				return false;
			}
			GEngine->Exec(W(), TEXT("noclip.GoDown 0"));
			C.Next();
			return false;
		default:
			if (Gen()->GetFloorAt(Char->GetActorLocation()) != 0)
			{
				if (C.ST() > 3.f) { C.Fail(TEXT("noclip.GoDown 0 nao trouxe o host de volta ao Level 0.")); return true; }
				return false;
			}
			LastGlitchTime = C.Now;
			return true;
		}
	});

	// ------------------------------------------------------------------ 9. Salas que mudam
	Add(TEXT("Salas que mudam (noclip.ShiftRooms)"), 15.f, false, [this](FCtx& C)
	{
		ANoclipLevelGenerator* G = Gen();
		switch (C.Stage)
		{
		case 0:
			if (G->GetNumShifters() == 0)
			{
				C.Note(TEXT("Esta seed nao tem divisorias que mudam (pulado)."));
				return true;
			}
			// Todo mundo no subsolo: as divisorias so mudam longe dos estagiarios.
			for (APlayerController* PC : Players())
			{
				GM()->DebugNoclip(PC, true);
			}
			C.Next();
			return false;
		case 1:
			if (C.ST() < 1.f) { return false; }
			Mask0 = GS()->GetLayoutShiftMask();
			Int0 = G->GetLayoutVersion();
			GEngine->Exec(W(), TEXT("noclip.ShiftRooms"));
			C.Next();
			return false;
		default:
		{
			if (C.ST() < 0.3f) { return false; }
			const uint32 Mask = GS()->GetLayoutShiftMask();
			if (!C.Expect(Mask != Mask0, TEXT("Nenhuma divisoria mudou (achados ou estagiarios perto de todas?).")))
			{
				return true;
			}
			C.Expect(G->GetAppliedShiftMask() == Mask, TEXT("O gerador nao aplicou a mascara do GameState."));
			C.Expect(G->GetLayoutVersion() != Int0, TEXT("A versao do layout nao mudou (as entidades nao refazem a grade)."));
			const FCollisionShape Probe = FCollisionShape::MakeSphere(6.f);
			int32 Changed = 0;
			for (int32 i = 0; i < FMath::Min(G->GetNumShifters(), 32); ++i)
			{
				if (((Mask ^ Mask0) >> i) & 1u)
				{
					++Changed;
					const FNoclipShifter& S = G->GetShifter(i);
					const bool bAtB = ((Mask >> i) & 1u) != 0;
					C.Expect(W()->OverlapBlockingTestByChannel(bAtB ? S.CenterB : S.CenterA, FQuat::Identity, ECC_Pawn, Probe), FString::Printf(TEXT("Divisoria %d: a porta nova nao bloqueia."), i));
					C.Expect(!W()->OverlapBlockingTestByChannel(bAtB ? S.CenterA : S.CenterB, FQuat::Identity, ECC_Pawn, Probe), FString::Printf(TEXT("Divisoria %d: a porta antiga continua fechada."), i));
				}
			}
			C.Note(FString::Printf(TEXT("%d divisoria(s) mudaram."), Changed));
			C.Expect(GM()->GetEnemyDirector()->EnsureGrid(), TEXT("A grade das entidades nao refez."));
			for (APlayerController* PC : Players())
			{
				GM()->DebugNoclip(PC, false);
			}
			LastGlitchTime = C.Now;
			return true;
		}
		}
	});

	// ------------------------------------------------------------------ 10. Apagao
	Add(TEXT("Apagao e gerador (QTE, 2 estagiarios)"), 30.f, false, [this](FCtx& C)
	{
		ANoclipGameState* State = GS();
		ANoclipPowerGenerator* Power = Gen()->GetPowerGenerator();
		ANoclipCharacter* Char = HostChar();
		if (!C.Expect(Power != nullptr && Char != nullptr, TEXT("Sem gerador ou sem estagiario do host.")))
		{
			return true;
		}
		const float Gain = FNoclipTestAccess::HitGain(Power);
		switch (C.Stage)
		{
		case 0:
			if (State->IsPowerOut())
			{
				GM()->RestorePower(nullptr);
			}
			GEngine->Exec(W(), TEXT("noclip.Blackout"));
			C.Next();
			return false;
		case 1:
		{
			if (C.ST() < 0.2f) { return false; }
			if (!C.Expect(State->IsPowerOut(), TEXT("noclip.Blackout nao derrubou a energia.")))
			{
				return true;
			}
			C.Expect(FMath::IsNearlyZero(Power->GetProgress()), TEXT("A carga do gerador nao zerou no apagao."));
			// Host de um lado do gerador, um colega de teste do outro.
			const FVector SideA = Power->GetActorTransform().TransformPosition(FVector(0.f, 110.f, 0.f));
			const FVector SideB = Power->GetActorTransform().TransformPosition(FVector(0.f, -110.f, 0.f));
			FVector StandA = Walkable(0, FVector2D(SideA), 90.f), StandB = Walkable(0, FVector2D(SideB), 90.f);
			if (!Power->IsInReach(StandA)) { Swap(StandA, StandB); }
			Teleport(Char, StandA);
			if (ANoclipCharacter* Bot = SpawnBot(StandB))
			{
				Bots.Add(Bot);
			}
			C.Next();
			return false;
		}
		case 2:
			if (C.ST() < 1.2f) { return false; }
			C.Expect(Power->IsInReach(Char->GetActorLocation()), FString::Printf(TEXT("Host nao ficou ao alcance do painel (%s)."), *Char->GetActorLocation().ToCompactString()));
			if (IsHostLocal())
			{
				C.Expect(Char->GetGeneratorInReach() == Power, TEXT("O personagem nao acha o gerador ao alcance (GetGeneratorInReach)."));
				// Primeiro toque abre o QTE.
				FNoclipTestAccess::PressGeneratorQTE(Char, true);
				C.Expect(Char->IsGeneratorQTEActive(), TEXT("O toque no gerador nao abriu o QTE."));
			}
			Int0 = 0;
			C.Next();
			return false;
		case 3:
			// 3 acertos e 1 erro pelo QTE de verdade (ou pelo servidor, se o host nao for local).
			if (C.ST() < 0.3f) { return false; }
			C.StageStart = C.Now;
			Value0 = Power->GetProgress();
			if (IsHostLocal()) { FNoclipTestAccess::PressGeneratorQTE(Char, Int0 < 3); }
			else { Power->ServerAttempt(Char, Int0 < 3); }
			Value1 = Power->GetProgress();
			if (Int0 < 3)
			{
				C.Expect(FMath::IsNearlyEqual(Value1 - Value0, Gain, 0.005f), FString::Printf(TEXT("Acerto %d somou %.3f (esperado %.3f)."), Int0 + 1, Value1 - Value0, Gain));
			}
			else
			{
				C.Expect(Value1 < Value0, FString::Printf(TEXT("O erro nao tirou carga (%.3f -> %.3f)."), Value0, Value1));
				C.Next();
				Int1 = 0;
			}
			++Int0;
			return false;
		case 4:
		{
			// Os dois mexendo: tem que contar 2 no gerador e religar.
			if (!State->IsPowerOut())
			{
				C.Expect(Int1 >= 2, FString::Printf(TEXT("O gerador nao contou 2 estagiarios (maximo %d)."), Int1));
				C.Note(FString::Printf(TEXT("Energia de volta em %.1f s com 2 no gerador."), C.ST()));
				C.Expect(SeenMusic.Contains(static_cast<uint8>(ENoclipMusicState::Tension)) || !UNoclipAudioSubsystem::Get(W()), TEXT("A musica nao foi para Tensao no apagao."));
				return true;
			}
			if (C.ST() > 10.f)
			{
				C.Fail(FString::Printf(TEXT("A energia nao voltou (carga %.0f%%)."), Power->GetProgress() * 100.f));
				return true;
			}
			Int1 = FMath::Max(Int1, Power->GetWorkers());
			const int32 Tick = FMath::FloorToInt(C.ST() / 0.13f);
			if (Tick != Int0)
			{
				Int0 = Tick;
				ANoclipCharacter* Bot = Bots.Num() > 0 ? Bots[0].Get() : nullptr;
				if (Tick % 2 == 0 && Bot) { Power->ServerAttempt(Bot, true); }
				else if (IsHostLocal()) { FNoclipTestAccess::PressGeneratorQTE(Char, true); }
				else { Power->ServerAttempt(Char, true); }
			}
			return false;
		}
		}
		return true;
	});

	// ------------------------------------------------------------------ 11. Carregar em equipe
	Add(TEXT("Carregar em equipe (arrastar, levantar, dividir o peso)"), 40.f, false, [this](FCtx& C)
	{
		ANoclipLootActor* L = Loot.Get();
		UPrimitiveComponent* Prim = Body(L);
		auto BotAt = [this](int32 i) { return Bots.IsValidIndex(i) ? Bots[i].Get() : nullptr; };
		auto Carriers = [L]() { return L ? L->GetNumCarriers() : 0; };
		const UNoclipGrabComponent* Defaults = GetDefault<UNoclipGrabComponent>();
		if (C.Stage > 0 && !C.Expect(L != nullptr && Prim != nullptr, TEXT("O achado de teste sumiu.")))
		{
			return true;
		}

		switch (C.Stage)
		{
		case 0:
		{
			const int32 Index = FindTeamItem();
			if (!C.Expect(Index != INDEX_NONE, TEXT("Nenhum achado de equipe no catalogo.")))
			{
				return true;
			}
			// Tira o host do caminho e poe o achado no meio da Sala Manila (sala aberta).
			if (ANoclipCharacter* Char = HostChar())
			{
				Teleport(Char, Walkable(1, ManilaCenter(), 800.f));
			}
			Loot = SpawnCatalogLoot(Index, ManilaCenter(), 0);
			if (!C.Expect(Loot.IsValid(), TEXT("Nao deu para spawnar o achado de equipe.")))
			{
				return true;
			}
			C.Note(FString::Printf(TEXT("Achado: %s (%.0f kg, %d carregadores)."), *NoclipLoot::Get(Index)->Id.ToString(), Loot->GetMassKg(), Loot->GetRequiredCarriers()));
			C.Next();
			return false;
		}
		case 1:
		{
			if (!C.Expect(Prim != nullptr, TEXT("O achado de equipe sumiu."))) { return true; }
			if (C.ST() < 1.2f) { return false; }
			// Equipe de teste: precisa + 2 (o ultimo testa o limite de gente).
			const int32 Required = L->GetRequiredCarriers();
			const int32 N = FMath::Min(Required + 2, 5);
			const FVector Center = Prim->Bounds.Origin;
			const float Back = Prim->Bounds.BoxExtent.X + 75.f;
			for (int32 i = 0; i < N; ++i)
			{
				const FVector2D XY(Center.X - Back, Center.Y + (i - (N - 1) * 0.5f) * 70.f);
				if (ANoclipCharacter* Bot = SpawnBot(Standing(0, XY)))
				{
					Bots.Add(Bot);
				}
			}
			C.Expect(Bots.Num() == N, TEXT("Nao spawnou a equipe de teste inteira."));
			C.Next();
			return false;
		}
		case 2:
		{
			if (C.ST() < 0.5f) { return false; }
			// Um so: arrasta.
			ANoclipCharacter* First = BotAt(0);
			if (!C.Expect(BotGrab(First, L), TEXT("O primeiro carregador nao conseguiu agarrar.")))
			{
				return true;
			}
			UNoclipGrabComponent* Grab = First->GetGrabComponent();
			C.Expect(Carriers() == 1, FString::Printf(TEXT("Carregadores = %d (esperado 1)."), Carriers()));
			C.Expect(!L->IsLifted(), TEXT("Levantou com 1 so."));
			C.Expect(Grab->IsDragging(), TEXT("Com gente faltando, nao esta arrastando."));
			C.Expect(FMath::IsNearlyEqual(Grab->GetCarrySpeedScale(), Grab->DragSpeedScale, 0.01f), FString::Printf(TEXT("Velocidade arrastando %.2f (esperado %.2f)."), Grab->GetCarrySpeedScale(), Grab->DragSpeedScale));
			C.Expect(L->GetLeadCarrier() == First && FNoclipTestAccess::IsLead(Grab), TEXT("O primeiro nao virou o lider da fisica."));
			Value0 = Prim->GetCenterOfMass().Z;
			C.Next();
			return false;
		}
		case 3:
		{
			if (C.ST() < 1.5f) { return false; }
			C.Expect(Prim->GetCenterOfMass().Z < Value0 + 8.f, FString::Printf(TEXT("Arrastando, o achado subiu %.0f cm (devia ficar no chao)."), Prim->GetCenterOfMass().Z - Value0));
			// Completa a equipe: levanta e divide o peso.
			const int32 Required = L->GetRequiredCarriers();
			for (int32 i = 1; i < Required; ++i)
			{
				C.Expect(BotGrab(BotAt(i), L), FString::Printf(TEXT("O carregador %d nao conseguiu entrar na equipe."), i + 1));
			}
			C.Expect(Carriers() == Required, FString::Printf(TEXT("Carregadores = %d (esperado %d)."), Carriers(), Required));
			C.Expect(L->IsLifted(), TEXT("Com a equipe completa, nao levantou."));
			const float Expected = FNoclipTestAccess::ExpectedCarrySpeed(Defaults, Required, Required, L->GetMassKg());
			for (int32 i = 0; i < Required; ++i)
			{
				const UNoclipGrabComponent* Grab = BotAt(i) ? BotAt(i)->GetGrabComponent() : nullptr;
				C.Expect(Grab && !Grab->IsDragging(), FString::Printf(TEXT("Carregador %d ainda arrastando."), i + 1));
				C.Expect(Grab && FMath::IsNearlyEqual(Grab->GetCarrySpeedScale(), Expected, 0.01f), FString::Printf(TEXT("Carregador %d: velocidade %.2f (esperado %.2f, %.0f kg cada)."), i + 1, Grab ? Grab->GetCarrySpeedScale() : -1.f, Expected, L->GetMassKg() / Required));
			}
			Value1 = Expected;
			C.Next();
			return false;
		}
		case 4:
		{
			if (C.ST() < 2.5f) { return false; }
			const float Rise = Prim->GetCenterOfMass().Z - Value0;
			C.Expect(Rise > 6.f, FString::Printf(TEXT("Com a equipe completa, o achado nao saiu do chao (subiu %.0f cm)."), Rise));
			C.Note(FString::Printf(TEXT("Levantado %.0f cm."), Rise));
			// Mais um ajuda (peso menor para cada) e o seguinte ja nao cabe.
			const int32 Required = L->GetRequiredCarriers();
			const int32 Cap = FMath::Min(Required + 1, 4);
			if (Required + 1 <= Cap && BotAt(Required))
			{
				C.Expect(BotGrab(BotAt(Required), L), TEXT("Um ajudante a mais nao conseguiu entrar."));
				const float Expected = FNoclipTestAccess::ExpectedCarrySpeed(Defaults, Required + 1, Required, L->GetMassKg());
				C.Expect(Expected >= Value1 - 0.001f && FMath::IsNearlyEqual(BotAt(0)->GetGrabComponent()->GetCarrySpeedScale(), Expected, 0.01f),
					FString::Printf(TEXT("Com %d, velocidade %.2f (esperado %.2f)."), Required + 1, BotAt(0)->GetGrabComponent()->GetCarrySpeedScale(), Expected));
			}
			if (ANoclipCharacter* Extra = BotAt(Cap))
			{
				C.Expect(!BotGrab(Extra, L), FString::Printf(TEXT("Entrou mais gente que o limite (%d)."), Cap));
				C.Expect(Carriers() == Cap, FString::Printf(TEXT("Carregadores = %d (limite %d)."), Carriers(), Cap));
			}
			// Sem arremesso: quem "arremessa" so solta, e o achado nao sai voando.
			ANoclipCharacter* Thrower = BotAt(1);
			if (!C.Expect(Thrower != nullptr, TEXT("Sem o segundo carregador.")))
			{
				return true;
			}
			const int32 Before = Carriers();
			FNoclipTestAccess::ServerThrow(Thrower->GetGrabComponent(), 1.f);
			C.Expect(!Thrower->GetGrabComponent()->IsHolding(), TEXT("Arremesso de achado de equipe nao soltou."));
			C.Expect(Carriers() == Before - 1, TEXT("Quem soltou continuou contando como carregador."));
			C.Expect(Prim->GetPhysicsLinearVelocity().Size() < 400.f, FString::Printf(TEXT("Achado de equipe foi arremessado (%.0f cm/s)."), Prim->GetPhysicsLinearVelocity().Size()));
			C.Next();
			return false;
		}
		case 5:
		{
			if (C.ST() < 0.4f) { return false; }
			// O lider solta: o proximo assume a fisica, o peso se redistribui.
			ANoclipCharacter* Lead = L->GetLeadCarrier();
			if (!C.Expect(Lead != nullptr, TEXT("A equipe ficou sem lider.")))
			{
				return true;
			}
			const int32 Before = Carriers();
			Lead->GetGrabComponent()->ForceDrop();
			ANoclipCharacter* NewLead = L->GetLeadCarrier();
			C.Expect(Carriers() == Before - 1, TEXT("O lider soltou e a contagem nao baixou."));
			C.Expect(NewLead && NewLead != Lead && FNoclipTestAccess::IsLead(NewLead->GetGrabComponent()), TEXT("Ninguem assumiu a fisica quando o lider soltou."));
			const int32 Required = L->GetRequiredCarriers();
			C.Expect(L->IsLifted() == (Carriers() >= Required), TEXT("Levantado/arrastando nao bate com a quantidade de gente."));
			const float Expected = FNoclipTestAccess::ExpectedCarrySpeed(Defaults, Carriers(), Required, L->GetMassKg());
			C.Expect(NewLead && FMath::IsNearlyEqual(NewLead->GetGrabComponent()->GetCarrySpeedScale(), Expected, 0.01f), TEXT("O peso nao se redistribuiu depois que o lider soltou."));
			Value0 = Prim->GetCenterOfMass().Z;
			C.Next();
			return false;
		}
		case 6:
		{
			if (C.ST() < 1.f) { return false; }
			C.Expect(L->GetNumCarriers() > 0 && L->GetLeadCarrier() != nullptr, TEXT("O achado caiu da mao da equipe depois da troca de lider."));
			for (const TWeakObjectPtr<ANoclipCharacter>& Bot : Bots)
			{
				if (Bot.IsValid())
				{
					Bot->GetGrabComponent()->ForceDrop();
				}
			}
			C.Expect(Carriers() == 0 && L->GetLeadCarrier() == nullptr, TEXT("Todos soltaram e ainda tem carregador."));
			return true;
		}
		}
		return true;
	});

	// ------------------------------------------------------------------ 12. Achado comum + mira de verdade
	Add(TEXT("Agarrar sozinho, um por vez e arremessar (mira real)"), 20.f, false, [this](FCtx& C)
	{
		ANoclipLootActor* L = Loot.Get();
		UPrimitiveComponent* Prim = Body(L);
		auto BotAt = [this](int32 i) { return Bots.IsValidIndex(i) ? Bots[i].Get() : nullptr; };
		ANoclipCharacter* Char = HostChar();
		if (C.Stage > 0 && !C.Expect(L != nullptr && Prim != nullptr, TEXT("O achado de teste sumiu.")))
		{
			return true;
		}
		if (C.Stage >= 3 && !C.Expect(Char != nullptr, TEXT("Host sem estagiario.")))
		{
			return true;
		}
		switch (C.Stage)
		{
		case 0:
		{
			const int32 Index = FindSoloItem();
			if (!C.Expect(Index != INDEX_NONE, TEXT("Nenhum achado leve de uma mao no catalogo.")))
			{
				return true;
			}
			Loot = SpawnCatalogLoot(Index, ManilaCenter(), 0);
			C.Expect(Loot.IsValid(), TEXT("Nao deu para spawnar o achado."));
			C.Note(FString::Printf(TEXT("Achado: %s."), *NoclipLoot::Get(Index)->Id.ToString()));
			C.Next();
			return !Loot.IsValid();
		}
		case 1:
		{
			if (C.ST() < 1.f) { return false; }
			const FVector Center = Prim->Bounds.Origin;
			for (int32 i = 0; i < 2; ++i)
			{
				if (ANoclipCharacter* Bot = SpawnBot(Standing(0, FVector2D(Center.X - 110.f, Center.Y + (i ? 45.f : -45.f)))))
				{
					Bots.Add(Bot);
				}
			}
			C.Next();
			return false;
		}
		case 2:
		{
			if (C.ST() < 0.4f) { return false; }
			if (!C.Expect(BotAt(0) && BotAt(1), TEXT("Os bonecos de teste nao spawnaram.")))
			{
				return true;
			}
			C.Expect(BotGrab(BotAt(0), L), TEXT("Nao conseguiu agarrar o achado leve."));
			C.Expect(!L->IsTeamCarry() && FMath::IsNearlyEqual(BotAt(0)->GetGrabComponent()->GetCarrySpeedScale(),
				FNoclipTestAccess::ExpectedCarrySpeed(GetDefault<UNoclipGrabComponent>(), 1, 1, L->GetMassKg()), 0.01f), TEXT("Velocidade com achado leve errada."));
			C.Expect(!BotGrab(BotAt(1), L), TEXT("Dois agarraram o mesmo achado comum (deveria ser um por vez)."));
			C.Expect(L->GetNumCarriers() == 1, FString::Printf(TEXT("Achado comum com %d carregadores."), L->GetNumCarriers()));
			BotAt(0)->GetGrabComponent()->ForceDrop();
			C.Expect(L->GetNumCarriers() == 0, TEXT("Soltou e o achado continua contando carregador."));
			for (const TWeakObjectPtr<ANoclipCharacter>& Bot : Bots)
			{
				if (Bot.IsValid()) { Bot->Destroy(); }
			}
			Bots.Reset();
			if (!IsHostLocal() || !Char)
			{
				C.Note(TEXT("Host nao e local: mira e arremesso pulados."));
				return true;
			}
			// Host na frente do achado.
			Teleport(Char, Standing(0, FVector2D(Prim->Bounds.Origin.X - 140.f, Prim->Bounds.Origin.Y)));
			C.Next();
			return false;
		}
		case 3:
			if (C.ST() < 0.8f) { return false; }
			HostPC()->SetControlRotation((Prim->GetCenterOfMass() - Char->GetEyeLocation()).Rotation());
			C.Next();
			return false;
		case 4:
			if (C.ST() < 0.5f) { return false; }
			Char->GetGrabComponent()->ToggleGrab();
			C.Next();
			return false;
		case 5:
			if (C.ST() < 0.4f) { return false; }
			if (!C.Expect(Char->GetGrabComponent()->GetHeldActor() == L, TEXT("Mirando no achado e apertando agarrar, o host nao agarrou.")))
			{
				return true;
			}
			// Olha para a frente (um pouco para cima) antes de arremessar: mirando no chao, o achado bate no piso.
			HostPC()->SetControlRotation(FRotator(8.f, HostPC()->GetControlRotation().Yaw, 0.f));
			C.Next();
			return false;
		case 6:
			if (C.ST() < 0.4f) { return false; }
			Char->GetGrabComponent()->BeginThrowCharge();
			C.Next();
			return false;
		case 7:
			if (C.ST() < 0.6f) { return false; }
			Char->GetGrabComponent()->ReleaseThrow();
			Value0 = 0.f;
			C.Next();
			return false;
		default:
			// Maior velocidade nos primeiros quadros (depois ele bate em algo e para).
			Value0 = FMath::Max(Value0, static_cast<float>(Prim->GetPhysicsLinearVelocity().Size()));
			if (C.ST() < 0.25f) { return false; }
			C.Expect(!Char->GetGrabComponent()->IsHolding(), TEXT("Arremessou e continuou segurando."));
			C.Expect(Value0 > 250.f, FString::Printf(TEXT("O arremesso nao empurrou o achado (pico %.0f cm/s)."), Value0));
			C.Note(FString::Printf(TEXT("Arremesso: pico de %.0f cm/s."), Value0));
			return true;
		}
	});

	// ------------------------------------------------------------------ 13. Extracao
	Add(TEXT("Extracao pela Sala Manila"), 30.f, false, [this](FCtx& C)
	{
		if (C.Stage == 0)
		{
			if (!C.Expect(Phase() == ENoclipShiftPhase::Active, TEXT("Fora do expediente: nao da para extrair.")))
			{
				return true;
			}
			GM()->bDebugExtractNow = true; // equipe e ate 6 achados para dentro da sala
			C.Next();
			return false;
		}
		if (Phase() != ENoclipShiftPhase::Ended)
		{
			return false;
		}
		const FNoclipShiftResult& Res = GS()->GetLastResult();
		C.Note(FString::Printf(TEXT("Extraido %d (%d achados) / quota %d -> %s, +%d."), Res.Extracted, Res.ItemsExtracted, Res.Quota, Res.bQuotaMet ? TEXT("quota batida") : TEXT("demitidos"), Res.MoneyEarned));
		C.Expect(Res.bExtractedEarly, TEXT("O turno nao terminou pelo elevador."));
		C.Expect(!Res.bTeamWiped && Res.PlayersDied == 0, TEXT("Resultado marcou mortes sem ninguem morrer."));
		C.Expect(Res.PlayersExtracted == Res.PlayersTotal, FString::Printf(TEXT("%d de %d estagiarios na sala."), Res.PlayersExtracted, Res.PlayersTotal));
		C.Expect(Res.PlayersLeftBehind == 0, TEXT("Alguem ficou para tras numa extracao pelo elevador."));
		C.Expect(Res.ItemsExtracted > 0 && Res.Extracted > 0, TEXT("Nenhum achado contou na extracao."));
		C.Expect(Res.bQuotaMet == (Res.Extracted >= Res.Quota) && Res.bFired == !Res.bQuotaMet, TEXT("Quota batida / demissao nao bate com o valor."));
		C.Expect(Res.MoneyEarned == (Res.bQuotaMet ? Res.Extracted - Res.Quota + Res.BonusPaid : 0), TEXT("Dinheiro ganho nao bate com a conta."));
		return true;
	});

	AddLobbyStep(TEXT("Volta ao saguao (limpa o turno)"), false);

	// ------------------------------------------------------------------ 15..18. Morte
	AddStartShiftStep(TEXT("Turno 2 comeca"));

	Add(TEXT("Morte e espectador"), 10.f, false, [this](FCtx& C)
	{
		if (C.Stage == 0)
		{
			Int0 = 0;
			for (APlayerController* PC : Players())
			{
				if (ANoclipCharacter* Char = Cast<ANoclipCharacter>(PC->GetPawn()))
				{
					Char->ApplyHurt(1000.f, Char->GetActorLocation() + FVector(100.f, 0.f, 0.f), ENoclipDeathCause::Hound);
					++Int0;
				}
			}
			C.Expect(Int0 > 0, TEXT("Ninguem de pe para morrer."));
			C.Next();
			return Int0 == 0;
		}
		int32 Dead = 0;
		for (APlayerController* PC : Players())
		{
			const ANoclipPlayerState* PS = PC->GetPlayerState<ANoclipPlayerState>();
			Dead += PS && PS->IsDead() ? 1 : 0;
		}
		if (C.Stage == 1)
		{
			if (Dead < Int0)
			{
				return false;
			}
			for (APlayerController* PC : Players())
			{
				const ANoclipPlayerState* PS = PC->GetPlayerState<ANoclipPlayerState>();
				C.Expect(PC->GetPawn() == nullptr, FString::Printf(TEXT("%s morreu e continua com pawn."), *GetNameSafe(PS)));
				C.Expect(PS && PS->GetDeathCause() == ENoclipDeathCause::Hound, TEXT("Causa da morte nao foi guardada."));
				if (const ANoclipPlayerController* NPC = Cast<ANoclipPlayerController>(PC))
				{
					C.Expect(NPC->IsSpectating(), FString::Printf(TEXT("%s morto e nao esta assistindo."), *GetNameSafe(PS)));
				}
			}
			C.Expect(CountActors<ANoclipCorpse>() >= Int0, FString::Printf(TEXT("Corpos no chao: %d (esperado %d)."), CountActors<ANoclipCorpse>(), Int0));
			C.Next();
			return false;
		}
		// Musica de morto (so em maquina com tela).
		if (C.ST() < 1.5f && Phase() == ENoclipShiftPhase::Active)
		{
			return false;
		}
		C.Expect(SeenMusic.Contains(static_cast<uint8>(ENoclipMusicState::Dead)) || !UNoclipAudioSubsystem::Get(W()) || !IsHostLocal(), TEXT("A musica nao foi para Morto."));
		return true;
	});

	Add(TEXT("Equipe inteira morta: o turno acaba sozinho"), 15.f, false, [this](FCtx& C)
	{
		if (Phase() != ENoclipShiftPhase::Ended)
		{
			return false;
		}
		const FNoclipShiftResult& Res = GS()->GetLastResult();
		C.Note(FString::Printf(TEXT("Turno acabou %.1f s depois (TeamWipeDelay %.0f s)."), C.T(), GM()->TeamWipeDelay));
		C.Expect(Res.bTeamWiped, TEXT("O resultado nao marcou a equipe inteira morta."));
		C.Expect(Res.PlayersDied == Players().Num(), FString::Printf(TEXT("Mortos no resultado: %d de %d."), Res.PlayersDied, Players().Num()));
		C.Expect(!Res.bExtractedEarly, TEXT("Wipe contou como extracao."));
		return true;
	});

	AddLobbyStep(TEXT("Saguao depois do wipe (todo mundo volta vivo)"), true);

	// ------------------------------------------------------------------ 19. Som, console e dispositivos
	Add(TEXT("Som, console e dispositivos de audio"), 5.f, false, [this](FCtx& C)
	{
		UGameInstance* GI = W()->GetGameInstance();
		C.Expect(GI && GI->GetSubsystem<UNoclipConsoleSubsystem>() != nullptr, TEXT("Subsistema do console nao existe (o console nao abre com a UI na frente)."));
		C.Expect(GetDefault<UInputSettings>()->ConsoleKeys.Num() > 0, TEXT("Nenhuma tecla de console configurada (DefaultInput.ini)."));
		if (!IsHostLocal() || !UNoclipAudioSubsystem::Get(W()))
		{
			C.Note(TEXT("Sem audio local (servidor dedicado): musica nao conferida."));
			return true;
		}
		FString Seen;
		for (const uint8 S : SeenMusic)
		{
			Seen += MusicName(S) + TEXT(" ");
		}
		C.Note(TEXT("Musicas que tocaram: ") + Seen);
		C.Expect(SeenMusic.Contains(static_cast<uint8>(ENoclipMusicState::Lobby)) || SeenMusic.Contains(static_cast<uint8>(ENoclipMusicState::Descending)), TEXT("Nao tocou a musica do saguao/elevador."));
		C.Expect(SeenMusic.Contains(static_cast<uint8>(ENoclipMusicState::Explore)) || SeenMusic.Contains(static_cast<uint8>(ENoclipMusicState::SafeRoom)), TEXT("Nao tocou a musica do expediente."));
		C.Expect(SeenMusic.Contains(static_cast<uint8>(ENoclipMusicState::ReportGood)) || SeenMusic.Contains(static_cast<uint8>(ENoclipMusicState::ReportFired)), TEXT("Nao tocou a musica do relatorio."));
		if (const UNoclipAudioDeviceSubsystem* Devices = UNoclipAudioDeviceSubsystem::Get(W()))
		{
			C.Note(FString::Printf(TEXT("Dispositivos: %d saidas, %d microfones."), Devices->GetOutputDevices().Num(), Devices->GetInputDevices().Num()));
			C.Expect(Devices->GetOutputDevices().Num() > 0, TEXT("Nenhuma saida de audio listada (a lista do Audio Mixer nunca chegou: o fone salvo nao e aplicado)."));
		}
		else
		{
			C.Fail(TEXT("Subsistema de dispositivos de audio nao existe."));
		}
		return true;
	});
}

void FNoclipSelfTest::FImpl::AddStartShiftStep(const TCHAR* Name)
{
	Add(Name, 30.f, true, [this](FCtx& C)
	{
		if (C.Stage == 0)
		{
			if (Phase() == ENoclipShiftPhase::Lobby)
			{
				C.Expect(GM()->RequestStartShift(nullptr), TEXT("RequestStartShift recusou no saguao."));
			}
			C.Next();
			return false;
		}
		if (Phase() != ENoclipShiftPhase::Active)
		{
			return false;
		}
		QuietEnemies();
		C.Note(FString::Printf(TEXT("Turno %d, quota %d, seed %d."), GS()->GetShiftIndex(), GS()->GetQuota(), GS()->GetLevelSeed()));
		C.Expect(Gen()->GetGeneratedSeed() == GS()->GetLevelSeed(), TEXT("O nivel nao foi gerado com a seed do turno."));
		C.Expect(Gen()->GetPowerGenerator() != nullptr, TEXT("Sem gerador de energia no turno."));
		C.Expect(GS()->GetLayoutShiftMask() == 0, TEXT("Divisorias nao voltaram ao estado inicial."));
		for (APlayerController* PC : Players())
		{
			C.Expect(PC->GetPawn() != nullptr, FString::Printf(TEXT("%s sem estagiario no comeco do turno."), *GetNameSafe(PC->PlayerState)));
		}
		LastGlitchTime = C.Now;
		return true;
	});
}

void FNoclipSelfTest::FImpl::AddGlitchStep(const TCHAR* Name, ENoclipGlitchKind Kind, bool bRequired)
{
	Add(Name, 15.f, false, [this, Kind, bRequired](FCtx& C)
	{
		using namespace NoclipSelfTestPrivate;
		ANoclipLevelGenerator* G = Gen();
		ANoclipCharacter* Char = HostChar();
		if (!C.Expect(Char != nullptr, TEXT("Host sem estagiario.")))
		{
			return true;
		}
		const int32 From = GlitchFloor(Kind);
		switch (C.Stage)
		{
		case 0:
		{
			GlitchIndex = INDEX_NONE;
			float Best = BIG_NUMBER;
			for (int32 i = 0; i < G->GetGlitches().Num(); ++i)
			{
				const float D = FVector::Dist(G->GetGlitches()[i].Box.GetCenter(), Char->GetActorLocation());
				if (G->GetGlitches()[i].Kind == Kind && D < Best)
				{
					Best = D;
					GlitchIndex = i;
				}
			}
			if (GlitchIndex == INDEX_NONE)
			{
				if (bRequired) { C.Fail(TEXT("O nivel nao tem esta falha.")); }
				else { C.Note(TEXT("O nivel nao tem esta falha (pulado).")); }
				return true;
			}
			// Comeca no andar certo.
			if (G->GetFloorAt(Char->GetActorLocation()) != From)
			{
				Teleport(Char, Walkable(From, FVector2D(G->GetGlitches()[GlitchIndex].Box.GetCenter()), 1200.f));
			}
			C.Next();
			return false;
		}
		case 1:
		{
			// Espera o intervalo entre dois noclips do GameMode (2,5 s).
			if (C.Now - LastGlitchTime < 2.7 || C.ST() < 0.4f || G->GetFloorAt(Char->GetActorLocation()) != From)
			{
				return false;
			}
			const FBox Box = G->GetGlitches()[GlitchIndex].Box;
			const FVector Into(Box.GetCenter().X, Box.GetCenter().Y, FMath::Clamp(G->GetFloorZ(From) + StandZ, Box.Min.Z + 5.f, Box.Max.Z - 5.f));
			Teleport(Char, Into);
			C.Next();
			return false;
		}
		default:
		{
			const int32 To = 1 - From;
			if (G->GetFloorAt(Char->GetActorLocation()) != To)
			{
				if (C.ST() > 4.f)
				{
					C.Fail(FString::Printf(TEXT("Entrou na falha e nada aconteceu (esta em %s, andar %d)."), *Char->GetActorLocation().ToCompactString(), G->GetFloorAt(Char->GetActorLocation())));
					return true;
				}
				return false;
			}
			LastGlitchTime = C.Now;
			C.Note(FString::Printf(TEXT("Atravessou em %.1f s -> %s."), C.ST(), *Char->GetActorLocation().ToCompactString()));
			C.Expect(G->FindGlitchAt(Char->GetActorLocation()) == INDEX_NONE, TEXT("Caiu dentro de outra falha."));
			if (IsHostLocal())
			{
				C.Expect(Char->GetLastNoclipKind() == static_cast<uint8>(Kind) && Char->GetNoclipAge() < 5.f, TEXT("O dono nao recebeu o efeito do noclip (chiado/imagem)."));
			}
			if (Kind == ENoclipGlitchKind::ServiceExit)
			{
				const float D = FVector2D::Distance(FVector2D(Char->GetActorLocation()), ManilaCenter());
				C.Expect(D <= G->GetExtractionRect().GetExtent().GetMax() + 1300.f, FString::Printf(TEXT("Saiu a %.0f cm da Sala Manila (devia sair do lado)."), D));
			}
			return true;
		}
		}
	});
}

void FNoclipSelfTest::FImpl::AddLobbyStep(const TCHAR* Name, bool bAfterWipe)
{
	Add(Name, 15.f, true, [this, bAfterWipe](FCtx& C)
	{
		if (C.Stage == 0)
		{
			if (!C.Expect(Phase() == ENoclipShiftPhase::Ended, TEXT("O turno nao esta no relatorio.")))
			{
				return true;
			}
			// Fica um pouco no relatorio (a musica avalia a cada 0,25 s) e depois o lider pula.
			if (C.T() < 2.f)
			{
				return false;
			}
			C.Expect(GM()->RequestSkipToLobby(HostPC()), TEXT("O lider nao conseguiu pular para o saguao."));
			C.Next();
			return false;
		}
		if (Phase() != ENoclipShiftPhase::Lobby || C.ST() < 0.3f)
		{
			return false;
		}
		C.Expect(Gen()->GetPowerGenerator() == nullptr, TEXT("O gerador de energia ficou no saguao."));
		C.Expect(CountActors<ANoclipLootActor>() == 0, FString::Printf(TEXT("Sobraram %d achados no saguao."), CountActors<ANoclipLootActor>()));
		C.Expect(CountActors<ANoclipCorpse>() == 0, TEXT("Sobraram corpos no saguao."));
		C.Expect(!GS()->IsPowerOut(), TEXT("O saguao ficou sem energia."));
		for (APlayerController* PC : Players())
		{
			const ANoclipPlayerState* PS = PC->GetPlayerState<ANoclipPlayerState>();
			C.Expect(PC->GetPawn() != nullptr, FString::Printf(TEXT("%s sem estagiario no saguao."), *GetNameSafe(PS)));
			C.Expect(PS && !PS->IsDead(), FString::Printf(TEXT("%s continua morto no saguao."), *GetNameSafe(PS)));
		}
		if (bAfterWipe)
		{
			C.Note(FString::Printf(TEXT("Caixa da equipe: %d, turno %d."), GS()->GetMoney(), GS()->GetShiftIndex()));
		}
		return true;
	});
}

// =====================================================================================================
// Execucao
// =====================================================================================================

TSharedPtr<FNoclipSelfTest> FNoclipSelfTest::Start(UWorld* World, bool bInQuitWhenDone)
{
	if (NoclipSelfTestPrivate::GRunning.IsValid() || !World)
	{
		return nullptr;
	}
	TSharedPtr<FNoclipSelfTest> Test = MakeShareable(new FNoclipSelfTest());
	Test->Impl = MakeUnique<FImpl>();
	Test->Impl->World = World;
	Test->Impl->BuildSteps();
	Test->bQuitWhenDone = bInQuitWhenDone;
	Test->TickerHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateSP(Test.ToSharedRef(), &FNoclipSelfTest::Tick));
	NoclipSelfTestPrivate::GRunning = Test;
	// O ticker so guarda referencia fraca: o proprio teste se mantem vivo ate terminar.
	static TArray<TSharedPtr<FNoclipSelfTest>> Alive;
	Alive.RemoveAll([](const TSharedPtr<FNoclipSelfTest>& T) { return !T.IsValid() || T->IsFinished(); });
	Alive.Add(Test);

	UE_LOG(LogNoclipTest, Display, TEXT("==== NOCLIP SelfTest: %d etapas ===="), Test->Impl->Steps.Num());
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 6.f, FColor::Cyan, FString::Printf(TEXT("NOCLIP SelfTest comecou (%d etapas, ~2 min). Nao mexa no jogo."), Test->Impl->Steps.Num()));
	}
	return Test;
}

TSharedPtr<FNoclipSelfTest> FNoclipSelfTest::GetRunning()
{
	return NoclipSelfTestPrivate::GRunning.Pin();
}

FNoclipSelfTest::~FNoclipSelfTest()
{
	if (TickerHandle.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(TickerHandle);
	}
}

bool FNoclipSelfTest::HasFailures() const
{
	return Results.ContainsByPredicate([](const FNoclipSelfTestStepResult& R) { return !R.bPassed || R.bSkipped; });
}

void FNoclipSelfTest::Abort(const FString& Why)
{
	if (bFinished)
	{
		return;
	}
	FNoclipSelfTestStepResult& R = Results.AddDefaulted_GetRef();
	R.Name = TEXT("Teste interrompido");
	R.bPassed = false;
	R.Failures.Add(Why);
	Finish();
}

bool FNoclipSelfTest::Tick(float DeltaTime)
{
	if (bFinished)
	{
		return false;
	}
	FImpl& I = *Impl;
	UWorld* World = I.W();
	if (!World || World->bIsTearingDown)
	{
		Abort(TEXT("O mundo do teste fechou (PIE parou ou o mapa trocou)."));
		return false;
	}
	if (const UNoclipAudioSubsystem* Audio = UNoclipAudioSubsystem::Get(World))
	{
		I.SeenMusic.Add(static_cast<uint8>(Audio->GetMusicState()));
	}
	const double Now = World->GetTimeSeconds();

	if (I.bStepDone)
	{
		I.DestroyStepActors();
		++I.Current;
		if (!I.Steps.IsValidIndex(I.Current))
		{
			Finish();
			return false;
		}
		I.bStepDone = false;
		I.Ctx = FImpl::FCtx();
		I.Ctx.Now = I.Ctx.StepStart = I.Ctx.StageStart = Now;
		FNoclipSelfTestStepResult& R = Results.AddDefaulted_GetRef();
		R.Name = I.Steps[I.Current].Name;
		UE_LOG(LogNoclipTest, Display, TEXT("-> [%d/%d] %s"), I.Current + 1, I.Steps.Num(), *R.Name);
	}

	FImpl::FCtx& C = I.Ctx;
	C.Now = Now;
	C.R = &Results.Last();
	const FImpl::FStep& Step = I.Steps[I.Current];
	bool bDone;
	if (I.bSkipRest)
	{
		C.R->bSkipped = true;
		C.R->bPassed = false;
		C.R->Notes.Add(TEXT("pulado: uma etapa essencial anterior falhou"));
		bDone = true;
	}
	else
	{
		bDone = Step.Run(C);
		if (!bDone && C.T() > Step.Timeout)
		{
			C.Fail(FString::Printf(TEXT("tempo esgotado (%.0f s) no passo %d"), Step.Timeout, C.Stage));
			bDone = true;
		}
	}
	if (bDone)
	{
		FNoclipSelfTestStepResult& R = Results.Last();
		R.Seconds = C.T();
		I.bStepDone = true;
		if (!R.bPassed && Step.bCritical)
		{
			I.bSkipRest = true;
		}
		const FString Line = FString::Printf(TEXT("%s %s (%.1f s)%s%s"), R.bSkipped ? TEXT("[PULADO]") : (R.bPassed ? TEXT("[OK]    ") : TEXT("[FALHOU]")), *R.Name, R.Seconds,
			R.Failures.Num() ? TEXT(": ") : TEXT(""), *FString::Join(R.Failures, TEXT(" | ")));
		if (R.bPassed) { UE_LOG(LogNoclipTest, Display, TEXT("%s"), *Line); }
		else { UE_LOG(LogNoclipTest, Error, TEXT("%s"), *Line); }
		for (const FString& N : R.Notes)
		{
			UE_LOG(LogNoclipTest, Display, TEXT("           %s"), *N);
		}
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(-1, 12.f, R.bPassed ? FColor::Green : (R.bSkipped ? FColor::Yellow : FColor::Red), Line);
		}
	}
	return true;
}

FString FNoclipSelfTest::GetReport() const
{
	int32 Passed = 0, Failed = 0, Skipped = 0;
	FString Body;
	for (const FNoclipSelfTestStepResult& R : Results)
	{
		Passed += R.bPassed ? 1 : 0;
		Skipped += R.bSkipped ? 1 : 0;
		Failed += (!R.bPassed && !R.bSkipped) ? 1 : 0;
		Body += FString::Printf(TEXT("%s %s (%.1f s)\n"), R.bSkipped ? TEXT("[PULADO]") : (R.bPassed ? TEXT("[OK]    ") : TEXT("[FALHOU]")), *R.Name, R.Seconds);
		for (const FString& F : R.Failures)
		{
			Body += TEXT("         x ") + F + TEXT("\n");
		}
		for (const FString& N : R.Notes)
		{
			Body += TEXT("         - ") + N + TEXT("\n");
		}
	}
	return FString::Printf(TEXT("NOCLIP SelfTest - %s\n%d ok, %d falharam, %d pulados\n\n%s"), *FDateTime::Now().ToString(), Passed, Failed, Skipped, *Body);
}

void FNoclipSelfTest::Finish()
{
	if (bFinished)
	{
		return;
	}
	bFinished = true;
	if (Impl)
	{
		Impl->DestroyStepActors();
	}
	const FString Report = GetReport();
	const FString File = FPaths::ProjectLogDir() / TEXT("NoclipSelfTest.txt");
	FFileHelper::SaveStringToFile(Report, *File, FFileHelper::EEncodingOptions::ForceUTF8);

	TArray<FString> Lines;
	Report.ParseIntoArrayLines(Lines);
	for (const FString& L : Lines)
	{
		UE_LOG(LogNoclipTest, Display, TEXT("%s"), *L);
	}
	const bool bOk = !HasFailures();
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 30.f, bOk ? FColor::Green : FColor::Red,
			FString::Printf(TEXT("NOCLIP SelfTest: %s. Relatorio em %s"), bOk ? TEXT("TUDO OK") : TEXT("HA FALHAS"), *FPaths::ConvertRelativePathToFull(File)));
	}
	if (bQuitWhenDone)
	{
		FPlatformMisc::RequestExitWithStatus(false, bOk ? 0 : 1);
	}
}

// =====================================================================================================
// Console e automacao
// =====================================================================================================

namespace NoclipSelfTestPrivate
{
	FAutoConsoleCommandWithWorldAndArgs GSelfTestCmd(TEXT("noclip.SelfTest"),
		TEXT("Testa o loop do jogo inteiro no mapa do expediente (no host). 'quit' fecha o jogo no fim; 'stop' interrompe."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			if (Args.Num() > 0 && Args[0].Equals(TEXT("stop"), ESearchCase::IgnoreCase))
			{
				if (TSharedPtr<FNoclipSelfTest> Running = FNoclipSelfTest::GetRunning())
				{
					Running->Abort(TEXT("Interrompido pelo console."));
				}
				return;
			}
			if (!World || World->GetNetMode() == NM_Client)
			{
				UE_LOG(LogNoclipTest, Error, TEXT("noclip.SelfTest roda no host (servidor ou standalone), nao num cliente."));
				return;
			}
			if (!FNoclipSelfTest::Start(World, Args.Num() > 0 && Args[0].Equals(TEXT("quit"), ESearchCase::IgnoreCase)))
			{
				UE_LOG(LogNoclipTest, Warning, TEXT("Ja tem um noclip.SelfTest rodando."));
			}
		}));
}

#if WITH_DEV_AUTOMATION_TESTS

namespace NoclipSelfTestPrivate
{
	/** Acha o mundo do expediente (PIE ou jogo) com o GameMode do NOCLIP. */
	UWorld* FindShiftWorld()
	{
		for (const FWorldContext& Ctx : GEngine->GetWorldContexts())
		{
			UWorld* World = Ctx.World();
			if (World && (Ctx.WorldType == EWorldType::PIE || Ctx.WorldType == EWorldType::Game) && World->GetAuthGameMode<ANoclipGameMode>() && World->HasBegunPlay())
			{
				return World;
			}
		}
		return nullptr;
	}

	UWorld* AnyGameWorld()
	{
		for (const FWorldContext& Ctx : GEngine->GetWorldContexts())
		{
			if (Ctx.World() && (Ctx.WorldType == EWorldType::PIE || Ctx.WorldType == EWorldType::Game))
			{
				return Ctx.World();
			}
		}
		return nullptr;
	}

	class FRunSelfTestCommand : public IAutomationLatentCommand
	{
	public:
		explicit FRunSelfTestCommand(FAutomationTestBase* InTest) : Test(InTest) {}

		virtual bool Update() override
		{
			const double Now = FPlatformTime::Seconds();
			if (StartTime < 0.0)
			{
				StartTime = Now;
			}
			if (!Run.IsValid())
			{
				UWorld* World = FindShiftWorld();
				if (!World)
				{
					if (GIsEditor && !AnyGameWorld())
					{
						Test->AddError(TEXT("Abra o L_Level0_Test e de Play (PIE) antes de rodar o NOCLIP.Game.FullLoop no editor (ou use noclip.SelfTest no console)."));
						return true;
					}
					if (!bOpened)
					{
						bOpened = true;
						if (UWorld* Any = AnyGameWorld())
						{
							GEngine->Exec(Any, TEXT("open /Game/NOCLIP/Maps/L_Level0_Test?seed=4242"));
						}
					}
					if (Now - StartTime > 90.0)
					{
						Test->AddError(TEXT("O mapa do expediente (L_Level0_Test) nao abriu em 90 s."));
						return true;
					}
					return false;
				}
				// Da um tempo para o nivel gerar e o jogador nascer.
				if (WorldReadyTime < 0.0)
				{
					WorldReadyTime = Now;
				}
				if (Now - WorldReadyTime < 3.0)
				{
					return false;
				}
				Run = FNoclipSelfTest::Start(World);
				if (!Run.IsValid())
				{
					Run = FNoclipSelfTest::GetRunning();
				}
				RunStart = Now;
				return false;
			}
			if (!Run->IsFinished())
			{
				if (Now - RunStart > 400.0)
				{
					Run->Abort(TEXT("O teste passou de 400 s."));
				}
				return false;
			}
			for (const FNoclipSelfTestStepResult& R : Run->GetResults())
			{
				if (R.bSkipped)
				{
					Test->AddWarning(FString::Printf(TEXT("%s: pulado"), *R.Name));
				}
				for (const FString& F : R.Failures)
				{
					Test->AddError(FString::Printf(TEXT("%s: %s"), *R.Name, *F));
				}
				for (const FString& N : R.Notes)
				{
					Test->AddInfo(FString::Printf(TEXT("%s: %s"), *R.Name, *N));
				}
				if (R.bPassed)
				{
					Test->AddInfo(FString::Printf(TEXT("OK: %s (%.1f s)"), *R.Name, R.Seconds));
				}
			}
			return true;
		}

	private:
		FAutomationTestBase* Test;
		TSharedPtr<FNoclipSelfTest> Run;
		double StartTime = -1.0;
		double WorldReadyTime = -1.0;
		double RunStart = 0.0;
		bool bOpened = false;
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNoclipFullLoopTest, "NOCLIP.Game.FullLoop", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FNoclipFullLoopTest::RunTest(const FString& Parameters)
{
	ADD_LATENT_AUTOMATION_COMMAND(NoclipSelfTestPrivate::FRunSelfTestCommand(this));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS

#endif // !UE_BUILD_SHIPPING
