// Copyright Fuly Games. All Rights Reserved.

#include "World/NoclipLevelGenerator.h"
#include "World/NoclipLootCatalog.h"
#include "World/NoclipLootActor.h"
#include "World/NoclipExtractionZone.h"
#include "World/NoclipPowerGenerator.h"
#include "Game/NoclipGameState.h"
#include "NOCLIP.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Engine/CollisionProfile.h"
#include "TimerManager.h"
#include "UObject/ConstructorHelpers.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(NoclipLevelGenerator)

namespace NoclipGen
{
	static UHierarchicalInstancedStaticMeshComponent* MakeHISM(AActor* Owner, USceneComponent* Root, const TCHAR* Name)
	{
		UHierarchicalInstancedStaticMeshComponent* Comp = Owner->CreateDefaultSubobject<UHierarchicalInstancedStaticMeshComponent>(Name);
		Comp->SetupAttachment(Root);
		Comp->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
		Comp->SetMobility(EComponentMobility::Static);
		return Comp;
	}

	static float SnapTo(float V, float Step)
	{
		return FMath::RoundToFloat(V / Step) * Step;
	}
}

ANoclipLevelGenerator::ANoclipLevelGenerator()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = false; // cada maquina gera a partir da seed

	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent->SetMobility(EComponentMobility::Static);

	FloorInstances = NoclipGen::MakeHISM(this, RootComponent, TEXT("FloorInstances"));
	CeilingInstances = NoclipGen::MakeHISM(this, RootComponent, TEXT("CeilingInstances"));
	WallInstances = NoclipGen::MakeHISM(this, RootComponent, TEXT("WallInstances"));
	LightPanelInstances = NoclipGen::MakeHISM(this, RootComponent, TEXT("LightPanelInstances"));
	LightPanelInstances->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	LightPanelInstances->SetCastShadow(false);
	FakeWallInstances = NoclipGen::MakeHISM(this, RootComponent, TEXT("FakeWallInstances"));
	FakeWallInstances->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	GlitchFloorInstances = NoclipGen::MakeHISM(this, RootComponent, TEXT("GlitchFloorInstances"));
	GlitchFloorInstances->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	GlitchFloorInstances->SetCastShadow(false);
	ShiftInstances = NoclipGen::MakeHISM(this, RootComponent, TEXT("ShiftInstances"));
	ShiftInstances->SetMobility(EComponentMobility::Movable); // os tampoes trocam de lugar no turno
	ExitInstances = NoclipGen::MakeHISM(this, RootComponent, TEXT("ExitInstances"));

	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (Cube.Succeeded())
	{
		CubeMesh = Cube.Object;
	}

	FillWeights.Add(ENoclipZoneFill::Open, 0.8f);
	FillWeights.Add(ENoclipZoneFill::Stripes, 2.5f);
	FillWeights.Add(ENoclipZoneFill::Comb, 1.5f);
	FillWeights.Add(ENoclipZoneFill::Pillars, 2.f);
	FillWeights.Add(ENoclipZoneFill::Blocks, 1.5f);
	FillWeights.Add(ENoclipZoneFill::Dashes, 1.5f);

	ExtractionZoneClass = ANoclipExtractionZone::StaticClass();
}

// ===================================================================== Ciclo de vida

void ANoclipLevelGenerator::BeginPlay()
{
	Super::BeginPlay();

	if (ANoclipGameState* GS = GetWorld()->GetGameState<ANoclipGameState>())
	{
		if (GS->HasLevelSeed())
		{
			HandleSeedReady(GS->GetLevelSeed());
		}
		GS->OnLevelSeedReady.AddUniqueDynamic(this, &ANoclipLevelGenerator::HandleSeedReady);
	}
	else if (GetNetMode() == NM_Client)
	{
		FTimerHandle Retry;
		GetWorldTimerManager().SetTimer(Retry, this, &ANoclipLevelGenerator::BeginPlayRetry, 0.2f, false);
	}
	else
	{
		UE_LOG(LogNoclip, Warning, TEXT("LevelGenerator: GameState nao e ANoclipGameState; gerando com seed 1337 para teste."));
		Generate(1337);
	}
}

void ANoclipLevelGenerator::BeginPlayRetry()
{
	if (ANoclipGameState* GS = GetWorld()->GetGameState<ANoclipGameState>())
	{
		if (GS->HasLevelSeed())
		{
			HandleSeedReady(GS->GetLevelSeed());
		}
		GS->OnLevelSeedReady.AddUniqueDynamic(this, &ANoclipLevelGenerator::HandleSeedReady);
	}
	else
	{
		FTimerHandle Retry;
		GetWorldTimerManager().SetTimer(Retry, this, &ANoclipLevelGenerator::BeginPlayRetry, 0.2f, false);
	}
}

void ANoclipLevelGenerator::HandleSeedReady(int32 Seed)
{
	if (Seed != GeneratedSeed)
	{
		Generate(Seed);
	}
}

void ANoclipLevelGenerator::ClearGenerated()
{
	FloorInstances->ClearInstances();
	CeilingInstances->ClearInstances();
	WallInstances->ClearInstances();
	LightPanelInstances->ClearInstances();
	FakeWallInstances->ClearInstances();
	GlitchFloorInstances->ClearInstances();
	ShiftInstances->ClearInstances();
	ExitInstances->ClearInstances();
	for (UActorComponent* Extra : SpawnedExtras)
	{
		if (Extra)
		{
			Extra->DestroyComponent();
		}
	}
	SpawnedExtras.Reset();
	PendingWalls.Reset();
	FloorObstacles.Reset();
	Glitches.Reset();
	Shifters.Reset();
	ServiceExits.Reset();
	AppliedShiftMask = 0;
	bHasSubFloor = false;
	FloorOffset = 0.f;

	for (UActorComponent* Light : SpawnedLights)
	{
		if (Light)
		{
			Light->DestroyComponent();
		}
	}
	SpawnedLights.Reset();

	ClearShiftContent();
	GeneratedSeed = INDEX_NONE;
	GeneratedBounds = FBox2D(ForceInit);
	ExtractionRect = FBox2D(ForceInit);
}

void ANoclipLevelGenerator::UpdateCollapseLights(bool bCollapse, float Progress, float TimeSeconds, bool bPowerOut, float PowerChangeTime)
{
	// Apagao: tudo cai (piscando no primeiro segundo e meio); a Sala Manila tem luz de emergencia.
	const float SincePower = TimeSeconds - PowerChangeTime;
	const bool bPowerFlicker = SincePower >= 0.f && SincePower < 1.6f;
	if (!bCollapse && !bAnyLightOff && !bPowerOut && !bPowerFlicker)
	{
		return;
	}
	const int32 NumLights = SpawnedLights.Num();
	if (CollapseCacheSeed != GeneratedSeed || LightOff.Num() != NumLights)
	{
		// Nivel novo: guarda a posicao original dos paineis (um painel por luz, na mesma ordem).
		CollapseCacheSeed = GeneratedSeed;
		LightOff.Init(false, NumLights);
		PanelOriginals.Reset(NumLights);
		for (int32 i = 0; i < NumLights; ++i)
		{
			FTransform T = FTransform::Identity;
			if (LightPanelInstances && i < LightPanelInstances->GetInstanceCount())
			{
				LightPanelInstances->GetInstanceTransform(i, T, true);
			}
			PanelOriginals.Add(T);
		}
		bAnyLightOff = false;
	}
	if (NumLights == 0)
	{
		return;
	}

	// Raio que ainda tem luz: comeca na borda mais distante e fecha ate a Sala Manila.
	const FVector2D Center = ExtractionRect.bIsValid ? ExtractionRect.GetCenter() : Origin2D();
	const float Keep = ExtractionRect.bIsValid ? ExtractionRect.GetExtent().Size() + 250.f : 600.f;
	float MaxDist = Keep;
	if (GeneratedBounds.bIsValid)
	{
		const FVector2D Corners[4] = { GeneratedBounds.Min, GeneratedBounds.Max, FVector2D(GeneratedBounds.Min.X, GeneratedBounds.Max.Y), FVector2D(GeneratedBounds.Max.X, GeneratedBounds.Min.Y) };
		for (const FVector2D& C : Corners) { MaxDist = FMath::Max(MaxDist, FVector2D::Distance(C, Center)); }
	}
	const float Radius = bCollapse ? FMath::Lerp(MaxDist + 1.f, Keep, FMath::Clamp(Progress, 0.f, 1.f)) : TNumericLimits<float>::Max();
	constexpr float FlickerBand = 450.f;

	bool bPanelsChanged = false;
	bAnyLightOff = false;
	for (int32 i = 0; i < NumLights; ++i)
	{
		ULightComponent* Light = Cast<ULightComponent>(SpawnedLights[i]);
		if (!Light)
		{
			continue;
		}
		const float Dist = FVector2D::Distance(FVector2D(Light->GetComponentLocation()), Center);
		bool bOff = Dist > Radius;
		if (!bOff && bCollapse && Dist > Radius - FlickerBand)
		{
			// Na borda do apagao as lampadas piscam (deterministico: igual para todos).
			const float Phase = FMath::Frac(TimeSeconds * 6.f + i * 0.618f);
			bOff = Phase < 0.25f + 0.5f * (1.f - (Radius - Dist) / FlickerBand);
		}
		if (bPowerOut || bPowerFlicker)
		{
			const bool bEmergency = ExtractionRect.bIsValid && ExtractionRect.ExpandBy(150.f).IsInside(FVector2D(Light->GetComponentLocation()))
				&& GetFloorAt(Light->GetComponentLocation()) == 0;
			bool bPowerOff = bPowerOut && !bEmergency;
			if (bPowerFlicker && !bEmergency)
			{
				// Lampadas engasgando: caindo (apagando) ou voltando (acendendo), deterministico.
				const float Phase = FMath::Frac(TimeSeconds * 9.f + i * 0.37f);
				const float Ramp = SincePower / 1.6f;
				bPowerOff = bPowerOut ? Phase < Ramp : Phase > Ramp;
			}
			bOff |= bPowerOff;
		}
		bAnyLightOff |= bOff;
		if (bOff == LightOff[i])
		{
			continue;
		}
		LightOff[i] = bOff;
		Light->SetVisibility(!bOff);
		if (LightPanelInstances && PanelOriginals.IsValidIndex(i) && i < LightPanelInstances->GetInstanceCount())
		{
			FTransform T = PanelOriginals[i];
			if (bOff)
			{
				// Painel apagado: some dentro do forro (HISM nao tem emissivo por instancia).
				T.SetScale3D(FVector(0.001f));
			}
			LightPanelInstances->UpdateInstanceTransform(i, T, true, false, true);
			bPanelsChanged = true;
		}
	}
	if (bPanelsChanged && LightPanelInstances)
	{
		LightPanelInstances->MarkRenderStateDirty();
	}
}

void ANoclipLevelGenerator::ClearShiftContent()
{
	// Atencao: este ator nao e replicado, entao HasAuthority() e verdadeiro tambem no cliente.
	if (GetNetMode() == NM_Client)
	{
		SpawnedLoot.Reset();
		ExtractionZone = nullptr;
		PowerGenerator = nullptr;
		return;
	}
	for (AActor* Loot : SpawnedLoot)
	{
		if (IsValid(Loot))
		{
			Loot->Destroy();
		}
	}
	SpawnedLoot.Reset();
	if (IsValid(ExtractionZone))
	{
		ExtractionZone->Destroy();
	}
	ExtractionZone = nullptr;
	if (IsValid(PowerGenerator))
	{
		PowerGenerator->Destroy();
	}
	PowerGenerator = nullptr;
}

void ANoclipLevelGenerator::SpawnShiftContent()
{
	if (GetNetMode() == NM_Client || GeneratedSeed == INDEX_NONE)
	{
		return;
	}
	ClearShiftContent();

	if (ExtractionRect.bIsValid && ExtractionZoneClass)
	{
		const FVector2D Center = ExtractionRect.GetCenter();
		const FVector2D Half = ExtractionRect.GetExtent();
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		Params.Owner = this;
		ExtractionZone = GetWorld()->SpawnActor<ANoclipExtractionZone>(ExtractionZoneClass, ToWorld(Center, WallHeight * 0.5f), FRotator::ZeroRotator, Params);
		if (ExtractionZone)
		{
			ExtractionZone->Setup(Half, WallHeight, ExtractionMarkerMaterial);
		}
	}

	FRandomStream LootStream(GeneratedSeed + 7919);
	SpawnLoot(LootStream);

	// Subsolo: alguns achados a mais (quem arrisca o noclip acha coisa boa).
	if (bHasSubFloor && SubFloorLootCount > 0 && bUseOfficeLoot && NoclipLoot::Num() > 0)
	{
		const int32 SavedCount = LootCount;
		LootCount = SubFloorLootCount;
		FloorOffset = -SubFloorDepth;
		FRandomStream SubLoot(GeneratedSeed + 15331);
		SpawnOfficeLoot(SubLoot);
		FloorOffset = 0.f;
		LootCount = SavedCount;
	}

	FRandomStream PowerStream(GeneratedSeed + 2207);
	SpawnPowerGenerator(PowerStream);
}

void ANoclipLevelGenerator::Generate(int32 Seed)
{
	ClearGenerated();

	if (!CubeMesh)
	{
		UE_LOG(LogNoclip, Error, TEXT("LevelGenerator: CubeMesh nao definido."));
		return;
	}

	for (UHierarchicalInstancedStaticMeshComponent* Comp : { FloorInstances.Get(), CeilingInstances.Get(), WallInstances.Get(), LightPanelInstances.Get(),
		FakeWallInstances.Get(), GlitchFloorInstances.Get(), ShiftInstances.Get(), ExitInstances.Get() })
	{
		Comp->SetStaticMesh(CubeMesh);
	}
	if (FloorMaterial) { FloorInstances->SetMaterial(0, FloorMaterial); GlitchFloorInstances->SetMaterial(0, FloorMaterial); }
	if (CeilingMaterial) { CeilingInstances->SetMaterial(0, CeilingMaterial); }
	if (WallMaterial) { WallInstances->SetMaterial(0, WallMaterial); FakeWallInstances->SetMaterial(0, WallMaterial); ShiftInstances->SetMaterial(0, WallMaterial); ExitInstances->SetMaterial(0, WallMaterial); }
	if (LightPanelMaterial) { LightPanelInstances->SetMaterial(0, LightPanelMaterial); }

	FRandomStream Stream(Seed);
	// Fluxos separados para as falhas e divisorias: o layout base continua o mesmo de antes para a mesma seed.
	ShiftStream.Initialize(Seed + 4242);
	FRandomStream GlitchStream(Seed + 9091);
	FloorOffset = 0.f;
	bAllowShifting = MaxShifters > 0;

	ENoclipLayoutMode Mode = LayoutMode;
	if (Mode == ENoclipLayoutMode::Random)
	{
		Mode = Stream.FRand() < 0.5f ? ENoclipLayoutMode::Zones : ENoclipLayoutMode::Linear;
	}

	switch (Mode)
	{
	case ENoclipLayoutMode::Grid:   GenerateGrid(Stream); break;
	case ENoclipLayoutMode::Linear: GenerateLinear(Stream); break;
	default:                        GenerateZones(Stream); break;
	}
	bAllowShifting = false;

	// Level 0: paredes (algumas falsas), pisos falsos, piso, teto e luz.
	FlushWalls(0, GlitchStream);
	AddFakeFloors(GlitchStream);
	AddFloorAndCeiling(GeneratedBounds);
	AddLights(Stream, GeneratedBounds);

	GeneratedSeed = Seed;
	GeneratedMode = Mode;

	// Subsolo (destino do noclip) e estado atual das divisorias.
	if (bGenerateSubFloor)
	{
		GenerateSubFloor(Seed);
	}
	const ANoclipGameState* ShiftGS = GetWorld()->GetGameState<ANoclipGameState>();
	ApplyShiftMask(ShiftGS ? ShiftGS->GetLayoutShiftMask() : 0u, false);

	UE_LOG(LogNoclip, Log, TEXT("Level 0 gerado: seed %d, modo %s, %d solidos, %d luzes, %d falhas, %d divisorias que mudam, subsolo %s"), Seed, *UEnum::GetValueAsString(Mode),
		WallInstances->GetInstanceCount(), SpawnedLights.Num(), Glitches.Num(), Shifters.Num(), bHasSubFloor ? TEXT("sim") : TEXT("nao"));
	OnGenerated(Seed);
}

// ===================================================================== Primitivas

void ANoclipLevelGenerator::AddBox(UHierarchicalInstancedStaticMeshComponent* Target, const FVector& Center, const FVector& SizeCm)
{
	// O cubo basico tem 100 cm e pivo no centro.
	Target->AddInstance(FTransform(FQuat::Identity, Center, SizeCm / 100.f), /*bWorldSpace*/ true);
}

void ANoclipLevelGenerator::AddSolid(const FVector2D& Center, const FVector2D& Size, float Height)
{
	// Distancia do spawn ate o retangulo.
	const FVector2D O = Origin2D();
	const FVector2D Half = Size * 0.5f;
	const FVector2D Closest(FMath::Clamp(O.X, Center.X - Half.X, Center.X + Half.X), FMath::Clamp(O.Y, Center.Y - Half.Y, Center.Y + Half.Y));
	if (FVector2D::DistSquared(O, Closest) < FMath::Square(SpawnClearRadius))
	{
		return;
	}
	AddBox(WallInstances, ToWorld(Center, Height * 0.5f), FVector(Size.X, Size.Y, Height));
	FloorObstacles.Add(FBox2D(Center - Half, Center + Half));
}

void ANoclipLevelGenerator::AddWallSegment(const FVector2D& From, const FVector2D& To, float Thickness)
{
	const FVector2D Delta = To - From;
	if (Delta.SizeSquared() < 1.f)
	{
		return;
	}

	const bool bAlongX = FMath::Abs(Delta.X) >= FMath::Abs(Delta.Y);
	const float Fixed = bAlongX ? From.Y : From.X;
	float A = bAlongX ? FMath::Min(From.X, To.X) : FMath::Min(From.Y, To.Y);
	float B = bAlongX ? FMath::Max(From.X, To.X) : FMath::Max(From.Y, To.Y);

	auto Emit = [&](float S, float E)
	{
		if (E - S < 5.f)
		{
			return;
		}
		const float Mid = (S + E) * 0.5f;
		const float Len = E - S + Thickness;
		const FVector2D C = bAlongX ? FVector2D(Mid, Fixed) : FVector2D(Fixed, Mid);
		const FVector Size = bAlongX ? FVector(Len, Thickness, WallHeight) : FVector(Thickness, Len, WallHeight);
		// Fica pendente: no fim do andar algumas viram paredes falsas (FlushWalls).
		PendingWalls.Add({ ToWorld(C, WallHeight * 0.5f), Size });
		FloorObstacles.Add(FBox2D(C - FVector2D(Size.X, Size.Y) * 0.5f, C + FVector2D(Size.X, Size.Y) * 0.5f));
	};

	// Recorta o trecho que atravessa o circulo de spawn.
	const FVector2D O = Origin2D();
	const float OFixed = bAlongX ? O.Y : O.X;
	const float OAlong = bAlongX ? O.X : O.Y;
	const float D = FMath::Abs(Fixed - OFixed);
	if (D < SpawnClearRadius)
	{
		const float HalfChord = FMath::Sqrt(FMath::Square(SpawnClearRadius) - FMath::Square(D));
		const float C0 = OAlong - HalfChord;
		const float C1 = OAlong + HalfChord;
		if (C1 > A && C0 < B)
		{
			Emit(A, FMath::Min(B, C0));
			Emit(FMath::Max(A, C1), B);
			return;
		}
	}
	Emit(A, B);
}

void ANoclipLevelGenerator::AddDashedLine(FRandomStream& Stream, const FVector2D& A, const FVector2D& B, float MinSeg, float MaxSeg, float MinGap, float MaxGap)
{
	const FVector2D Dir = (B - A).GetSafeNormal();
	const float Len = (B - A).Size();
	float Cursor = Stream.FRandRange(0.f, MinGap);
	while (Cursor < Len)
	{
		const float Seg = Stream.FRandRange(MinSeg, MaxSeg);
		const float End = FMath::Min(Cursor + Seg, Len);
		if (End - Cursor > MinSeg * 0.4f)
		{
			AddWallSegment(A + Dir * Cursor, A + Dir * End, WallThickness);
		}
		Cursor = End + Stream.FRandRange(MinGap, MaxGap);
	}
}

void ANoclipLevelGenerator::AddDividerWithDoors(FRandomStream& Stream, const FVector2D& A, const FVector2D& B, int32 DoorCount, float Thickness)
{
	const FVector2D Dir = (B - A).GetSafeNormal();
	const float Len = (B - A).Size();
	const float DoorW = DoorwayWidth * 1.3f;

	// Sala que muda: uma porta a mais e um tampao que alterna entre duas delas (sempre sobra DoorCount abertas).
	const bool bShift = bAllowShifting && Shifters.Num() < MaxShifters && Len > DoorW * (DoorCount + 1) * 2.2f
		&& ShiftStream.FRand() < ShiftDividerChance;
	const int32 Gaps = bShift ? DoorCount + 1 : DoorCount;

	// Portas em posicoes ordenadas e sem sobreposicao.
	TArray<float> Doors;
	for (int32 i = 0; i < Gaps; ++i)
	{
		const float Slot = Len / Gaps;
		const float Lo = Slot * i + DoorW;
		const float Hi = Slot * (i + 1) - DoorW;
		Doors.Add(Hi > Lo ? Stream.FRandRange(Lo, Hi) : Slot * (i + 0.5f));
	}

	float Cursor = 0.f;
	for (const float DoorCenter : Doors)
	{
		AddWallSegment(A + Dir * Cursor, A + Dir * (DoorCenter - DoorW * 0.5f), Thickness);
		Cursor = DoorCenter + DoorW * 0.5f;
	}
	AddWallSegment(A + Dir * Cursor, B, Thickness);

	if (bShift && Doors.Num() >= 2)
	{
		const int32 IA = ShiftStream.RandRange(0, Doors.Num() - 1);
		const int32 IB = (IA + 1 + ShiftStream.RandRange(0, Doors.Num() - 2)) % Doors.Num();
		AddShifterPlug(A + Dir * Doors[IA], A + Dir * Doors[IB], Dir, DoorW, Thickness);
	}
}

void ANoclipLevelGenerator::AddFloorAndCeiling(const FBox2D& Bounds)
{
	if (!Bounds.bIsValid)
	{
		return;
	}
	// Placas de ate ~8 m: triangulos gigantes geram artefatos e atrapalham o culling.
	const FVector2D Size = Bounds.GetSize();
	const int32 NX = FMath::Max(1, FMath::CeilToInt(Size.X / 800.f));
	const int32 NY = FMath::Max(1, FMath::CeilToInt(Size.Y / 800.f));
	const FVector2D Tile(Size.X / NX, Size.Y / NY);

	for (int32 Y = 0; Y < NY; ++Y)
	{
		for (int32 X = 0; X < NX; ++X)
		{
			const FVector2D C = Bounds.Min + FVector2D((X + 0.5f) * Tile.X, (Y + 0.5f) * Tile.Y);
			AddBox(FloorInstances, ToWorld(C, -10.f), FVector(Tile.X, Tile.Y, 20.f));
			AddBox(CeilingInstances, ToWorld(C, WallHeight + 10.f), FVector(Tile.X, Tile.Y, 20.f));
		}
	}
}

void ANoclipLevelGenerator::AddLights(FRandomStream& Stream, const FBox2D& Bounds)
{
	if (!Bounds.bIsValid)
	{
		return;
	}
	const FVector2D Size = Bounds.GetSize();
	const int32 NX = FMath::Max(1, FMath::FloorToInt(Size.X / LightSpacing));
	const int32 NY = FMath::Max(1, FMath::FloorToInt(Size.Y / LightSpacing));
	const FVector2D Step(Size.X / NX, Size.Y / NY);

	for (int32 Y = 0; Y < NY; ++Y)
	{
		for (int32 X = 0; X < NX; ++X)
		{
			const FVector2D P = Bounds.Min + FVector2D((X + 0.5f) * Step.X, (Y + 0.5f) * Step.Y)
				+ FVector2D(Stream.FRandRange(-0.15f, 0.15f) * Step.X, Stream.FRandRange(-0.15f, 0.15f) * Step.Y);
			const bool bNearSpawn = FVector2D::Distance(P, Origin2D()) < SpawnClearRadius + Step.X * 0.5f;
			if (!bNearSpawn && Stream.FRand() >= LightChance)
			{
				continue;
			}

			const bool bAlongX = Stream.FRand() < 0.5f;
			AddBox(LightPanelInstances, ToWorld(P, WallHeight - 2.f), bAlongX ? FVector(120.f, 60.f, 4.f) : FVector(60.f, 120.f, 4.f));

			UPointLightComponent* Light = NewObject<UPointLightComponent>(this);
			Light->SetupAttachment(RootComponent);
			Light->SetMobility(EComponentMobility::Movable);
			Light->SetWorldLocation(ToWorld(P, WallHeight - 30.f));
			Light->SetIntensityUnits(ELightUnits::Candelas);
			Light->SetIntensity(LightIntensity);
			Light->SetAttenuationRadius(LightRadius);
			Light->SetLightColor(LightColor);
			Light->SetCastShadows(false);
			Light->RegisterComponent();
			SpawnedLights.Add(Light);
		}
	}
}

// ===================================================================== Grade (original)

void ANoclipLevelGenerator::GenerateGrid(FRandomStream& Stream)
{
	const int32 N = FMath::Max(5, GridSize | 1);
	const int32 Center = N / 2;
	const float HalfCell = CellSize * 0.5f;
	const FVector2D O = Origin2D();
	const float HalfTotal = N * CellSize * 0.5f;

	auto Cell = [&](int32 X, int32 Y) { return O + FVector2D((X - Center) * CellSize, (Y - Center) * CellSize); };
	auto HIdx = [N](int32 X, int32 Y) { return X + Y * (N - 1); };
	auto VIdx = [N](int32 X, int32 Y) { return X + Y * N; };

	TArray<bool> HOpen; HOpen.Init(false, (N - 1) * N);
	TArray<bool> VOpen; VOpen.Init(false, N * (N - 1));

	// Labirinto base por DFS (garante conectividade).
	{
		TArray<bool> Visited; Visited.Init(false, N * N);
		TArray<FIntPoint> StackCells;
		StackCells.Push(FIntPoint(Center, Center));
		Visited[Center + Center * N] = true;
		const FIntPoint Dirs[4] = { {1, 0}, {-1, 0}, {0, 1}, {0, -1} };
		while (StackCells.Num() > 0)
		{
			const FIntPoint Cur = StackCells.Last();
			int32 Order[4] = { 0, 1, 2, 3 };
			for (int32 i = 3; i > 0; --i) { Swap(Order[i], Order[Stream.RandRange(0, i)]); }
			bool bMoved = false;
			for (int32 k = 0; k < 4 && !bMoved; ++k)
			{
				const FIntPoint D = Dirs[Order[k]];
				const FIntPoint Next = Cur + D;
				if (Next.X < 0 || Next.Y < 0 || Next.X >= N || Next.Y >= N || Visited[Next.X + Next.Y * N]) { continue; }
				if (D.X != 0) { HOpen[HIdx(FMath::Min(Cur.X, Next.X), Cur.Y)] = true; }
				else { VOpen[VIdx(Cur.X, FMath::Min(Cur.Y, Next.Y))] = true; }
				Visited[Next.X + Next.Y * N] = true;
				StackCells.Push(Next);
				bMoved = true;
			}
			if (!bMoved) { StackCells.Pop(EAllowShrinking::No); }
		}
	}
	for (bool& bOpen : HOpen) { if (!bOpen && Stream.FRand() < ExtraOpeningChance) { bOpen = true; } }
	for (bool& bOpen : VOpen) { if (!bOpen && Stream.FRand() < ExtraOpeningChance) { bOpen = true; } }

	auto EdgeWall = [&](const FVector2D& A, const FVector2D& B)
	{
		if (Stream.FRand() < DoorwayChance) { AddDividerWithDoors(Stream, A, B, 1, WallThickness); }
		else { AddWallSegment(A, B, WallThickness); }
	};
	for (int32 Y = 0; Y < N; ++Y)
	{
		for (int32 X = 0; X < N - 1; ++X)
		{
			if (!HOpen[HIdx(X, Y)])
			{
				const FVector2D C = Cell(X, Y) + FVector2D(HalfCell, 0.f);
				EdgeWall(C - FVector2D(0.f, HalfCell), C + FVector2D(0.f, HalfCell));
			}
		}
	}
	for (int32 Y = 0; Y < N - 1; ++Y)
	{
		for (int32 X = 0; X < N; ++X)
		{
			if (!VOpen[VIdx(X, Y)])
			{
				const FVector2D C = Cell(X, Y) + FVector2D(0.f, HalfCell);
				EdgeWall(C - FVector2D(HalfCell, 0.f), C + FVector2D(HalfCell, 0.f));
			}
		}
	}
	for (int32 Y = 1; Y < N; ++Y)
	{
		for (int32 X = 1; X < N; ++X)
		{
			if (Stream.FRand() < PillarChance)
			{
				AddSolid(Cell(X, Y) - FVector2D(HalfCell, HalfCell), FVector2D(70.f, 70.f), WallHeight);
			}
		}
	}

	GeneratedBounds = FBox2D(O - FVector2D(HalfTotal, HalfTotal), O + FVector2D(HalfTotal, HalfTotal));
	{
		const FVector2D Corner = Cell(0, 0);
		const FVector2D Half(FMath::Min(ExtractionMaxSize.X, CellSize - 100.f) * 0.5f, FMath::Min(ExtractionMaxSize.Y, CellSize - 100.f) * 0.5f);
		ExtractionRect = FBox2D(Corner - Half, Corner + Half);
	}
	const FVector2D Mn = GeneratedBounds.Min, Mx = GeneratedBounds.Max;
	AddWallSegment(FVector2D(Mn.X, Mn.Y), FVector2D(Mx.X, Mn.Y), WallThickness);
	AddWallSegment(FVector2D(Mn.X, Mx.Y), FVector2D(Mx.X, Mx.Y), WallThickness);
	AddWallSegment(FVector2D(Mn.X, Mn.Y), FVector2D(Mn.X, Mx.Y), WallThickness);
	AddWallSegment(FVector2D(Mx.X, Mn.Y), FVector2D(Mx.X, Mx.Y), WallThickness);
}

// ===================================================================== Zonas (BSP)

void ANoclipLevelGenerator::GenerateZones(FRandomStream& Stream)
{
	const FVector2D O = Origin2D();
	const FVector2D Half = ZonesAreaSize * 0.5f;
	const FBox2D Area(O - Half, O + Half);
	GeneratedBounds = Area;

	TArray<FBox2D> Leaves;
	SplitZone(Stream, Area, 0, Leaves);

	// Sala Manila: a zona mais distante do spawn.
	int32 ExtractLeaf = INDEX_NONE;
	float BestDist = -1.f;
	for (int32 i = 0; i < Leaves.Num(); ++i)
	{
		const float Dist = FVector2D::Distance(Leaves[i].GetCenter(), O);
		if (Dist > BestDist)
		{
			BestDist = Dist;
			ExtractLeaf = i;
		}
	}

	for (int32 i = 0; i < Leaves.Num(); ++i)
	{
		const FBox2D& Leaf = Leaves[i];
		ENoclipZoneFill Fill = PickFill(Stream);
		// A zona onde os jogadores nascem fica aberta.
		if (Leaf.IsInside(O))
		{
			Fill = ENoclipZoneFill::Open;
		}
		if (i == ExtractLeaf)
		{
			const FVector2D RoomHalf(FMath::Min(ExtractionMaxSize.X * 0.5f, Leaf.GetExtent().X - ZoneMargin), FMath::Min(ExtractionMaxSize.Y * 0.5f, Leaf.GetExtent().Y - ZoneMargin));
			ExtractionRect = FBox2D(Leaf.GetCenter() - RoomHalf, Leaf.GetCenter() + RoomHalf);
			continue; // sem preenchimento
		}
		FillZone(Stream, Leaf, Fill);
	}

	// Borda externa grossa.
	const float T = WallThickness * 2.f;
	AddWallSegment(FVector2D(Area.Min.X, Area.Min.Y), FVector2D(Area.Max.X, Area.Min.Y), T);
	AddWallSegment(FVector2D(Area.Min.X, Area.Max.Y), FVector2D(Area.Max.X, Area.Max.Y), T);
	AddWallSegment(FVector2D(Area.Min.X, Area.Min.Y), FVector2D(Area.Min.X, Area.Max.Y), T);
	AddWallSegment(FVector2D(Area.Max.X, Area.Min.Y), FVector2D(Area.Max.X, Area.Max.Y), T);
}

void ANoclipLevelGenerator::SplitZone(FRandomStream& Stream, const FBox2D& Rect, int32 Depth, TArray<FBox2D>& OutLeaves)
{
	const FVector2D Size = Rect.GetSize();
	const bool bCanSplitX = Size.X >= MinZoneSize * 2.f;
	const bool bCanSplitY = Size.Y >= MinZoneSize * 2.f;
	// Zonas pequenas as vezes param antes, para variar o tamanho.
	const bool bStopEarly = Depth >= 2 && Stream.FRand() < 0.2f;
	if (Depth >= MaxSplitDepth || (!bCanSplitX && !bCanSplitY) || bStopEarly)
	{
		OutLeaves.Add(Rect);
		return;
	}

	bool bSplitAcrossX; // true = corta com uma linha vertical (divide o eixo X)
	if (bCanSplitX && bCanSplitY)
	{
		const float Ratio = Size.X / Size.Y;
		bSplitAcrossX = Ratio > 1.25f ? true : (Ratio < 0.8f ? false : Stream.FRand() < 0.5f);
	}
	else
	{
		bSplitAcrossX = bCanSplitX;
	}

	const float T = Stream.FRandRange(0.35f, 0.65f);
	// Paredes principais (profundidade baixa) sao mais grossas e tem mais portas.
	const float Thickness = Depth <= 1 ? WallThickness * 2.f : WallThickness;

	if (bSplitAcrossX)
	{
		const float Cut = NoclipGen::SnapTo(FMath::Lerp(Rect.Min.X, Rect.Max.X, T), 50.f);
		const int32 Doors = FMath::Clamp(FMath::RoundToInt(Size.Y / 1400.f), 1, 4);
		AddDividerWithDoors(Stream, FVector2D(Cut, Rect.Min.Y), FVector2D(Cut, Rect.Max.Y), Doors, Thickness);
		SplitZone(Stream, FBox2D(Rect.Min, FVector2D(Cut, Rect.Max.Y)), Depth + 1, OutLeaves);
		SplitZone(Stream, FBox2D(FVector2D(Cut, Rect.Min.Y), Rect.Max), Depth + 1, OutLeaves);
	}
	else
	{
		const float Cut = NoclipGen::SnapTo(FMath::Lerp(Rect.Min.Y, Rect.Max.Y, T), 50.f);
		const int32 Doors = FMath::Clamp(FMath::RoundToInt(Size.X / 1400.f), 1, 4);
		AddDividerWithDoors(Stream, FVector2D(Rect.Min.X, Cut), FVector2D(Rect.Max.X, Cut), Doors, Thickness);
		SplitZone(Stream, FBox2D(Rect.Min, FVector2D(Rect.Max.X, Cut)), Depth + 1, OutLeaves);
		SplitZone(Stream, FBox2D(FVector2D(Rect.Min.X, Cut), Rect.Max), Depth + 1, OutLeaves);
	}
}

ENoclipZoneFill ANoclipLevelGenerator::PickFill(FRandomStream& Stream) const
{
	// Ordem fixa (deterministica): o TMap nao garante ordem de iteracao.
	static const ENoclipZoneFill Order[] = { ENoclipZoneFill::Open, ENoclipZoneFill::Stripes, ENoclipZoneFill::Comb, ENoclipZoneFill::Pillars, ENoclipZoneFill::Blocks, ENoclipZoneFill::Dashes };
	float Total = 0.f;
	for (const ENoclipZoneFill F : Order)
	{
		const float* W = FillWeights.Find(F);
		Total += W ? FMath::Max(*W, 0.f) : 0.f;
	}
	float Roll = Stream.FRand() * FMath::Max(Total, KINDA_SMALL_NUMBER);
	for (const ENoclipZoneFill F : Order)
	{
		const float* W = FillWeights.Find(F);
		Roll -= W ? FMath::Max(*W, 0.f) : 0.f;
		if (Roll <= 0.f)
		{
			return F;
		}
	}
	return ENoclipZoneFill::Open;
}

void ANoclipLevelGenerator::FillZone(FRandomStream& Stream, const FBox2D& Rect, ENoclipZoneFill Fill)
{
	const FBox2D In(Rect.Min + FVector2D(ZoneMargin, ZoneMargin), Rect.Max - FVector2D(ZoneMargin, ZoneMargin));
	const FVector2D Size = In.GetSize();
	if (Size.X < 200.f || Size.Y < 200.f)
	{
		return;
	}

	// Eixo longo da zona: as listras correm ao longo dele (ou no outro, as vezes).
	const bool bLongX = Size.X >= Size.Y;
	const bool bAlongX = Stream.FRand() < 0.75f ? bLongX : !bLongX;
	const float LenAlong = bAlongX ? Size.X : Size.Y;
	const float LenAcross = bAlongX ? Size.Y : Size.X;
	auto P = [&](float Along, float Across)
	{
		return bAlongX ? FVector2D(In.Min.X + Along, In.Min.Y + Across) : FVector2D(In.Min.X + Across, In.Min.Y + Along);
	};

	switch (Fill)
	{
	case ENoclipZoneFill::Stripes:
	{
		const float Spacing = Stream.FRandRange(240.f, 380.f);
		for (float Across = Spacing * 0.5f; Across < LenAcross; Across += Spacing)
		{
			AddDashedLine(Stream, P(0.f, Across), P(LenAlong, Across), 300.f, 1100.f, 130.f, 240.f);
		}
		break;
	}
	case ENoclipZoneFill::Comb:
	{
		const float Spacing = Stream.FRandRange(300.f, 420.f);
		const float StubEvery = Stream.FRandRange(260.f, 420.f);
		const float StubLen = Stream.FRandRange(60.f, 110.f);
		for (float Across = Spacing * 0.5f; Across < LenAcross; Across += Spacing)
		{
			AddDashedLine(Stream, P(0.f, Across), P(LenAlong, Across), 500.f, 1400.f, 140.f, 220.f);
			for (float Along = StubEvery * 0.5f; Along < LenAlong; Along += StubEvery)
			{
				const float Side = Stream.FRand() < 0.5f ? -1.f : 1.f;
				AddWallSegment(P(Along, Across), P(Along, Across + Side * StubLen), WallThickness);
			}
		}
		break;
	}
	case ENoclipZoneFill::Pillars:
	{
		const float Spacing = Stream.FRandRange(300.f, 520.f);
		const float PillarSize = Stream.FRandRange(45.f, 80.f);
		for (float Along = Spacing * 0.5f; Along < LenAlong; Along += Spacing)
		{
			for (float Across = Spacing * 0.5f; Across < LenAcross; Across += Spacing)
			{
				AddSolid(P(Along, Across), FVector2D(PillarSize, PillarSize), WallHeight);
			}
		}
		break;
	}
	case ENoclipZoneFill::Blocks:
	{
		TArray<FBox2D> Placed;
		const int32 Tries = FMath::Clamp(FMath::RoundToInt(Size.X * Size.Y / 90000.f), 4, 60);
		for (int32 i = 0; i < Tries; ++i)
		{
			const FVector2D BlockSize(Stream.FRandRange(120.f, 420.f), Stream.FRandRange(120.f, 420.f));
			if (BlockSize.X > Size.X || BlockSize.Y > Size.Y)
			{
				continue;
			}
			const FVector2D C(Stream.FRandRange(In.Min.X + BlockSize.X * 0.5f, In.Max.X - BlockSize.X * 0.5f), Stream.FRandRange(In.Min.Y + BlockSize.Y * 0.5f, In.Max.Y - BlockSize.Y * 0.5f));
			const FBox2D Box(C - BlockSize * 0.5f - FVector2D(140.f, 140.f), C + BlockSize * 0.5f + FVector2D(140.f, 140.f));
			bool bOverlaps = false;
			for (const FBox2D& Other : Placed)
			{
				if (Box.Intersect(Other)) { bOverlaps = true; break; }
			}
			if (!bOverlaps)
			{
				Placed.Add(Box);
				AddSolid(C, BlockSize, WallHeight);
			}
		}
		break;
	}
	case ENoclipZoneFill::Dashes:
	{
		const float Spacing = Stream.FRandRange(220.f, 320.f);
		int32 Row = 0;
		for (float Across = Spacing * 0.5f; Across < LenAcross; Across += Spacing, ++Row)
		{
			const float DashLen = Stream.FRandRange(110.f, 200.f);
			const float Step = DashLen + Stream.FRandRange(150.f, 260.f);
			const float Offset = (Row % 2) * Step * 0.5f;
			for (float Along = Offset; Along + DashLen < LenAlong; Along += Step)
			{
				AddWallSegment(P(Along, Across), P(Along + DashLen, Across), WallThickness);
			}
		}
		break;
	}
	case ENoclipZoneFill::Open:
	default:
	{
		// Sala aberta, as vezes com alguns pilares soltos.
		const int32 Count = Stream.RandRange(0, 4);
		for (int32 i = 0; i < Count; ++i)
		{
			AddSolid(FVector2D(Stream.FRandRange(In.Min.X, In.Max.X), Stream.FRandRange(In.Min.Y, In.Max.Y)), FVector2D(60.f, 60.f), WallHeight);
		}
		break;
	}
	}
}

// ===================================================================== Corrente de salas

void ANoclipLevelGenerator::GenerateLinear(FRandomStream& Stream)
{
	// Tipos: 0 = salao com aglomerados de pilares, 1 = conector estreito, 2 = vault hall, 3 = salao vazio.
	TArray<int32> Types;
	TArray<float> Lengths;
	for (int32 i = 0; i < LinearRoomCount; ++i)
	{
		int32 Type;
		if (i > 0 && Types.Last() != 1 && Stream.FRand() < 0.35f)
		{
			Type = 1;
		}
		else
		{
			const float R = Stream.FRand();
			Type = R < 0.4f ? 0 : (R < 0.75f ? 2 : 3);
		}
		Types.Add(Type);
		switch (Type)
		{
		case 0:  Lengths.Add(NoclipGen::SnapTo(Stream.FRandRange(1800.f, 2600.f), 50.f)); break;
		case 1:  Lengths.Add(NoclipGen::SnapTo(Stream.FRandRange(450.f, 700.f), 50.f)); break;
		case 2:  Lengths.Add(NoclipGen::SnapTo(Stream.FRandRange(900.f, 1300.f), 50.f)); break;
		default: Lengths.Add(NoclipGen::SnapTo(Stream.FRandRange(1000.f, 1600.f), 50.f)); break;
		}
	}

	float Total = 0.f;
	for (const float L : Lengths) { Total += L; }

	const FVector2D O = Origin2D();
	const float HalfW = LinearWidth * 0.5f;
	// O spawn fica no meio da primeira sala grande, para o grupo explorar nos dois sentidos.
	float StartX = O.X - Total * 0.5f;
	GeneratedBounds = FBox2D(FVector2D(StartX, O.Y - HalfW), FVector2D(StartX + Total, O.Y + HalfW));

	float X = StartX;
	for (int32 i = 0; i < Types.Num(); ++i)
	{
		const FBox2D Room(FVector2D(X, O.Y - HalfW), FVector2D(X + Lengths[i], O.Y + HalfW));
		if (i == Types.Num() - 1)
		{
			// Ultima sala da faixa: Sala Manila.
			const FVector2D Half(FMath::Min(ExtractionMaxSize.X * 0.5f, Room.GetExtent().X - ZoneMargin), FMath::Min(ExtractionMaxSize.Y * 0.5f, Room.GetExtent().Y - ZoneMargin));
			ExtractionRect = FBox2D(Room.GetCenter() - Half, Room.GetCenter() + Half);
		}
		else
		{
			FillLinearRoom(Stream, Room, Types[i]);
		}

		// Parede de separacao com portas (exceto depois da ultima sala).
		if (i < Types.Num() - 1)
		{
			const float WallX = X + Lengths[i];
			const bool bNextIsConnector = Types[i + 1] == 1 || Types[i] == 1;
			const int32 Doors = bNextIsConnector ? 1 : Stream.RandRange(1, 2);
			AddDividerWithDoors(Stream, FVector2D(WallX, O.Y - HalfW), FVector2D(WallX, O.Y + HalfW), Doors, WallThickness * 1.5f);
		}
		X += Lengths[i];
	}

	const float T = WallThickness * 2.f;
	const FBox2D& B = GeneratedBounds;
	AddWallSegment(FVector2D(B.Min.X, B.Min.Y), FVector2D(B.Max.X, B.Min.Y), T);
	AddWallSegment(FVector2D(B.Min.X, B.Max.Y), FVector2D(B.Max.X, B.Max.Y), T);
	AddWallSegment(FVector2D(B.Min.X, B.Min.Y), FVector2D(B.Min.X, B.Max.Y), T);
	AddWallSegment(FVector2D(B.Max.X, B.Min.Y), FVector2D(B.Max.X, B.Max.Y), T);
}

void ANoclipLevelGenerator::FillLinearRoom(FRandomStream& Stream, const FBox2D& Rect, int32 RoomType)
{
	const FBox2D In(Rect.Min + FVector2D(ZoneMargin, ZoneMargin), Rect.Max - FVector2D(ZoneMargin, ZoneMargin));
	const FVector2D Size = In.GetSize();
	if (Size.X < 100.f || Size.Y < 100.f)
	{
		return;
	}

	switch (RoomType)
	{
	case 0: // aglomerados de pilares finos
	{
		const int32 Clusters = Stream.RandRange(3, 5);
		for (int32 c = 0; c < Clusters; ++c)
		{
			const FVector2D Center(Stream.FRandRange(In.Min.X + 200.f, In.Max.X - 200.f), Stream.FRandRange(In.Min.Y + 200.f, In.Max.Y - 200.f));
			const int32 Count = Stream.RandRange(6, 14);
			const float Spread = Stream.FRandRange(120.f, 260.f);
			for (int32 i = 0; i < Count; ++i)
			{
				const FVector2D Off(Stream.FRandRange(-1.f, 1.f) * Spread, Stream.FRandRange(-1.f, 1.f) * Spread);
				AddSolid(Center + Off, FVector2D(32.f, 32.f), WallHeight);
			}
		}
		break;
	}
	case 1: // conector: vazio
		break;
	case 2: // vault hall: grade regular
	{
		const int32 Cols = FMath::Max(2, FMath::RoundToInt(Size.X / 280.f));
		const int32 Rows = FMath::Max(2, FMath::RoundToInt(Size.Y / 330.f));
		for (int32 r = 0; r < Rows; ++r)
		{
			for (int32 c = 0; c < Cols; ++c)
			{
				const FVector2D Pos = In.Min + FVector2D((c + 0.5f) * Size.X / Cols, (r + 0.5f) * Size.Y / Rows);
				AddSolid(Pos, FVector2D(55.f, 55.f), WallHeight);
			}
		}
		break;
	}
	default: // salao vazio com poucos pilares
	{
		const int32 Count = Stream.RandRange(0, 3);
		for (int32 i = 0; i < Count; ++i)
		{
			AddSolid(FVector2D(Stream.FRandRange(In.Min.X, In.Max.X), Stream.FRandRange(In.Min.Y, In.Max.Y)), FVector2D(70.f, 70.f), WallHeight);
		}
		break;
	}
	}
}

// ===================================================================== Loot

void ANoclipLevelGenerator::SpawnOfficeLoot(FRandomStream& Stream)
{
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
	Params.Owner = this;
	Params.bDeferConstruction = true;

	const FBox2D Inner(GeneratedBounds.Min + FVector2D(150.f, 150.f), GeneratedBounds.Max - FVector2D(150.f, 150.f));
	int32 Spawned = 0;
	for (int32 i = 0; i < LootCount; ++i)
	{
		const int32 Index = NoclipLoot::PickWeighted(Stream, FloorOffset < 0.f ? 1 : 0);
		const FNoclipLootDef* Def = NoclipLoot::Get(Index);
		UStaticMesh* LootMesh = NoclipLoot::LoadMesh(Index);
		if (!Def || !LootMesh)
		{
			continue;
		}
		// Caixa do modelo (pivo no chao ou no centro, depende do FBX): apoia o fundo a 3 cm do piso.
		const FBoxSphereBounds MeshBounds = LootMesh->GetBounds();
		const FVector Extent = MeshBounds.BoxExtent * Def->Scale;
		const FVector Center = MeshBounds.Origin * Def->Scale;
		const float BottomZ = Center.Z - Extent.Z;
		const FCollisionShape Probe = FCollisionShape::MakeBox(Extent + FVector(4.f, 4.f, 0.f)); // sem encostar no piso

		FTransform Xf;
		bool bFound = false;
		for (int32 Attempt = 0; Attempt < 16 && !bFound; ++Attempt)
		{
			const FVector2D P(Stream.FRandRange(Inner.Min.X, Inner.Max.X), Stream.FRandRange(Inner.Min.Y, Inner.Max.Y));
			if (FVector2D::Distance(P, Origin2D()) < SpawnClearRadius * 0.6f || (ExtractionRect.bIsValid && ExtractionRect.ExpandBy(150.f).IsInside(P)))
			{
				continue;
			}
			const FQuat Rot(FRotator(0.f, Stream.FRandRange(0.f, 360.f), 0.f));
			const FVector Location = ToWorld(P, 3.f - BottomZ);
			bFound = !GetWorld()->OverlapBlockingTestByChannel(Location + Rot.RotateVector(Center) + FVector(0.f, 0.f, 5.f), Rot, ECC_WorldStatic, Probe);
			Xf = FTransform(Rot, Location);
		}
		if (!bFound)
		{
			continue;
		}
		ANoclipLootActor* Loot = GetWorld()->SpawnActor<ANoclipLootActor>(ANoclipLootActor::StaticClass(), Xf, Params);
		if (Loot)
		{
			Loot->SetupFromCatalog(Index, Stream);
			Loot->FinishSpawning(Xf);
			SpawnedLoot.Add(Loot);
			++Spawned;
		}
	}
	UE_LOG(LogNoclip, Log, TEXT("Achados do escritorio: %d de %d (catalogo com %d modelos)."), Spawned, LootCount, NoclipLoot::Num());
}

void ANoclipLevelGenerator::SpawnLoot(FRandomStream& Stream)
{
	if (bUseOfficeLoot && NoclipLoot::Num() > 0 && LootCount > 0 && GeneratedBounds.bIsValid)
	{
		SpawnOfficeLoot(Stream);
		return;
	}
	if (LootClasses.Num() == 0 || LootCount <= 0 || !GeneratedBounds.bIsValid)
	{
		return;
	}

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
	Params.Owner = this;

	const FBox2D Inner(GeneratedBounds.Min + FVector2D(150.f, 150.f), GeneratedBounds.Max - FVector2D(150.f, 150.f));
	const FCollisionShape Probe = FCollisionShape::MakeSphere(70.f);

	for (int32 i = 0; i < LootCount; ++i)
	{
		const TSubclassOf<ANoclipLootActor> LootClass = LootClasses[Stream.RandRange(0, LootClasses.Num() - 1)];
		if (!LootClass)
		{
			continue;
		}

		FVector Location = FVector::ZeroVector;
		bool bFound = false;
		for (int32 Attempt = 0; Attempt < 12 && !bFound; ++Attempt)
		{
			const FVector2D P(Stream.FRandRange(Inner.Min.X, Inner.Max.X), Stream.FRandRange(Inner.Min.Y, Inner.Max.Y));
			if (FVector2D::Distance(P, Origin2D()) < SpawnClearRadius * 0.6f || (ExtractionRect.bIsValid && ExtractionRect.ExpandBy(150.f).IsInside(P)))
			{
				continue;
			}
			Location = ToWorld(P, 80.f);
			bFound = !GetWorld()->OverlapBlockingTestByChannel(Location, FQuat::Identity, ECC_WorldStatic, Probe);
		}
		if (!bFound)
		{
			continue;
		}

		const FRotator Rotation(0.f, Stream.FRandRange(0.f, 360.f), 0.f);
		if (ANoclipLootActor* Loot = GetWorld()->SpawnActor<ANoclipLootActor>(LootClass, Location, Rotation, Params))
		{
			SpawnedLoot.Add(Loot);
		}
	}
}
