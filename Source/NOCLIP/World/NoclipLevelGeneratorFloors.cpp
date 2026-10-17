// Copyright Fuly Games. All Rights Reserved.
//
// Andares, noclip e salas que mudam do gerador do Level 0 (o resto esta em NoclipLevelGenerator.cpp).
//  - Level 0 (andar 0): algumas paredes finas viram paredes falsas (sem colisao) e alguns trechos de carpete
//    viram pisos falsos. Quem atravessa cai no subsolo.
//  - Subsolo (andar 1, SubFloorDepth abaixo): outro labirinto, mais escuro, sem entidades, com achados extras,
//    saidas de servico (voltam para perto da Sala Manila) e paredes falsas que devolvem ao Level 0.
//  - Divisorias que mudam: tampoes que trocam de porta durante o turno (o servidor decide; GameState replica).

#include "World/NoclipLevelGenerator.h"
#include "World/NoclipPowerGenerator.h"
#include "Audio/NoclipAudioSubsystem.h"
#include "NOCLIP.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/TextRenderComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "DrawDebugHelpers.h"
#include "HAL/IConsoleManager.h"

static TAutoConsoleVariable<int32> CVarNoclipDebugGlitches(TEXT("noclip.DebugGlitches"), 0,
	TEXT("Desenha as paredes/pisos falsos (vermelho), saidas de servico (verde) e divisorias que mudam (amarelo)."));

// ===================================================================== Paredes pendentes / falsas

void ANoclipLevelGenerator::FlushWalls(int32 Floor, FRandomStream& GlitchStream)
{
	const int32 WantFake = Floor == 0 ? (bGenerateSubFloor ? FakeWallCount : 0) : UpWallCount;

	// Candidatas: paredes finas internas, longe do spawn, da borda e da Sala Manila.
	TArray<int32> Candidates;
	if (WantFake > 0 && GeneratedBounds.bIsValid)
	{
		const FBox2D Inner = GeneratedBounds.ExpandBy(-90.f);
		for (int32 i = 0; i < PendingWalls.Num(); ++i)
		{
			const FPendingWall& W = PendingWalls[i];
			const float Along = FMath::Max(W.Size.X, W.Size.Y);
			const float Thin = FMath::Min(W.Size.X, W.Size.Y);
			const FVector2D C(W.Center);
			if (Along < 150.f || Along > 700.f || Thin > WallThickness * 1.6f || !Inner.IsInside(C))
			{
				continue;
			}
			if (FVector2D::Distance(C, Origin2D()) < 1400.f)
			{
				continue;
			}
			if (Floor == 0 && ExtractionRect.bIsValid && ExtractionRect.ExpandBy(500.f).IsInside(C))
			{
				continue;
			}
			Candidates.Add(i);
		}
	}
	for (int32 i = Candidates.Num() - 1; i > 0; --i)
	{
		Candidates.Swap(i, GlitchStream.RandRange(0, i));
	}
	// Espalhadas: nada de duas falsas a menos de 15 m uma da outra.
	TSet<int32> Fake;
	TArray<FVector2D> Picked;
	for (const int32 Index : Candidates)
	{
		if (Fake.Num() >= WantFake)
		{
			break;
		}
		const FVector2D C(PendingWalls[Index].Center);
		if (Picked.ContainsByPredicate([&C](const FVector2D& O) { return FVector2D::Distance(O, C) < 1500.f; }))
		{
			continue;
		}
		Picked.Add(C);
		Fake.Add(Index);
	}

	for (int32 i = 0; i < PendingWalls.Num(); ++i)
	{
		const FPendingWall& W = PendingWalls[i];
		if (Fake.Contains(i))
		{
			AddBox(FakeWallInstances, W.Center, W.Size);
			FNoclipGlitch G;
			G.Box = FBox(W.Center - W.Size * 0.5f, W.Center + W.Size * 0.5f).ExpandBy(FVector(18.f, 18.f, 0.f));
			G.Kind = Floor == 0 ? ENoclipGlitchKind::FakeWall : ENoclipGlitchKind::UpWall;
			Glitches.Add(G);
		}
		else
		{
			AddBox(WallInstances, W.Center, W.Size);
		}
	}
	PendingWalls.Reset();
}

bool ANoclipLevelGenerator::IsAreaClear(const FBox2D& Area) const
{
	for (const FBox2D& O : FloorObstacles)
	{
		if (O.Intersect(Area))
		{
			return false;
		}
	}
	return true;
}

void ANoclipLevelGenerator::AddFakeFloors(FRandomStream& GlitchStream)
{
	if (!GeneratedBounds.bIsValid || FakeFloorCount <= 0 || !bGenerateSubFloor)
	{
		return;
	}
	constexpr float Half = 85.f;
	TArray<FVector2D> Spots;
	for (int32 Try = 0; Try < 90 && Spots.Num() < FakeFloorCount; ++Try)
	{
		const FVector2D P(GlitchStream.FRandRange(GeneratedBounds.Min.X + 300.f, GeneratedBounds.Max.X - 300.f),
			GlitchStream.FRandRange(GeneratedBounds.Min.Y + 300.f, GeneratedBounds.Max.Y - 300.f));
		if (FVector2D::Distance(P, Origin2D()) < 1500.f || (ExtractionRect.bIsValid && ExtractionRect.ExpandBy(700.f).IsInside(P)))
		{
			continue;
		}
		if (Spots.ContainsByPredicate([&P](const FVector2D& O) { return FVector2D::Distance(O, P) < 1800.f; }))
		{
			continue;
		}
		// No meio do caminho, nao encostado em parede (da para pisar sem querer).
		if (!IsAreaClear(FBox2D(P - FVector2D(Half + 70.f), P + FVector2D(Half + 70.f))))
		{
			continue;
		}
		Spots.Add(P);
		// Remendo do mesmo carpete, girado 90 graus: a emenda nao bate (quem presta atencao percebe).
		GlitchFloorInstances->AddInstance(FTransform(FRotator(0.f, 90.f, 0.f), ToWorld(P, 0.4f), FVector(Half * 2.f / 100.f, Half * 2.f / 100.f, 0.008f)), true);
		FNoclipGlitch G;
		G.Box = FBox(ToWorld(P - FVector2D(Half), -10.f), ToWorld(P + FVector2D(Half), 140.f));
		G.Kind = ENoclipGlitchKind::FakeFloor;
		Glitches.Add(G);
	}
}

// ===================================================================== Subsolo

void ANoclipLevelGenerator::GenerateSubFloor(int32 Seed)
{
	if (!GeneratedBounds.bIsValid)
	{
		return;
	}
	PendingWalls.Reset();
	FloorObstacles.Reset();
	FloorOffset = -SubFloorDepth;

	FRandomStream Sub(Seed * 7 + 101);
	FRandomStream SubGlitch(Seed * 13 + 77);
	const FBox2D Area = GeneratedBounds;

	// Outro labirinto no mesmo contorno (menos dividido: saloes maiores e vazios).
	TArray<FBox2D> Leaves;
	const int32 SavedDepth = MaxSplitDepth;
	MaxSplitDepth = FMath::Max(2, MaxSplitDepth - 1);
	SplitZone(Sub, Area, 0, Leaves);
	MaxSplitDepth = SavedDepth;
	for (const FBox2D& Leaf : Leaves)
	{
		FillZone(Sub, Leaf, PickFill(Sub));
	}
	const float T = WallThickness * 2.f;
	AddWallSegment(FVector2D(Area.Min.X, Area.Min.Y), FVector2D(Area.Max.X, Area.Min.Y), T);
	AddWallSegment(FVector2D(Area.Min.X, Area.Max.Y), FVector2D(Area.Max.X, Area.Max.Y), T);
	AddWallSegment(FVector2D(Area.Min.X, Area.Min.Y), FVector2D(Area.Min.X, Area.Max.Y), T);
	AddWallSegment(FVector2D(Area.Max.X, Area.Min.Y), FVector2D(Area.Max.X, Area.Max.Y), T);

	// Saidas de servico: nas salas mais afastadas umas das outras (a primeira, a mais longe do spawn).
	TArray<FVector2D> Exits;
	for (int32 e = 0; e < ServiceExitCount && Leaves.Num() > 0; ++e)
	{
		int32 BestLeaf = INDEX_NONE;
		float BestScore = -1.f;
		for (int32 i = 0; i < Leaves.Num(); ++i)
		{
			const FVector2D C = Leaves[i].GetCenter();
			float Score = Exits.Num() == 0 ? FVector2D::Distance(C, Origin2D()) : TNumericLimits<float>::Max();
			for (const FVector2D& X : Exits)
			{
				Score = FMath::Min(Score, FVector2D::Distance(C, X));
			}
			if (Score > BestScore)
			{
				BestScore = Score;
				BestLeaf = i;
			}
		}
		const FBox2D Leaf = Leaves[BestLeaf];
		FVector2D Spot = Leaf.GetCenter();
		for (int32 Try = 0; Try < 30; ++Try)
		{
			const FVector2D P(Sub.FRandRange(Leaf.Min.X + 200.f, FMath::Max(Leaf.Min.X + 200.f, Leaf.Max.X - 200.f)),
				Sub.FRandRange(Leaf.Min.Y + 200.f, FMath::Max(Leaf.Min.Y + 200.f, Leaf.Max.Y - 200.f)));
			if (IsAreaClear(FBox2D(P - FVector2D(140.f), P + FVector2D(140.f))))
			{
				Spot = P;
				break;
			}
		}
		Exits.Add(Spot);
		AddServiceExit(Spot, 90.f * Sub.RandRange(0, 3));
		Leaves.RemoveAt(BestLeaf);
	}

	FlushWalls(1, SubGlitch);
	AddFloorAndCeiling(Area);
	// Mais escuro que o Level 0.
	const float SavedChance = LightChance;
	LightChance *= 0.4f;
	AddLights(Sub, Area);
	LightChance = SavedChance;

	FloorOffset = 0.f;
	FloorObstacles.Reset();
	bHasSubFloor = true;
}

void ANoclipLevelGenerator::AddServiceExit(const FVector2D& Center, float Yaw)
{
	const FRotator Rot(0.f, Yaw, 0.f);
	const FVector Base = ToWorld(Center, 0.f);
	auto Local = [&](float X, float Y, float Z) { return Base + Rot.RotateVector(FVector(X, Y, Z)); };
	auto Box = [&](const FVector& LocalCenter, const FVector& Size)
	{
		ExitInstances->AddInstance(FTransform(Rot, Local(LocalCenter.X, LocalCenter.Y, LocalCenter.Z), Size / 100.f), true);
	};
	// Porta de hospital (Assets/Hospital/door): batentes e verga do modelo, girados para o vao ficar no eixo Y local.
	// Atras, um fundo escuro (a porta "abre" para o escuro da escada de servico).
	const FRotator MeshRot = Rot + FRotator(0.f, 90.f, 0.f);
	auto AddMesh = [&](const TCHAR* Path, bool bCollide)
	{
		UStaticMesh* M = LoadObject<UStaticMesh>(nullptr, Path);
		if (!M)
		{
			return false;
		}
		UStaticMeshComponent* C = NewObject<UStaticMeshComponent>(this);
		C->SetupAttachment(RootComponent);
		C->SetStaticMesh(M);
		C->SetWorldLocationAndRotation(Base, MeshRot);
		C->SetCollisionProfileName(bCollide ? TEXT("BlockAll") : TEXT("NoCollision"));
		C->SetCanEverAffectNavigation(false);
		C->RegisterComponent();
		SpawnedExtras.Add(C);
		return true;
	};
	const bool bDoor = AddMesh(TEXT("/Game/NOCLIP/Assets/Hospital/door/StaticMeshes/door.door"), true)
		& AddMesh(TEXT("/Game/NOCLIP/Assets/Hospital/door/StaticMeshes/door_casing_r.door_casing_r"), true)
		& AddMesh(TEXT("/Game/NOCLIP/Assets/Hospital/door/StaticMeshes/door_casing_top.door_casing_top"), true);
	AddMesh(TEXT("/Game/NOCLIP/Assets/Hospital/exit_sign/StaticMeshes/exit_sign.exit_sign"), false);
	if (!bDoor)
	{
		// Sem o modelo: batente de caixas.
		Box(FVector(0.f, -55.f, 115.f), FVector(20.f, 12.f, 230.f));
		Box(FVector(0.f, 55.f, 115.f), FVector(20.f, 12.f, 230.f));
		Box(FVector(0.f, 0.f, 232.f), FVector(20.f, 122.f, 14.f));
	}
	// Paredinha dos dois lados (a porta nao fica solta no meio da sala) e o fundo escuro.
	Box(FVector(-4.f, -120.f, 145.f), FVector(16.f, 130.f, 290.f));
	Box(FVector(-4.f, 120.f, 145.f), FVector(16.f, 130.f, 290.f));
	Box(FVector(0.f, 0.f, 258.f), FVector(16.f, 110.f, 64.f));
	Box(FVector(-20.f, 0.f, 113.f), FVector(6.f, 104.f, 226.f));
	FloorObstacles.Add(FBox2D(Center - FVector2D(190.f), Center + FVector2D(190.f)));

	UTextRenderComponent* Sign = NewObject<UTextRenderComponent>(this);
	Sign->SetupAttachment(RootComponent);
	Sign->SetWorldLocation(Local(14.f, 0.f, 240.f));
	Sign->SetWorldRotation(Rot);
	Sign->SetHorizontalAlignment(EHTA_Center);
	Sign->SetVerticalAlignment(EVRTA_TextCenter);
	Sign->SetWorldSize(11.f);
	Sign->SetTextRenderColor(FColor(90, 255, 130));
	Sign->SetText(NSLOCTEXT("NoclipLevel", "ServiceExit", "SAÍDA DE SERVIÇO ↑"));
	Sign->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Sign->RegisterComponent();
	SpawnedExtras.Add(Sign);

	// Luz de emergencia verde (fica acesa no apagao: nao esta na lista das lampadas).
	UPointLightComponent* Light = NewObject<UPointLightComponent>(this);
	Light->SetupAttachment(RootComponent);
	Light->SetMobility(EComponentMobility::Movable);
	Light->SetWorldLocation(Local(60.f, 0.f, 200.f));
	Light->SetIntensityUnits(ELightUnits::Candelas);
	Light->SetIntensity(5.f);
	Light->SetAttenuationRadius(520.f);
	Light->SetLightColor(FLinearColor(0.35f, 1.f, 0.5f));
	Light->SetCastShadows(false);
	Light->RegisterComponent();
	SpawnedExtras.Add(Light);

	ServiceExits.Add(Local(30.f, 0.f, 0.f));

	// Gatilho: o vao da porta (por tras o fundo bloqueia). Yaw multiplo de 90: a caixa alinhada bate certinho.
	const FVector A = Local(-15.f, -42.f, 0.f);
	const FVector B = Local(40.f, 42.f, 220.f);
	FNoclipGlitch G;
	G.Box = FBox(A.ComponentMin(B), A.ComponentMax(B));
	G.Kind = ENoclipGlitchKind::ServiceExit;
	Glitches.Add(G);
}

// ===================================================================== Destinos

int32 ANoclipLevelGenerator::FindGlitchAt(const FVector& Location) const
{
	for (int32 i = 0; i < Glitches.Num(); ++i)
	{
		if (Glitches[i].Box.IsInsideOrOn(Location))
		{
			return i;
		}
	}
	return INDEX_NONE;
}

bool ANoclipLevelGenerator::FindWalkable(int32 Floor, const FVector2D& Near, float SearchRadius, FVector& OutLocation) const
{
	const UWorld* World = GetWorld();
	if (!World || !GeneratedBounds.bIsValid)
	{
		return false;
	}
	const FBox2D Inner = GeneratedBounds.ExpandBy(-120.f);
	const FCollisionShape Capsule = FCollisionShape::MakeCapsule(38.f, 88.f);
	// Espiral (angulo de ouro): perto primeiro, cada vez mais longe.
	for (int32 k = 0; k < 32; ++k)
	{
		const float Radius = k == 0 ? 0.f : SearchRadius * FMath::Sqrt(k / 31.f);
		const float Angle = k * 2.39996f;
		const FVector2D P = Near + FVector2D(FMath::Cos(Angle), FMath::Sin(Angle)) * Radius;
		if (!Inner.IsInside(P))
		{
			continue;
		}
		const FVector L(P.X, P.Y, GetFloorZ(Floor) + 94.f);
		if (!World->OverlapBlockingTestByChannel(L, FQuat::Identity, ECC_Pawn, Capsule))
		{
			OutLocation = L;
			return true;
		}
	}
	return false;
}

bool ANoclipLevelGenerator::ResolveGlitchDestination(int32 GlitchIndex, const FVector& From, FVector& OutLocation) const
{
	if (!Glitches.IsValidIndex(GlitchIndex) || !GeneratedBounds.bIsValid)
	{
		return false;
	}
	auto RandomOn = [this](int32 Floor, float AvoidManila, FVector& Out)
	{
		const FBox2D Inner = GeneratedBounds.ExpandBy(-250.f);
		for (int32 Try = 0; Try < 40; ++Try)
		{
			const FVector2D P(FMath::FRandRange(Inner.Min.X, Inner.Max.X), FMath::FRandRange(Inner.Min.Y, Inner.Max.Y));
			if (AvoidManila > 0.f && ExtractionRect.bIsValid && FVector2D::Distance(P, ExtractionRect.GetCenter()) < AvoidManila)
			{
				continue;
			}
			if (FindWalkable(Floor, P, 0.f, Out))
			{
				return true;
			}
		}
		return FindWalkable(Floor, Origin2D(), 1500.f, Out);
	};

	switch (Glitches[GlitchIndex].Kind)
	{
	case ENoclipGlitchKind::FakeWall:
		// Atravessou a parede: acorda num canto qualquer do subsolo.
		return RandomOn(1, 0.f, OutLocation);
	case ENoclipGlitchKind::FakeFloor:
		// Afundou no carpete: cai logo abaixo (um pouco acima do piso do subsolo).
		if (FindWalkable(1, FVector2D(From), 700.f, OutLocation) || RandomOn(1, 0.f, OutLocation))
		{
			OutLocation.Z = GetFloorZ(1) + 190.f;
			return true;
		}
		return false;
	case ENoclipGlitchKind::ServiceExit:
	{
		// Escada de servico: sai do lado da Sala Manila, virado para ela.
		if (!ExtractionRect.bIsValid)
		{
			return RandomOn(0, 0.f, OutLocation);
		}
		const FVector2D C = ExtractionRect.GetCenter();
		const FVector2D Dir = (Origin2D() - C).GetSafeNormal();
		const FVector2D Near = C + (Dir.IsNearlyZero() ? FVector2D(1.f, 0.f) : Dir) * (ExtractionRect.GetExtent().GetMax() + 180.f);
		return FindWalkable(0, Near, 500.f, OutLocation) || FindWalkable(0, C, 600.f, OutLocation);
	}
	case ENoclipGlitchKind::UpWall:
		// Parede falsa do subsolo: devolve para longe da Sala Manila (de graca nao e).
		return RandomOn(0, 2500.f, OutLocation);
	}
	return false;
}

// ===================================================================== Salas que mudam

void ANoclipLevelGenerator::AddShifterPlug(const FVector2D& PlugA, const FVector2D& PlugB, const FVector2D& Dir, float DoorWidth, float Thickness)
{
	// Nao mexe perto do spawn (o elevador precisa de saida).
	const float Keep = SpawnClearRadius + DoorWidth;
	if (FVector2D::Distance(PlugA, Origin2D()) < Keep || FVector2D::Distance(PlugB, Origin2D()) < Keep)
	{
		return;
	}
	const bool bAlongX = FMath::Abs(Dir.X) >= FMath::Abs(Dir.Y);
	const FVector Size = bAlongX ? FVector(DoorWidth + Thickness, Thickness, WallHeight) : FVector(Thickness, DoorWidth + Thickness, WallHeight);
	FNoclipShifter S;
	S.CenterA = ToWorld(PlugA, WallHeight * 0.5f);
	S.CenterB = ToWorld(PlugB, WallHeight * 0.5f);
	S.Size = Size;
	Shifters.Add(S);
	// Duas instancias por divisoria: tampao na porta A e na porta B (a que nao vale fica escondida).
	ShiftInstances->AddInstance(FTransform(FQuat::Identity, S.CenterA, Size / 100.f), true);
	// Escondida = so muito abaixo, no tamanho certo: a colisao (Chaos) nao volta do tamanho se a escala mudar.
	ShiftInstances->AddInstance(FTransform(FQuat::Identity, S.CenterB - FVector(0.f, 0.f, 100000.f), Size / 100.f), true);
}

void ANoclipLevelGenerator::ApplyShiftMask(uint32 Mask, bool bAnnounce)
{
	const uint32 Changed = Mask ^ AppliedShiftMask;
	UNoclipAudioSubsystem* Audio = bAnnounce ? UNoclipAudioSubsystem::Get(this) : nullptr;
	const int32 Count = FMath::Min(Shifters.Num(), 32);
	for (int32 i = 0; i < Count; ++i)
	{
		const FNoclipShifter& S = Shifters[i];
		const bool bAtB = ((Mask >> i) & 1u) != 0;
		const FTransform Real(FQuat::Identity, bAtB ? S.CenterB : S.CenterA, S.Size / 100.f);
		const FTransform Hidden(FQuat::Identity, (bAtB ? S.CenterA : S.CenterB) - FVector(0.f, 0.f, 100000.f), S.Size / 100.f);
		if (ShiftInstances->GetInstanceCount() > i * 2 + 1)
		{
			ShiftInstances->UpdateInstanceTransform(i * 2, bAtB ? Hidden : Real, true, false, true);
			ShiftInstances->UpdateInstanceTransform(i * 2 + 1, bAtB ? Real : Hidden, true, false, true);
		}
		if (Audio && ((Changed >> i) & 1u) != 0)
		{
			Audio->PlayWallShift((S.CenterA + S.CenterB) * 0.5f);
		}
	}
	if (ShiftInstances)
	{
		ShiftInstances->MarkRenderStateDirty();
	}
	AppliedShiftMask = Mask;
}

// ===================================================================== Gerador de energia

void ANoclipLevelGenerator::SpawnPowerGenerator(FRandomStream& Stream)
{
	if (!GeneratedBounds.bIsValid || GetNetMode() == NM_Client)
	{
		return;
	}
	// Longe da Sala Manila e do elevador: religar a energia e uma viagem.
	const FBox2D Inner = GeneratedBounds.ExpandBy(-400.f);
	const FVector2D Manila = ExtractionRect.bIsValid ? ExtractionRect.GetCenter() : Origin2D();
	FVector Best = FVector::ZeroVector;
	float BestScore = -1.f;
	const FCollisionShape Footprint = FCollisionShape::MakeBox(FVector(140.f, 140.f, 70.f)); // gerador de 2,3 m em qualquer direcao
	for (int32 Try = 0; Try < 70; ++Try)
	{
		const FVector2D P(Stream.FRandRange(Inner.Min.X, Inner.Max.X), Stream.FRandRange(Inner.Min.Y, Inner.Max.Y));
		if (ExtractionRect.bIsValid && ExtractionRect.ExpandBy(800.f).IsInside(P))
		{
			continue;
		}
		FVector L;
		if (!FindWalkable(0, P, 250.f, L))
		{
			continue;
		}
		const FVector BoxCenter(L.X, L.Y, GetFloorZ(0) + 80.f);
		if (GetWorld()->OverlapBlockingTestByChannel(BoxCenter, FQuat::Identity, ECC_WorldStatic, Footprint))
		{
			continue;
		}
		const float Score = FMath::Min(FVector2D::Distance(FVector2D(L), Manila), FVector2D::Distance(FVector2D(L), Origin2D())) * Stream.FRandRange(0.7f, 1.f);
		if (Score > BestScore)
		{
			BestScore = Score;
			Best = L;
		}
	}
	if (BestScore < 0.f)
	{
		UE_LOG(LogNoclip, Warning, TEXT("Gerador de energia: nenhum lugar livre encontrado."));
		return;
	}
	FActorSpawnParameters Params;
	Params.Owner = this;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	const FRotator Rot(0.f, 90.f * Stream.RandRange(0, 3), 0.f);
	PowerGenerator = GetWorld()->SpawnActor<ANoclipPowerGenerator>(ANoclipPowerGenerator::StaticClass(), FVector(Best.X, Best.Y, GetFloorZ(0)), Rot, Params);
	UE_LOG(LogNoclip, Log, TEXT("Gerador de energia em %s."), *Best.ToCompactString());
}

void ANoclipLevelGenerator::DebugDrawGlitches() const
{
	const UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	for (const FNoclipGlitch& G : Glitches)
	{
		const FColor Color = G.Kind == ENoclipGlitchKind::ServiceExit ? FColor::Green : (G.Kind == ENoclipGlitchKind::UpWall ? FColor::Cyan : FColor::Red);
		DrawDebugBox(World, G.Box.GetCenter(), G.Box.GetExtent(), Color, false, -1.f, 0, 3.f);
	}
	for (int32 i = 0; i < Shifters.Num(); ++i)
	{
		const FNoclipShifter& S = Shifters[i];
		const bool bAtB = ((AppliedShiftMask >> i) & 1u) != 0;
		DrawDebugBox(World, S.CenterA, S.Size * 0.5f, bAtB ? FColor(80, 80, 0) : FColor::Yellow, false, -1.f, 0, 2.f);
		DrawDebugBox(World, S.CenterB, S.Size * 0.5f, bAtB ? FColor::Yellow : FColor(80, 80, 0), false, -1.f, 0, 2.f);
	}
	if (PowerGenerator)
	{
		DrawDebugSphere(World, PowerGenerator->GetActorLocation() + FVector(0.f, 0.f, 150.f), 60.f, 12, FColor::Orange, false, -1.f, 0, 2.f);
	}
}
