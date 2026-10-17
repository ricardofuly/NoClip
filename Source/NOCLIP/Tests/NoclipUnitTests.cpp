// Copyright Fuly Games. All Rights Reserved.

/**
 * Testes de automacao do NOCLIP (logica e conteudo, sem precisar abrir o jogo).
 *
 * Rodar no editor:   Tools > Test Automation (Session Frontend) > Automation > filtro "NOCLIP.Unit" > Start Tests
 * Rodar sem janela:  UnrealEditor-Cmd.exe "F:\Dev\NOCLIP\NOCLIP.uproject" -ExecCmds="Automation RunTests NOCLIP.Unit; Quit"
 *                    -unattended -nopause -nosplash -log -ReportExportPath="F:\Dev\NOCLIP\Saved\Automation"
 *
 * O teste do jogo inteiro (turno, noclip, apagao, carregar em equipe, morte) e o NOCLIP.Game.FullLoop /
 * o comando de console noclip.SelfTest (Tests/NoclipSelfTest.cpp).
 */

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Tests/NoclipTestAccess.h"
#include "Character/NoclipToolTypes.h"
#include "Character/NoclipCharacterCatalog.h"
#include "Game/NoclipGameMode.h"
#include "Animation/AnimSequence.h"
#include "Animation/Skeleton.h"
#include "Engine/SkeletalMesh.h"
#include "PhysicsEngine/PhysicsAsset.h"
#include "World/NoclipLootCatalog.h"
#include "World/NoclipExtractionZone.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Engine/StaticMesh.h"
#include "PhysicsEngine/BodySetup.h"
#include "Sound/SoundBase.h"
#include "Math/RandomStream.h"
#include "UObject/Package.h"

// Declaradas nos .cpp do jogo (mesmo modulo).
namespace NoclipSounds
{
	void CollectStaticPaths(TArray<FSoftObjectPath>& Out);
}
namespace NoclipWaitingWalk
{
	void SamplePath(const TArray<FVector>& P, float Distance, FVector& OutLoc, FVector& OutDir);
}

#define NOCLIP_UNIT_TEST_FLAGS (EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

namespace NoclipUnitTests
{
	/** Mundo de jogo vazio (com fisica) so para o teste; destruido no fim do escopo. */
	class FTestWorld
	{
	public:
		FTestWorld()
		{
			World = UWorld::CreateWorld(EWorldType::Game, false, MakeUniqueObjectName(GetTransientPackage(), UWorld::StaticClass(), TEXT("NoclipTestWorld")));
			FWorldContext& Context = GEngine->CreateNewWorldContext(EWorldType::Game);
			Context.SetCurrentWorld(World);
			World->InitializeActorsForPlay(FURL());
		}

		~FTestWorld()
		{
			if (World)
			{
				GEngine->DestroyWorldContext(World);
				World->DestroyWorld(false);
				World = nullptr;
			}
		}

		/** Avanca alguns quadros (a fisica atualiza as consultas de colisao). */
		void Pump(int32 Frames = 2)
		{
			for (int32 i = 0; i < Frames; ++i)
			{
				World->Tick(LEVELTICK_All, 1.f / 60.f);
			}
		}

		template <class T>
		T* Spawn(const FVector& Location = FVector::ZeroVector, const FRotator& Rotation = FRotator::ZeroRotator)
		{
			FActorSpawnParameters Params;
			Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			return World->SpawnActor<T>(T::StaticClass(), Location, Rotation, Params);
		}

		UWorld* World = nullptr;
	};

	const TCHAR* KindName(ENoclipGlitchKind Kind)
	{
		switch (Kind)
		{
		case ENoclipGlitchKind::FakeWall:    return TEXT("parede falsa");
		case ENoclipGlitchKind::FakeFloor:   return TEXT("piso falso");
		case ENoclipGlitchKind::ServiceExit: return TEXT("saida de servico");
		case ENoclipGlitchKind::UpWall:      return TEXT("parede falsa do subsolo");
		}
		return TEXT("?");
	}

	/** Andar em que a falha fica e para onde ela leva. */
	int32 GlitchFloor(ENoclipGlitchKind Kind) { return (Kind == ENoclipGlitchKind::FakeWall || Kind == ENoclipGlitchKind::FakeFloor) ? 0 : 1; }
	int32 GlitchTargetFloor(ENoclipGlitchKind Kind) { return 1 - GlitchFloor(Kind); }

	float ExpectedCarrySpeed(const UNoclipGrabComponent* Grab, int32 Carriers, int32 Required, float MassKg)
	{
		return FNoclipTestAccess::ExpectedCarrySpeed(Grab, Carriers, Required, MassKg);
	}

	struct FLayoutSnapshot
	{
		TArray<FString> Lines;
		bool operator==(const FLayoutSnapshot& O) const { return Lines == O.Lines; }
	};

	FLayoutSnapshot Snapshot(const ANoclipLevelGenerator* Gen)
	{
		FLayoutSnapshot S;
		S.Lines.Add(FString::Printf(TEXT("seed %d modo %d bounds %s manila %s"), Gen->GetGeneratedSeed(), static_cast<int32>(Gen->GetGeneratedMode()),
			*Gen->GetGeneratedBounds().ToString(), *Gen->GetExtractionRect().ToString()));
		for (const FNoclipGlitch& G : Gen->GetGlitches())
		{
			S.Lines.Add(FString::Printf(TEXT("falha %d %s"), static_cast<int32>(G.Kind), *G.Box.ToString()));
		}
		for (int32 i = 0; i < Gen->GetNumShifters(); ++i)
		{
			const FNoclipShifter& Sh = Gen->GetShifter(i);
			S.Lines.Add(FString::Printf(TEXT("divisoria %s %s %s"), *Sh.CenterA.ToCompactString(), *Sh.CenterB.ToCompactString(), *Sh.Size.ToCompactString()));
		}
		for (const FVector& E : Gen->GetServiceExits())
		{
			S.Lines.Add(FString::Printf(TEXT("saida %s"), *E.ToCompactString()));
		}
		return S;
	}

	bool IsBlocked(UWorld* World, const FVector& At, const FCollisionShape& Shape)
	{
		return World->OverlapBlockingTestByChannel(At, FQuat::Identity, ECC_Pawn, Shape);
	}
}

// =====================================================================================================
// Achados: catalogo
// =====================================================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNoclipLootCatalogTest, "NOCLIP.Unit.Loot.Catalog", NOCLIP_UNIT_TEST_FLAGS)
bool FNoclipLootCatalogTest::RunTest(const FString& Parameters)
{
	using namespace NoclipUnitTests;
	const int32 Num = NoclipLoot::Num();
	TestTrue(TEXT("O catalogo tem achados"), Num > 20);

	const UNoclipGrabComponent* Grab = GetDefault<UNoclipGrabComponent>();
	TSet<FName> Ids;
	int32 Team = 0, SubOnly = 0, OnFloor[2] = { 0, 0 }, TeamOnFloor[2] = { 0, 0 };
	for (int32 i = 0; i < Num; ++i)
	{
		const FNoclipLootDef* D = NoclipLoot::Get(i);
		if (!TestNotNull(TEXT("Get(i) valido"), D))
		{
			continue;
		}
		const FString Who = D->Id.ToString();
		TestFalse(FString::Printf(TEXT("%s: Id repetido"), *Who), Ids.Contains(D->Id));
		Ids.Add(D->Id);
		TestFalse(FString::Printf(TEXT("%s: sem nome"), *Who), D->Name.IsEmpty());
		TestFalse(FString::Printf(TEXT("%s: sem descricao"), *Who), D->Description.IsEmpty());
		TestTrue(FString::Printf(TEXT("%s: caminho do modelo em /Game/NOCLIP/Assets"), *Who), D->MeshPath.StartsWith(TEXT("/Game/NOCLIP/Assets/")));
		TestTrue(FString::Printf(TEXT("%s: valor minimo > 0 e <= maximo"), *Who), D->MinValue > 0 && D->MinValue <= D->MaxValue);
		TestTrue(FString::Printf(TEXT("%s: massa > 0"), *Who), D->MassKg > 0.f);
		TestTrue(FString::Printf(TEXT("%s: fragilidade 0..2"), *Who), D->Fragility >= 0.f && D->Fragility <= 2.f);
		TestTrue(FString::Printf(TEXT("%s: escala > 0"), *Who), D->Scale > 0.f);
		TestTrue(FString::Printf(TEXT("%s: peso de sorteio > 0"), *Who), D->SpawnWeight > 0.f);
		TestTrue(FString::Printf(TEXT("%s: carregadores 1..4 (tem %d)"), *Who, D->Carriers), D->Carriers >= 1 && D->Carriers <= 4);
		TestTrue(FString::Printf(TEXT("%s: aparece em algum andar (mascara %d)"), *Who, D->FloorMask), D->FloorMask >= 1 && D->FloorMask <= 3);

		// Cada um aguenta a sua parte (senao ninguem consegue agarrar, nem com a equipe toda).
		TestTrue(FString::Printf(TEXT("%s: %.0f kg / %d cabe no limite de %.0f kg por pessoa"), *Who, D->MassKg, D->Carriers, Grab->MaxGrabMassKg),
			D->MassKg / D->Carriers <= Grab->MaxGrabMassKg);

		if (D->Carriers > 1)
		{
			++Team;
			// Levantar com a equipe tem que ser melhor que arrastar.
			const float Lifted = ExpectedCarrySpeed(Grab, D->Carriers, D->Carriers, D->MassKg);
			TestTrue(FString::Printf(TEXT("%s: levantado (%.2f) e mais rapido que arrastando (%.2f)"), *Who, Lifted, Grab->DragSpeedScale), Lifted > Grab->DragSpeedScale);
		}
		for (int32 Floor = 0; Floor < 2; ++Floor)
		{
			if (D->FloorMask & (1 << Floor))
			{
				++OnFloor[Floor];
				TeamOnFloor[Floor] += D->Carriers > 1 ? 1 : 0;
			}
		}
		SubOnly += D->FloorMask == 2 ? 1 : 0;
	}
	AddInfo(FString::Printf(TEXT("%d achados: %d no Level 0, %d no subsolo (%d so no subsolo), %d de equipe."), Num, OnFloor[0], OnFloor[1], SubOnly, Team));
	TestTrue(TEXT("Ha achados de equipe (Carriers > 1)"), Team > 0);
	TestTrue(TEXT("Ha achados de equipe no Level 0"), TeamOnFloor[0] > 0);
	TestTrue(TEXT("Ha achados de equipe no subsolo"), TeamOnFloor[1] > 0);
	TestTrue(TEXT("Ha achados exclusivos do subsolo"), SubOnly > 0);
	TestTrue(TEXT("Level 0 tem variedade (>= 10)"), OnFloor[0] >= 10);
	TestTrue(TEXT("Subsolo tem variedade (>= 6)"), OnFloor[1] >= 6);
	TestNull(TEXT("Get(-1) = nullptr"), NoclipLoot::Get(-1));
	TestNull(TEXT("Get(Num) = nullptr"), NoclipLoot::Get(Num));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNoclipLootFloorPickTest, "NOCLIP.Unit.Loot.FloorPick", NOCLIP_UNIT_TEST_FLAGS)
bool FNoclipLootFloorPickTest::RunTest(const FString& Parameters)
{
	using namespace NoclipUnitTests;
	const int32 Num = NoclipLoot::Num();
	constexpr int32 Draws = 20000;
	for (int32 Floor = 0; Floor < 2; ++Floor)
	{
		TArray<int32> Hits;
		Hits.SetNumZeroed(Num);
		float TotalWeight = 0.f;
		for (int32 i = 0; i < Num; ++i)
		{
			const FNoclipLootDef* D = NoclipLoot::Get(i);
			TotalWeight += (D->FloorMask & (1 << Floor)) ? D->SpawnWeight : 0.f;
		}
		FRandomStream Stream(1000 + Floor);
		int32 Wrong = 0;
		for (int32 n = 0; n < Draws; ++n)
		{
			const int32 Index = NoclipLoot::PickWeighted(Stream, Floor);
			if (!Hits.IsValidIndex(Index))
			{
				AddError(FString::Printf(TEXT("Andar %d: PickWeighted devolveu indice invalido %d"), Floor, Index));
				return false;
			}
			++Hits[Index];
			Wrong += (NoclipLoot::Get(Index)->FloorMask & (1 << Floor)) ? 0 : 1;
		}
		TestEqual(FString::Printf(TEXT("Andar %d: nenhum sorteio fora do andar"), Floor), Wrong, 0);

		// Proporcao: quem tem peso relevante aparece mais ou menos na proporcao certa.
		for (int32 i = 0; i < Num; ++i)
		{
			const FNoclipLootDef* D = NoclipLoot::Get(i);
			if (!(D->FloorMask & (1 << Floor)))
			{
				continue;
			}
			const float Expected = Draws * D->SpawnWeight / TotalWeight;
			if (Expected >= 50.f)
			{
				const float Ratio = Hits[i] / Expected;
				TestTrue(FString::Printf(TEXT("Andar %d, %s: %d sorteios para ~%.0f esperados"), Floor, *D->Id.ToString(), Hits[i], Expected), Ratio > 0.7f && Ratio < 1.3f);
			}
			else if (Expected >= 8.f)
			{
				TestTrue(FString::Printf(TEXT("Andar %d, %s aparece (esperado ~%.0f)"), Floor, *D->Id.ToString(), Expected), Hits[i] > 0);
			}
		}
	}
	// Mesma seed, mesma sequencia (o servidor sorteia; precisa ser reproduzivel para depurar).
	FRandomStream A(77), B(77);
	bool bSame = true;
	for (int32 n = 0; n < 200; ++n)
	{
		bSame &= NoclipLoot::PickWeighted(A, 1) == NoclipLoot::PickWeighted(B, 1);
	}
	TestTrue(TEXT("Sorteio deterministico pela seed"), bSame);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNoclipLootAssetsTest, "NOCLIP.Unit.Loot.MeshAssets", NOCLIP_UNIT_TEST_FLAGS)
bool FNoclipLootAssetsTest::RunTest(const FString& Parameters)
{
	using namespace NoclipUnitTests;
	int32 Missing = 0, NoCollision = 0;
	for (int32 i = 0; i < NoclipLoot::Num(); ++i)
	{
		const FNoclipLootDef* D = NoclipLoot::Get(i);
		UStaticMesh* Mesh = NoclipLoot::LoadMesh(i);
		if (!Mesh)
		{
			++Missing;
			AddError(FString::Printf(TEXT("%s: modelo nao carrega (%s)"), *D->Id.ToString(), *D->MeshPath));
			continue;
		}
		// Achado simula fisica: precisa de colisao simples (convexa / caixas), nao serve "complexo como simples".
		const UBodySetup* Body = Mesh->GetBodySetup();
		if (!Body || Body->AggGeom.GetElementCount() == 0)
		{
			++NoCollision;
			AddError(FString::Printf(TEXT("%s: %s sem colisao simples (atravessa o chao). Gere no editor: Collision > Auto Convex."), *D->Id.ToString(), *Mesh->GetName()));
		}
		const FVector Size = Mesh->GetBounds().BoxExtent * 2.f * D->Scale;
		TestTrue(FString::Printf(TEXT("%s: tamanho plausivel (%s cm)"), *D->Id.ToString(), *Size.ToCompactString()), Size.GetMax() > 3.f && Size.GetMax() < 400.f);
	}
	AddInfo(FString::Printf(TEXT("%d modelos conferidos: %d faltando, %d sem colisao."), NoclipLoot::Num(), Missing, NoCollision));
	return Missing == 0 && NoCollision == 0;
}

// =====================================================================================================
// Carregar em equipe: divisao do peso
// =====================================================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNoclipCarryShareTest, "NOCLIP.Unit.Carry.WeightSplit", NOCLIP_UNIT_TEST_FLAGS)
bool FNoclipCarryShareTest::RunTest(const FString& Parameters)
{
	using namespace NoclipUnitTests;
	UNoclipGrabComponent* Grab = NewObject<UNoclipGrabComponent>(GetTransientPackage());
	auto Share = [&](int32 Carriers, int32 Required, float Mass)
	{
		FNoclipTestAccess::SetRawCarrySpeed(Grab, -1.f);
		Grab->ApplyCarryShare(Carriers, Required, Mass);
		return FNoclipTestAccess::RawCarrySpeed(Grab);
	};

	// Faltando gente: arrasta.
	TestEqual(TEXT("Maca (3) com 1: arrasta"), Share(1, 3, 110.f), Grab->DragSpeedScale);
	TestEqual(TEXT("Maca (3) com 2: arrasta"), Share(2, 3, 110.f), Grab->DragSpeedScale);
	TestTrue(TEXT("Arrastar nao deixa correr (< 0.75)"), Grab->DragSpeedScale < 0.75f);

	// Gente suficiente: levanta e divide o peso.
	const float Three = Share(3, 3, 110.f);
	const float Four = Share(4, 3, 110.f);
	TestEqual(TEXT("Maca com 3: conta da divisao"), Three, ExpectedCarrySpeed(Grab, 3, 3, 110.f), 0.001f);
	TestTrue(FString::Printf(TEXT("Maca com 3 (%.2f) mais rapida que arrastando (%.2f)"), Three, Grab->DragSpeedScale), Three > Grab->DragSpeedScale);
	TestTrue(FString::Printf(TEXT("Maca com 4 (%.2f) >= com 3 (%.2f)"), Four, Three), Four >= Three);
	TestTrue(TEXT("Achado de equipe nao deixa correr (<= 0.72)"), Three <= 0.72f && Four <= 0.72f);

	// Mais gente nunca deixa mais lento.
	for (const float Mass : { 40.f, 80.f, 140.f, 220.f })
	{
		for (int32 Required = 2; Required <= 4; ++Required)
		{
			float Prev = Grab->DragSpeedScale;
			for (int32 Carriers = Required; Carriers <= 4; ++Carriers)
			{
				const float S = Share(Carriers, Required, Mass);
				TestTrue(FString::Printf(TEXT("%.0f kg, precisa %d, com %d: %.2f >= %.2f"), Mass, Required, Carriers, S, Prev), S >= Prev - KINDA_SMALL_NUMBER);
				Prev = S;
			}
		}
	}

	// Sozinho: leve quase sem penalidade, pesado (duas maos) trava a corrida.
	TestTrue(TEXT("Grampeador (0.4 kg) quase sem penalidade"), Share(1, 1, 0.4f) > 0.99f);
	TestTrue(TEXT("Achado de duas maos trava a corrida (<= 0.72)"), Share(1, 1, NoclipTools::TwoHandMassKg + 1.f) <= 0.72f);
	TestEqual(TEXT("Zero carregadores conta como 1 (nao divide por zero)"), Share(0, 1, 10.f), ExpectedCarrySpeed(Grab, 1, 1, 10.f), 0.001f);
	TestTrue(TEXT("Velocidade sempre > 0"), Share(1, 1, 500.f) > 0.f);
	return true;
}

// =====================================================================================================
// Gerador de energia: alcance do QTE
// =====================================================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNoclipPowerReachTest, "NOCLIP.Unit.Power.Reach", NOCLIP_UNIT_TEST_FLAGS)
bool FNoclipPowerReachTest::RunTest(const FString& Parameters)
{
	using namespace NoclipUnitTests;
	FTestWorld W;
	ANoclipPowerGenerator* Power = W.Spawn<ANoclipPowerGenerator>();
	if (!TestNotNull(TEXT("Gerador spawnou"), Power))
	{
		return false;
	}
	TestTrue(TEXT("Lado +Y, a 1 m"), Power->IsInReach(FVector(0.f, 141.f, 90.f)));
	TestTrue(TEXT("Lado -Y, a 1 m"), Power->IsInReach(FVector(0.f, -141.f, 90.f)));
	TestTrue(TEXT("Ponta +X, a 1,2 m"), Power->IsInReach(FVector(235.f, 0.f, 90.f)));
	TestTrue(TEXT("Quina, perto"), Power->IsInReach(FVector(170.f, 90.f, 90.f)));
	TestFalse(TEXT("Lado +Y, a 2 m: longe"), Power->IsInReach(FVector(0.f, 241.f, 90.f)));
	TestFalse(TEXT("Ponta -X, a 2 m: longe"), Power->IsInReach(FVector(-315.f, 0.f, 90.f)));
	TestFalse(TEXT("No andar de baixo: longe"), Power->IsInReach(FVector(0.f, 60.f, -700.f)));
	TestEqual(TEXT("Sem apagao: progresso 0"), Power->GetProgress(), 0.f);
	TestTrue(TEXT("8 a 20 acertos para religar sozinho"), FMath::CeilToInt(1.f / FNoclipTestAccess::HitGain(Power)) >= 8 && FMath::CeilToInt(1.f / FNoclipTestAccess::HitGain(Power)) <= 20);
	TestTrue(TEXT("Errar perde menos do que acertar ganha"), FNoclipTestAccess::MissLoss(Power) < FNoclipTestAccess::HitGain(Power));

	// Girado 90 graus: o alcance gira junto.
	Power->SetActorRotation(FRotator(0.f, 90.f, 0.f));
	TestTrue(TEXT("Girado: lado longo agora em Y"), Power->IsInReach(FVector(0.f, 235.f, 90.f)));
	TestFalse(TEXT("Girado: X a 2,4 m fica longe"), Power->IsInReach(FVector(241.f, 0.f, 90.f)));
	return true;
}

// =====================================================================================================
// Gerador de nivel: andares, falhas (noclip) e divisorias
// =====================================================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNoclipLevelGenerationTest, "NOCLIP.Unit.Level.Generation", NOCLIP_UNIT_TEST_FLAGS)
bool FNoclipLevelGenerationTest::RunTest(const FString& Parameters)
{
	using namespace NoclipUnitTests;
	const int32 Seeds[] = { 1, 42, 1337, 98765, 2026 };
	const ENoclipLayoutMode Modes[] = { ENoclipLayoutMode::Grid, ENoclipLayoutMode::Zones, ENoclipLayoutMode::Linear };
	const FCollisionShape Standing = FCollisionShape::MakeCapsule(38.f, 88.f);
	const FCollisionShape Walker = FCollisionShape::MakeCapsule(30.f, 80.f);

	for (const ENoclipLayoutMode Mode : Modes)
	{
		for (const int32 Seed : Seeds)
		{
			FTestWorld W;
			ANoclipLevelGenerator* Gen = W.Spawn<ANoclipLevelGenerator>();
			if (!TestNotNull(TEXT("Gerador de nivel spawnou"), Gen))
			{
				return false;
			}
			Gen->LayoutMode = Mode;
			Gen->Generate(Seed);
			W.Pump();

			const FString Tag = FString::Printf(TEXT("[%s seed %d]"), *UEnum::GetValueAsString(Mode), Seed);
			TestEqual(Tag + TEXT(" seed gerada"), Gen->GetGeneratedSeed(), Seed);
			TestTrue(Tag + TEXT(" area valida"), Gen->GetGeneratedBounds().bIsValid);
			TestTrue(Tag + TEXT(" Sala Manila reservada"), Gen->GetExtractionRect().bIsValid);
			TestTrue(Tag + TEXT(" tem subsolo"), Gen->HasSubFloor());
			TestEqual(Tag + TEXT(" GetFloorAt no Level 0"), Gen->GetFloorAt(Gen->GetActorLocation() + FVector(0.f, 0.f, 90.f)), 0);
			TestEqual(Tag + TEXT(" GetFloorAt no subsolo"), Gen->GetFloorAt(FVector(0.f, 0.f, Gen->GetFloorZ(1) + 90.f)), 1);

			// Spawn e Sala Manila nunca sao uma falha (ninguem cai sem querer ao nascer / extrair).
			TestEqual(Tag + TEXT(" spawn fora de falhas"), Gen->FindGlitchAt(Gen->GetActorLocation() + FVector(0.f, 0.f, 94.f)), INDEX_NONE);
			const FVector2D Manila = Gen->GetExtractionRect().GetCenter();
			TestEqual(Tag + TEXT(" Sala Manila fora de falhas"), Gen->FindGlitchAt(FVector(Manila.X, Manila.Y, Gen->GetFloorZ(0) + 94.f)), INDEX_NONE);

			int32 Count[4] = { 0, 0, 0, 0 };
			for (int32 i = 0; i < Gen->GetGlitches().Num(); ++i)
			{
				const FNoclipGlitch& G = Gen->GetGlitches()[i];
				++Count[static_cast<int32>(G.Kind)];
				const FString What = FString::Printf(TEXT("%s %s #%d"), *Tag, KindName(G.Kind), i);
				TestTrue(What + TEXT(": caixa valida"), G.Box.IsValid && G.Box.GetVolume() > 0.0);
				TestEqual(What + TEXT(": no andar certo"), Gen->GetFloorAt(G.Box.GetCenter()), GlitchFloor(G.Kind));

				// Atravessavel: a parede falsa / saida nao tem colisao no meio.
				if (G.Kind == ENoclipGlitchKind::FakeWall || G.Kind == ENoclipGlitchKind::UpWall)
				{
					const FVector Mid(G.Box.GetCenter().X, G.Box.GetCenter().Y, Gen->GetFloorZ(GlitchFloor(G.Kind)) + 94.f);
					TestFalse(What + TEXT(": da para atravessar (sem colisao no meio)"), IsBlocked(W.World, Mid, Walker));
				}

				// Para onde leva: outro andar, lugar livre, fora de outra falha (senao vira ping-pong).
				FVector Dest;
				const FVector From(G.Box.GetCenter().X, G.Box.GetCenter().Y, FMath::Clamp(Gen->GetFloorZ(GlitchFloor(G.Kind)) + 94.f, G.Box.Min.Z, G.Box.Max.Z));
				if (!TestTrue(What + TEXT(": destino encontrado"), Gen->ResolveGlitchDestination(i, From, Dest)))
				{
					continue;
				}
				TestEqual(What + TEXT(": leva para o outro andar"), Gen->GetFloorAt(Dest), GlitchTargetFloor(G.Kind));
				TestEqual(What + TEXT(": destino nao cai em outra falha"), Gen->FindGlitchAt(Dest), INDEX_NONE);
				TestFalse(What + TEXT(": destino livre para ficar em pe"), IsBlocked(W.World, Dest, Standing));
				TestTrue(What + TEXT(": destino dentro do nivel"), Gen->GetGeneratedBounds().IsInside(FVector2D(Dest)));
				if (G.Kind == ENoclipGlitchKind::ServiceExit)
				{
					const float D = FVector2D::Distance(FVector2D(Dest), Manila);
					TestTrue(FString::Printf(TEXT("%s: volta perto da Sala Manila (%.0f cm)"), *What, D), D <= Gen->GetExtractionRect().GetExtent().GetMax() + 1300.f);
				}
				if (G.Kind == ENoclipGlitchKind::UpWall && FVector2D::Distance(FVector2D(Dest), Manila) < 2500.f)
				{
					AddWarning(What + TEXT(": devolveu perto da Sala Manila (deveria ser longe)"));
				}
			}
			AddInfo(FString::Printf(TEXT("%s falhas: %d paredes, %d pisos, %d saidas, %d paredes do subsolo; %d divisorias que mudam"), *Tag,
				Count[0], Count[1], Count[2], Count[3], Gen->GetNumShifters()));
			TestTrue(Tag + TEXT(" tem parede falsa (Level 0 -> subsolo)"), Count[static_cast<int32>(ENoclipGlitchKind::FakeWall)] > 0);
			TestTrue(Tag + TEXT(" tem piso falso (Level 0 -> subsolo)"), Count[static_cast<int32>(ENoclipGlitchKind::FakeFloor)] > 0);
			TestTrue(Tag + TEXT(" tem saida de servico (subsolo -> Manila)"), Count[static_cast<int32>(ENoclipGlitchKind::ServiceExit)] > 0);
			TestEqual(Tag + TEXT(" saidas para o celular == falhas de saida"), Gen->GetServiceExits().Num(), Count[static_cast<int32>(ENoclipGlitchKind::ServiceExit)]);
			TestTrue(Tag + TEXT(" saidas <= ServiceExitCount"), Gen->GetServiceExits().Num() <= Gen->ServiceExitCount);
			if (Count[static_cast<int32>(ENoclipGlitchKind::UpWall)] == 0)
			{
				AddWarning(Tag + TEXT(" sem parede falsa no subsolo (so as saidas de servico voltam)"));
			}

			// Lugares livres nos dois andares (noclip.GoDown e o destino das falhas usam isso).
			FVector Spot;
			TestTrue(Tag + TEXT(" FindWalkable no Level 0"), Gen->FindWalkable(0, FVector2D(Gen->GetActorLocation()), 1500.f, Spot) && Gen->GetFloorAt(Spot) == 0);
			TestTrue(Tag + TEXT(" FindWalkable no subsolo"), Gen->FindWalkable(1, FVector2D(Gen->GetActorLocation()), 1500.f, Spot) && Gen->GetFloorAt(Spot) == 1);

			// Mesma seed -> mesmo nivel (cada maquina gera o seu e precisam bater).
			const FLayoutSnapshot First = Snapshot(Gen);
			const int32 Version = Gen->GetLayoutVersion();
			Gen->Generate(Seed);
			W.Pump();
			TestTrue(Tag + TEXT(" deterministico (gerar de novo da o mesmo nivel)"), Snapshot(Gen) == First);
			TestEqual(Tag + TEXT(" mesma versao de layout"), Gen->GetLayoutVersion(), Version);
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNoclipShiftingRoomsTest, "NOCLIP.Unit.Level.ShiftingRooms", NOCLIP_UNIT_TEST_FLAGS)
bool FNoclipShiftingRoomsTest::RunTest(const FString& Parameters)
{
	using namespace NoclipUnitTests;
	const FCollisionShape Probe = FCollisionShape::MakeSphere(6.f);
	int32 Tested = 0;
	for (const int32 Seed : { 3, 42, 1337, 4242, 90001 })
	{
		FTestWorld W;
		ANoclipLevelGenerator* Gen = W.Spawn<ANoclipLevelGenerator>();
		Gen->LayoutMode = ENoclipLayoutMode::Zones;
		Gen->Generate(Seed);
		W.Pump();
		const int32 Num = FMath::Min(Gen->GetNumShifters(), 32);
		if (Num == 0)
		{
			AddInfo(FString::Printf(TEXT("seed %d: sem divisorias que mudam"), Seed));
			continue;
		}
		TestEqual(FString::Printf(TEXT("seed %d: comeca com a mascara 0"), Seed), static_cast<int64>(Gen->GetAppliedShiftMask()), static_cast<int64>(0));
		const int32 V0 = Gen->GetLayoutVersion();
		const uint32 All = Num >= 32 ? 0xFFFFFFFFu : ((1u << Num) - 1u);

		for (const uint32 Mask : { 0u, All })
		{
			Gen->ApplyShiftMask(Mask, false);
			W.Pump();
			TestEqual(FString::Printf(TEXT("seed %d: mascara aplicada"), Seed), static_cast<int64>(Gen->GetAppliedShiftMask()), static_cast<int64>(Mask));
			for (int32 i = 0; i < Num; ++i)
			{
				const FNoclipShifter& S = Gen->GetShifter(i);
				const bool bAtB = ((Mask >> i) & 1u) != 0;
				const FVector Closed = bAtB ? S.CenterB : S.CenterA;
				const FVector Open = bAtB ? S.CenterA : S.CenterB;
				const FString What = FString::Printf(TEXT("seed %d divisoria %d (tampao em %s)"), Seed, i, bAtB ? TEXT("B") : TEXT("A"));
				TestTrue(What + TEXT(": duas portas diferentes"), FVector::Dist(S.CenterA, S.CenterB) > 50.f);
				TestTrue(What + TEXT(": porta fechada bloqueia"), IsBlocked(W.World, Closed, Probe));
				TestFalse(What + TEXT(": a outra porta fica aberta"), IsBlocked(W.World, Open, Probe));
				++Tested;
			}
		}
		TestNotEqual(FString::Printf(TEXT("seed %d: mudar a mascara muda a versao do layout (a grade das entidades refaz)"), Seed), Gen->GetLayoutVersion(), V0);
		Gen->ApplyShiftMask(0u, false);
		TestEqual(FString::Printf(TEXT("seed %d: voltar a mascara volta a versao"), Seed), Gen->GetLayoutVersion(), V0);
	}
	TestTrue(TEXT("Alguma seed gerou divisorias que mudam"), Tested > 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNoclipShiftContentTest, "NOCLIP.Unit.Level.ShiftContent", NOCLIP_UNIT_TEST_FLAGS)
bool FNoclipShiftContentTest::RunTest(const FString& Parameters)
{
	using namespace NoclipUnitTests;
	int32 TeamItems = 0;
	for (const int32 Seed : { 7, 42, 31337 })
	{
		FTestWorld W;
		ANoclipLevelGenerator* Gen = W.Spawn<ANoclipLevelGenerator>();
		Gen->Generate(Seed);
		W.Pump();
		Gen->SpawnShiftContent();
		W.Pump();

		const FString Tag = FString::Printf(TEXT("[seed %d]"), Seed);
		TestNotNull(Tag + TEXT(" Sala Manila spawnou"), Gen->GetExtractionZone());

		// Gerador do apagao: no Level 0, longe da Sala Manila.
		const ANoclipPowerGenerator* Power = Gen->GetPowerGenerator();
		if (TestNotNull(Tag + TEXT(" gerador de energia spawnou"), Power))
		{
			TestEqual(Tag + TEXT(" gerador no Level 0"), Gen->GetFloorAt(Power->GetActorLocation() + FVector(0.f, 0.f, 50.f)), 0);
			TestFalse(Tag + TEXT(" gerador longe da Sala Manila"), Gen->GetExtractionRect().ExpandBy(700.f).IsInside(FVector2D(Power->GetActorLocation())));
			FVector Stand;
			const FVector Side = Power->GetActorTransform().TransformPosition(FVector(0.f, 110.f, 0.f));
			const bool bReachable = Gen->FindWalkable(0, FVector2D(Side), 120.f, Stand) && Power->IsInReach(Stand);
			const FVector OtherSide = Power->GetActorTransform().TransformPosition(FVector(0.f, -110.f, 0.f));
			const bool bReachable2 = Gen->FindWalkable(0, FVector2D(OtherSide), 120.f, Stand) && Power->IsInReach(Stand);
			TestTrue(Tag + TEXT(" da para ficar em pe ao alcance do painel"), bReachable || bReachable2);
		}

		// Achados: quantos, onde e se respeitam o andar do catalogo.
		int32 OnFloor[2] = { 0, 0 };
		for (const TObjectPtr<AActor>& Actor : FNoclipTestAccess::SpawnedLoot(Gen))
		{
			const ANoclipLootActor* Loot = Cast<ANoclipLootActor>(Actor);
			if (!Loot)
			{
				continue;
			}
			const int32 Floor = Gen->GetFloorAt(Loot->GetActorLocation());
			++OnFloor[Floor];
			const FNoclipLootDef* Def = NoclipLoot::Get(FNoclipTestAccess::CatalogIndex(Loot));
			if (!TestNotNull(Tag + TEXT(" achado com indice do catalogo"), Def))
			{
				continue;
			}
			TestTrue(FString::Printf(TEXT("%s %s no andar %d permitido pelo catalogo"), *Tag, *Def->Id.ToString(), Floor), (Def->FloorMask & (1 << Floor)) != 0);
			TestEqual(FString::Printf(TEXT("%s %s: carregadores do catalogo"), *Tag, *Def->Id.ToString()), Loot->GetRequiredCarriers(), Def->Carriers);
			TestFalse(FString::Printf(TEXT("%s %s fora da Sala Manila"), *Tag, *Def->Id.ToString()), Gen->GetExtractionRect().IsInside(FVector2D(Loot->GetActorLocation())));
			TestTrue(FString::Printf(TEXT("%s %s com valor"), *Tag, *Def->Id.ToString()), Loot->GetBaseValue() >= Def->MinValue && Loot->GetBaseValue() <= Def->MaxValue);
			TeamItems += Loot->IsTeamCarry() ? 1 : 0;
		}
		AddInfo(FString::Printf(TEXT("%s achados: %d no Level 0 (de %d), %d no subsolo (de %d)"), *Tag, OnFloor[0], Gen->LootCount, OnFloor[1], Gen->SubFloorLootCount));
		TestTrue(Tag + TEXT(" Level 0 com pelo menos metade dos achados"), OnFloor[0] * 2 >= Gen->LootCount);
		TestTrue(Tag + TEXT(" subsolo com achados"), OnFloor[1] > 0);

		Gen->ClearShiftContent();
		TestNull(Tag + TEXT(" fim do turno limpa o gerador"), Gen->GetPowerGenerator());
		TestEqual(Tag + TEXT(" fim do turno limpa os achados"), FNoclipTestAccess::SpawnedLoot(Gen).Num(), 0);
	}
	TestTrue(FString::Printf(TEXT("Achados de equipe aparecem nos niveis (%d em 3 seeds)"), TeamItems), TeamItems > 0);
	return true;
}

// =====================================================================================================
// Sala de espera: rotas do elevador
// =====================================================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNoclipBoardingRoutesTest, "NOCLIP.Unit.WaitingRoom.BoardingRoutes", NOCLIP_UNIT_TEST_FLAGS)
bool FNoclipBoardingRoutesTest::RunTest(const FString& Parameters)
{
	using namespace NoclipUnitTests;
	constexpr int32 Slots = 6;
	constexpr float DoorGap = 0.6f; // NoclipWaitingWalk::DoorGap
	constexpr float MinSeparation = 50.f;

	FTestWorld W;
	const ANoclipWaitingRoom* Room = W.Spawn<ANoclipWaitingRoom>();
	if (!TestNotNull(TEXT("Sala de espera spawnou"), Room))
	{
		return false;
	}
	const FTransform RoomXf = Room->GetActorTransform();
	const float V = FMath::Max(Room->WalkSpeed, 1.f);

	for (int32 Slot = 0; Slot < Slots; ++Slot)
	{
		const TArray<FVector>& P = FNoclipTestAccess::RoutePoints(Room, Slot);
		const float Len = FNoclipTestAccess::RouteLength(Room, Slot);
		const float Door = FNoclipTestAccess::RouteDoorDistance(Room, Slot);
		const FString What = FString::Printf(TEXT("Lugar %d"), Slot);
		if (!TestTrue(What + TEXT(": rota com pontos"), P.Num() >= 8))
		{
			continue;
		}
		TestTrue(What + TEXT(": comeca no lugar de espera"), FVector::Dist2D(P[0], FNoclipTestAccess::IdleSpot(Room, Slot)) < 2.f);
		const FVector EndLocal = RoomXf.InverseTransformPosition(P.Last());
		TestTrue(FString::Printf(TEXT("%s: termina dentro da cabine (x local %.0f)"), *What, EndLocal.X), EndLocal.X > 390.f && EndLocal.X < 650.f);
		TestTrue(What + TEXT(": porta no meio da rota"), Door > 0.f && Door < Len);
		float MaxStep = 0.f;
		for (int32 i = 1; i < P.Num(); ++i)
		{
			MaxStep = FMath::Max(MaxStep, static_cast<float>(FVector::Dist2D(P[i - 1], P[i])));
		}
		TestTrue(FString::Printf(TEXT("%s: spline sem saltos (maior passo %.1f cm)"), *What, MaxStep), MaxStep < 16.f);
	}

	// Simula o embarque de todas as combinacoes de quem esta na sala.
	auto PositionAt = [&](int32 Slot, float Clock, const float Start[6])
	{
		const TArray<FVector>& P = FNoclipTestAccess::RoutePoints(Room, Slot);
		const float Total = FNoclipTestAccess::RouteLength(Room, Slot);
		const float Duration = FNoclipTestAccess::WalkDuration(Room, Total);
		const float A = Duration - Total / V; // arranque/parada
		const float T = Clock - Start[Slot];
		if (T <= 0.f)
		{
			return P[0];
		}
		if (T >= Duration || A <= KINDA_SMALL_NUMBER)
		{
			return P.Last();
		}
		float S;
		if (T < A)                 { S = 0.5f * V * T * T / A; }
		else if (T < Duration - A) { S = 0.5f * V * A + V * (T - A); }
		else                       { const float R = Duration - T; S = Total - 0.5f * V * R * R / A; }
		FVector Loc, Dir;
		NoclipWaitingWalk::SamplePath(P, S, Loc, Dir);
		return Loc;
	};

	float WorstSeparation = BIG_NUMBER;
	float WorstJump = 0.f;
	for (int32 Mask = 1; Mask < (1 << Slots); ++Mask)
	{
		float Start[6];
		FNoclipTestAccess::ComputeSchedule(Room, static_cast<uint8>(Mask), Start);

		// Porta: um de cada vez, com folga; o fundo da cabine entra antes da frente.
		TArray<TPair<float, int32>> DoorTimes;
		float Finish = 0.f;
		for (int32 Slot = 0; Slot < Slots; ++Slot)
		{
			if (Mask & (1 << Slot))
			{
				const float Len = FNoclipTestAccess::RouteLength(Room, Slot);
				DoorTimes.Emplace(Start[Slot] + FNoclipTestAccess::TimeAtDistance(Room, FNoclipTestAccess::RouteDoorDistance(Room, Slot), Len), Slot);
				Finish = FMath::Max(Finish, Start[Slot] + FNoclipTestAccess::WalkDuration(Room, Len));
				TestTrue(FString::Printf(TEXT("Mascara %d lugar %d: comeca depois do atraso"), Mask, Slot), Start[Slot] >= Room->WalkStartDelay - KINDA_SMALL_NUMBER);
			}
		}
		DoorTimes.Sort([](const TPair<float, int32>& A, const TPair<float, int32>& B) { return A.Key < B.Key; });
		for (int32 i = 1; i < DoorTimes.Num(); ++i)
		{
			const float Gap = DoorTimes[i].Key - DoorTimes[i - 1].Key;
			if (Gap < DoorGap - 0.01f)
			{
				AddError(FString::Printf(TEXT("Mascara %d: lugares %d e %d passam a porta com %.2f s de diferenca (minimo %.2f)"), Mask, DoorTimes[i - 1].Value, DoorTimes[i].Value, Gap, DoorGap));
			}
			if (FNoclipTestAccess::RouteRow(Room, DoorTimes[i].Value) < FNoclipTestAccess::RouteRow(Room, DoorTimes[i - 1].Value))
			{
				AddError(FString::Printf(TEXT("Mascara %d: lugar %d (fundo) entra depois do lugar %d (frente)"), Mask, DoorTimes[i].Value, DoorTimes[i - 1].Value));
			}
		}
		TestTrue(FString::Printf(TEXT("Mascara %d: embarque termina em menos de 15 s (%.1f s)"), Mask, Finish), Finish < 15.f);

		// Ninguem atropela ninguem e ninguem "teleporta".
		FVector Prev[6];
		for (int32 Slot = 0; Slot < Slots; ++Slot)
		{
			Prev[Slot] = PositionAt(Slot, 0.f, Start);
		}
		for (float Clock = 0.f; Clock <= Finish + 0.5f; Clock += 1.f / 60.f)
		{
			FVector Now[6];
			for (int32 Slot = 0; Slot < Slots; ++Slot)
			{
				if (!(Mask & (1 << Slot)))
				{
					continue;
				}
				Now[Slot] = PositionAt(Slot, Clock, Start);
				const float Step = FVector::Dist2D(Now[Slot], Prev[Slot]);
				WorstJump = FMath::Max(WorstJump, Step);
				if (Step > V / 60.f * 1.3f + 0.5f)
				{
					AddError(FString::Printf(TEXT("Mascara %d lugar %d: pulou %.1f cm num quadro (t=%.2f)"), Mask, Slot, Step, Clock));
				}
				Prev[Slot] = Now[Slot];
			}
			for (int32 A = 0; A < Slots; ++A)
			{
				for (int32 B = A + 1; B < Slots; ++B)
				{
					if ((Mask & (1 << A)) && (Mask & (1 << B)))
					{
						const float D = FVector::Dist2D(Now[A], Now[B]);
						WorstSeparation = FMath::Min(WorstSeparation, D);
						if (D < MinSeparation)
						{
							AddError(FString::Printf(TEXT("Mascara %d: lugares %d e %d a %.0f cm um do outro (t=%.2f)"), Mask, A, B, D, Clock));
						}
					}
				}
			}
		}
	}
	AddInfo(FString::Printf(TEXT("63 combinacoes: menor distancia entre bonecos %.0f cm, maior passo por quadro %.1f cm."), WorstSeparation, WorstJump));
	return true;
}

// =====================================================================================================
// Som e regras do turno
// =====================================================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNoclipSoundAssetsTest, "NOCLIP.Unit.Audio.SoundAssets", NOCLIP_UNIT_TEST_FLAGS)
bool FNoclipSoundAssetsTest::RunTest(const FString& Parameters)
{
	using namespace NoclipUnitTests;
	TArray<FSoftObjectPath> Paths;
	NoclipSounds::CollectStaticPaths(Paths);
	TestTrue(TEXT("Lista de sons preenchida"), Paths.Num() > 40);
	int32 Missing = 0;
	for (const FSoftObjectPath& Path : Paths)
	{
		if (!Cast<USoundBase>(Path.TryLoad()))
		{
			++Missing;
			AddError(FString::Printf(TEXT("Som nao encontrado: %s"), *Path.ToString()));
		}
	}
	AddInfo(FString::Printf(TEXT("%d sons conferidos, %d faltando."), Paths.Num(), Missing));
	return Missing == 0;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNoclipShiftRulesTest, "NOCLIP.Unit.Game.ShiftRules", NOCLIP_UNIT_TEST_FLAGS)
bool FNoclipShiftRulesTest::RunTest(const FString& Parameters)
{
	using namespace NoclipUnitTests;
	const ANoclipGameMode* GM = GetDefault<ANoclipGameMode>();
	int32 Prev = 0;
	for (int32 Shift = 1; Shift <= 12; ++Shift)
	{
		const int32 Quota = GM->GetQuotaForShift(Shift);
		TestTrue(FString::Printf(TEXT("Quota do turno %d (%d) cresce"), Shift, Quota), Quota > Prev);
		Prev = Quota;
	}
	TestEqual(TEXT("Turno 0 usa a quota do 1"), GM->GetQuotaForShift(0), GM->GetQuotaForShift(1));
	TestTrue(TEXT("Colapso menor que o turno"), GM->CollapseSeconds < GM->ShiftDurationSeconds);
	TestTrue(TEXT("Primeiro apagao cabe no turno (antes do colapso)"), GM->FirstBlackoutDelay.Y < GM->ShiftDurationSeconds - GM->CollapseSeconds - 75.f);
	TestTrue(TEXT("Intervalos do apagao validos (min <= max)"), GM->FirstBlackoutDelay.X <= GM->FirstBlackoutDelay.Y && GM->BlackoutInterval.X <= GM->BlackoutInterval.Y);
	TestTrue(TEXT("Intervalo das divisorias valido"), GM->RoomShiftInterval.X > 0.f && GM->RoomShiftInterval.X <= GM->RoomShiftInterval.Y);
	TestTrue(TEXT("Equipe inteira morta: espera curta antes do fim"), GM->TeamWipeDelay > 0.f && GM->TeamWipeDelay < 15.f);
	return true;
}

// =====================================================================================================
// Estagiarios jogaveis
// =====================================================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNoclipCharactersTest, "NOCLIP.Unit.Characters.Playable", NOCLIP_UNIT_TEST_FLAGS)
bool FNoclipCharactersTest::RunTest(const FString& Parameters)
{
	TestTrue(TEXT("Ha estagiarios jogaveis"), NoclipCharacters::NumPlayable() > 0);
	TSet<FName> Ids;
	for (int32 i = 0; i < NoclipCharacters::NumSlots(); ++i)
	{
		const FNoclipCharacterDef& D = NoclipCharacters::Get(i);
		const FString Who = D.Id.ToString();
		TestFalse(Who + TEXT(": Id repetido"), Ids.Contains(D.Id));
		Ids.Add(D.Id);
		if (i < NoclipCharacters::NumPlayable())
		{
			TestFalse(Who + TEXT(": jogavel depois de uma vaga trancada (as jogaveis vem primeiro)"), D.bLocked);
		}
		if (D.bLocked)
		{
			continue;
		}
		TestFalse(Who + TEXT(": sem nome"), D.Name.IsEmpty() || D.Short.IsEmpty() || D.Plate.IsEmpty());
		TestEqual(Who + TEXT(": ficha com 6 dados"), D.Facts.Num(), 6);

		USkeletalMesh* Mesh = NoclipCharacters::LoadMesh(i);
		if (!TestNotNull(FString::Printf(TEXT("%s: modelo carrega (%s)"), *Who, *D.MeshPath), Mesh))
		{
			continue;
		}
		const USkeleton* Skeleton = Mesh->GetSkeleton();
		TestNotNull(Who + TEXT(": physics asset (corpo cai em ragdoll ao morrer)"), Mesh->GetPhysicsAsset());
		const float Height = Mesh->GetBounds().BoxExtent.Z * 2.f;
		TestTrue(FString::Printf(TEXT("%s: altura plausivel (%.0f cm)"), *Who, Height), Height > 140.f && Height < 220.f);

		// Ossos que o Control Rig das maos e a cabeca usam.
		const FReferenceSkeleton& Ref = Mesh->GetRefSkeleton();
		for (const FName Bone : { D.UpperArmR, D.UpperArmL, D.HandR, D.HandL, D.Head })
		{
			TestTrue(FString::Printf(TEXT("%s: osso %s existe no modelo"), *Who, *Bone.ToString()), Ref.FindBoneIndex(Bone) != INDEX_NONE);
		}
		TestNotNull(FString::Printf(TEXT("%s: Control Rig das maos carrega (%s)"), *Who, *D.HandsRigClassPath), NoclipCharacters::LoadHandsRigClass(i));

		for (int32 a = 0; a < static_cast<int32>(ENoclipCharacterAnim::Count); ++a)
		{
			UAnimSequence* Anim = NoclipCharacters::LoadAnim(i, static_cast<ENoclipCharacterAnim>(a));
			if (!TestNotNull(FString::Printf(TEXT("%s: animacao %d carrega (%s)"), *Who, a, *D.AnimPaths[a]), Anim))
			{
				continue;
			}
			TestTrue(FString::Printf(TEXT("%s: %s usa o esqueleto do modelo"), *Who, *Anim->GetName()), Anim->GetSkeleton() == Skeleton);
			TestFalse(FString::Printf(TEXT("%s: %s sem root motion (o Mover move a capsula)"), *Who, *Anim->GetName()), Anim->bEnableRootMotion);
		}
	}
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
