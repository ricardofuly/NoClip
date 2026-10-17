// Copyright Fuly Games. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "NoclipLevelGenerator.generated.h"

class UHierarchicalInstancedStaticMeshComponent;
class UStaticMesh;
class UMaterialInterface;
class ANoclipLootActor;
class ANoclipExtractionZone;
class ANoclipPowerGenerator;
class UTextRenderComponent;

/** Pontos onde a realidade "falha" (noclip). */
enum class ENoclipGlitchKind : uint8
{
	/** Parede sem colisao no Level 0: atravessa e cai no subsolo. */
	FakeWall,
	/** Piso falso no Level 0: afunda e cai no subsolo logo abaixo. */
	FakeFloor,
	/** Saida de servico do subsolo: volta para perto da Sala Manila. */
	ServiceExit,
	/** Parede sem colisao no subsolo: volta para um lugar qualquer do Level 0. */
	UpWall
};

struct FNoclipGlitch
{
	FBox Box = FBox(ForceInit);
	ENoclipGlitchKind Kind = ENoclipGlitchKind::FakeWall;
};

/** Divisoria que muda: um tampao que alterna entre duas portas da mesma parede (sempre sobra caminho). */
struct FNoclipShifter
{
	FVector CenterA = FVector::ZeroVector;
	FVector CenterB = FVector::ZeroVector;
	FVector Size = FVector::ZeroVector;
};

UENUM(BlueprintType)
enum class ENoclipLayoutMode : uint8
{
	/** A seed escolhe entre Zonas e Corrente. */
	Random,
	/** Grade de salas com labirinto aberto (layout original). */
	Grid,
	/** Particao BSP em zonas, cada uma com um preenchimento (listras, pilares, blocos...). */
	Zones,
	/** Faixa de salas em sequencia (saloes, conectores, vault halls). */
	Linear
};

UENUM(BlueprintType)
enum class ENoclipZoneFill : uint8
{
	Open,
	Stripes,
	Comb,
	Pillars,
	Blocks,
	Dashes
};

/**
 * Gera o Level 0 a partir da seed replicada no GameState.
 * Cada maquina gera o mesmo mapa localmente; so o servidor spawna o loot (replicado).
 * Coloque UM gerador no mapa, centrado na origem (onde ficam os PlayerStarts).
 */
UCLASS(Blueprintable)
class NOCLIP_API ANoclipLevelGenerator : public AActor
{
	GENERATED_BODY()
	friend struct FNoclipTestAccess; // testes de automacao (Tests/)

public:
	ANoclipLevelGenerator();

	virtual void BeginPlay() override;

	/** Gera (ou regera) o nivel. Deterministico para a mesma seed. */
	UFUNCTION(BlueprintCallable, Category = "Noclip|Generation")
	void Generate(int32 Seed);

	UFUNCTION(BlueprintCallable, Category = "Noclip|Generation")
	void ClearGenerated();

	UFUNCTION(BlueprintPure, Category = "Noclip|Generation")
	int32 GetGeneratedSeed() const { return GeneratedSeed; }

	UFUNCTION(BlueprintPure, Category = "Noclip|Generation")
	ENoclipLayoutMode GetGeneratedMode() const { return GeneratedMode; }

	/** Servidor: spawna os achados e a Sala Manila do turno (chamado pelo GameMode no inicio do turno). */
	void SpawnShiftContent();

	/** Servidor: remove achados e Sala Manila (fim do turno). */
	void ClearShiftContent();

	UFUNCTION(BlueprintPure, Category = "Noclip|Generation")
	ANoclipExtractionZone* GetExtractionZone() const { return ExtractionZone; }

	/** Area do nivel gerado (plano XY) e da Sala Manila — usadas pela grade de navegacao das entidades. */
	FBox2D GetGeneratedBounds() const { return GeneratedBounds; }
	FBox2D GetExtractionRect() const { return ExtractionRect; }

	/**
	 * Colapso (GDD): o nivel "desliga" de fora para dentro. Cada maquina chama localmente com o progresso
	 * do colapso (0..1); as luzes alem do raio apagam (piscando na borda) ate sobrar so a Sala Manila.
	 * bCollapse = false acende tudo de novo.
	 */
	void UpdateCollapseLights(bool bCollapse, float Progress, float TimeSeconds, bool bPowerOut = false, float PowerChangeTime = -100.f);

	// ---------- Andares e noclip ----------
	/** Z do piso do andar (0 = Level 0, 1 = subsolo). */
	float GetFloorZ(int32 Floor) const { return GetActorLocation().Z - (Floor == 1 ? SubFloorDepth : 0.f); }
	/** Em que andar esta este ponto. */
	int32 GetFloorAt(const FVector& Location) const { return Location.Z < GetActorLocation().Z - SubFloorDepth * 0.5f ? 1 : 0; }
	bool HasSubFloor() const { return bHasSubFloor; }
	const TArray<FNoclipGlitch>& GetGlitches() const { return Glitches; }
	/** Ponto de falha que contem este lugar (INDEX_NONE se nenhum). */
	int32 FindGlitchAt(const FVector& Location) const;
	/** Servidor: para onde leva a falha (lugar livre no outro andar). */
	bool ResolveGlitchDestination(int32 GlitchIndex, const FVector& From, FVector& OutLocation) const;
	/** Lugar livre para um estagiario em pe, perto de Near (no andar Floor). */
	bool FindWalkable(int32 Floor, const FVector2D& Near, float SearchRadius, FVector& OutLocation) const;
	/** Saidas de servico do subsolo (para o celular apontar). */
	const TArray<FVector>& GetServiceExits() const { return ServiceExits; }

	// ---------- Salas que mudam ----------
	int32 GetNumShifters() const { return Shifters.Num(); }
	const FNoclipShifter& GetShifter(int32 Index) const { return Shifters[Index]; }
	uint32 GetAppliedShiftMask() const { return AppliedShiftMask; }
	/** Aplica o estado das divisorias (bit 1 = tampao na porta B). bAnnounce: toca o barulho das que mudaram. */
	void ApplyShiftMask(uint32 Mask, bool bAnnounce);
	/** Muda quando o nivel e regerado ou uma divisoria muda (a grade das entidades se refaz). */
	int32 GetLayoutVersion() const { return GeneratedSeed * 131 + static_cast<int32>(AppliedShiftMask & 0x7FFFFF); }

	/** noclip.DebugGlitches 1: desenha falhas (vermelho), saidas (verde) e divisorias que mudam (amarelo). */
	void DebugDrawGlitches() const;

	/** Gerador de energia do turno (apagao). */
	ANoclipPowerGenerator* GetPowerGenerator() const { return PowerGenerator; }

	// ---------- Geral ----------
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Noclip|Layout")
	ENoclipLayoutMode LayoutMode = ENoclipLayoutMode::Random;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Noclip|Layout")
	float WallHeight = 290.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Noclip|Layout")
	float WallThickness = 20.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Noclip|Layout")
	float DoorwayWidth = 180.f;

	/** Raio em volta do ator (spawn) que fica livre de paredes e pilares. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Noclip|Layout")
	float SpawnClearRadius = 550.f;

	// ---------- Noclip / subsolo ----------
	/** Gera o subsolo (andar de baixo) com paredes e pisos falsos levando ate ele. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Noclip|Noclip")
	bool bGenerateSubFloor = true;

	/** Distancia entre o piso do Level 0 e o do subsolo (cm). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Noclip|Noclip")
	float SubFloorDepth = 800.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Noclip|Noclip", meta = (ClampMin = "0", ClampMax = "12"))
	int32 FakeWallCount = 4;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Noclip|Noclip", meta = (ClampMin = "0", ClampMax = "12"))
	int32 FakeFloorCount = 3;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Noclip|Noclip", meta = (ClampMin = "1", ClampMax = "6"))
	int32 ServiceExitCount = 2;

	/** Paredes falsas no subsolo que devolvem para um lugar qualquer do Level 0. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Noclip|Noclip", meta = (ClampMin = "0", ClampMax = "8"))
	int32 UpWallCount = 2;

	/** Achados extras no subsolo (o risco paga). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Noclip|Noclip", meta = (ClampMin = "0"))
	int32 SubFloorLootCount = 8;

	// ---------- Salas que mudam ----------
	/** Chance de uma divisoria ganhar uma porta a mais e um tampao que troca de porta durante o turno. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Noclip|Shifting", meta = (ClampMin = "0", ClampMax = "1"))
	float ShiftDividerChance = 0.45f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Noclip|Shifting", meta = (ClampMin = "0", ClampMax = "32"))
	int32 MaxShifters = 16;

	// ---------- Grade ----------
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Noclip|Grid", meta = (ClampMin = "5", ClampMax = "61"))
	int32 GridSize = 13;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Noclip|Grid")
	float CellSize = 800.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Noclip|Grid", meta = (ClampMin = "0", ClampMax = "1"))
	float ExtraOpeningChance = 0.45f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Noclip|Grid", meta = (ClampMin = "0", ClampMax = "1"))
	float DoorwayChance = 0.35f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Noclip|Grid", meta = (ClampMin = "0", ClampMax = "1"))
	float PillarChance = 0.3f;

	// ---------- Zonas ----------
	/** Tamanho total da area particionada (cm). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Noclip|Zones")
	FVector2D ZonesAreaSize = FVector2D(9600.f, 4800.f);

	/** Lado minimo de uma zona antes de parar de dividir (cm). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Noclip|Zones")
	float MinZoneSize = 1100.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Noclip|Zones", meta = (ClampMin = "1", ClampMax = "8"))
	int32 MaxSplitDepth = 4;

	/** Espaco livre entre a parede da zona e o preenchimento (cm). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Noclip|Zones")
	float ZoneMargin = 160.f;

	/** Peso de cada preenchimento no sorteio (Open, Stripes, Comb, Pillars, Blocks, Dashes). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Noclip|Zones")
	TMap<ENoclipZoneFill, float> FillWeights;

	// ---------- Corrente ----------
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Noclip|Linear", meta = (ClampMin = "3", ClampMax = "40"))
	int32 LinearRoomCount = 11;

	/** Largura da faixa de salas (cm). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Noclip|Linear")
	float LinearWidth = 1500.f;

	// ---------- Luz ----------
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Noclip|Visual")
	float LightSpacing = 650.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Noclip|Visual", meta = (ClampMin = "0", ClampMax = "1"))
	float LightChance = 0.55f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Noclip|Visual")
	FLinearColor LightColor = FLinearColor(1.f, 0.93f, 0.72f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Noclip|Visual")
	float LightIntensity = 10.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Noclip|Visual")
	float LightRadius = 950.f;

	// ---------- Materiais ----------
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Noclip|Visual")
	TObjectPtr<UStaticMesh> CubeMesh;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Noclip|Visual")
	TObjectPtr<UMaterialInterface> WallMaterial;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Noclip|Visual")
	TObjectPtr<UMaterialInterface> FloorMaterial;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Noclip|Visual")
	TObjectPtr<UMaterialInterface> CeilingMaterial;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Noclip|Visual")
	TObjectPtr<UMaterialInterface> LightPanelMaterial;

	// ---------- Loot (so servidor) ----------
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Noclip|Loot")
	TArray<TSubclassOf<ANoclipLootActor>> LootClasses;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Noclip|Loot", meta = (ClampMin = "0"))
	int32 LootCount = 20;

	/** Sorteia os achados do catalogo do escritorio (Assets/Office) em vez das LootClasses. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Noclip|Loot")
	bool bUseOfficeLoot = true;

	// ---------- Sala Manila ----------
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Noclip|Extraction")
	TSubclassOf<ANoclipExtractionZone> ExtractionZoneClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Noclip|Extraction")
	TObjectPtr<UMaterialInterface> ExtractionMarkerMaterial;

	/** Tamanho maximo da Sala Manila (cm); fica menor se a zona escolhida for menor. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Noclip|Extraction")
	FVector2D ExtractionMaxSize = FVector2D(700.f, 700.f);

protected:
	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UHierarchicalInstancedStaticMeshComponent> FloorInstances;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UHierarchicalInstancedStaticMeshComponent> CeilingInstances;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UHierarchicalInstancedStaticMeshComponent> WallInstances;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UHierarchicalInstancedStaticMeshComponent> LightPanelInstances;

	/** Paredes falsas: iguais as outras, sem colisao. */
	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UHierarchicalInstancedStaticMeshComponent> FakeWallInstances;

	/** Remendos de carpete dos pisos falsos (sem colisao, girados: a emenda nao bate). */
	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UHierarchicalInstancedStaticMeshComponent> GlitchFloorInstances;

	/** Tampoes das divisorias que mudam (com colisao). */
	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UHierarchicalInstancedStaticMeshComponent> ShiftInstances;

	/** Batentes das saidas de servico do subsolo. */
	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UHierarchicalInstancedStaticMeshComponent> ExitInstances;

	UFUNCTION()
	void HandleSeedReady(int32 Seed);

	UFUNCTION(BlueprintImplementableEvent, Category = "Noclip|Generation")
	void OnGenerated(int32 Seed);

private:
	// Layouts
	void GenerateGrid(FRandomStream& Stream);
	void GenerateZones(FRandomStream& Stream);
	void GenerateLinear(FRandomStream& Stream);

	// Zonas
	void SplitZone(FRandomStream& Stream, const FBox2D& Rect, int32 Depth, TArray<FBox2D>& OutLeaves);
	ENoclipZoneFill PickFill(FRandomStream& Stream) const;
	void FillZone(FRandomStream& Stream, const FBox2D& Rect, ENoclipZoneFill Fill);
	void AddDashedLine(FRandomStream& Stream, const FVector2D& A, const FVector2D& B, float MinSeg, float MaxSeg, float MinGap, float MaxGap);
	void AddDividerWithDoors(FRandomStream& Stream, const FVector2D& A, const FVector2D& B, int32 DoorCount, float Thickness);

	// Corrente
	void FillLinearRoom(FRandomStream& Stream, const FBox2D& Rect, int32 RoomType);

	// Comum
	void AddFloorAndCeiling(const FBox2D& Bounds);
	void AddLights(FRandomStream& Stream, const FBox2D& Bounds);
	void AddBox(UHierarchicalInstancedStaticMeshComponent* Target, const FVector& Center, const FVector& SizeCm);
	/** Bloco solido no chao (pilar, caixa). Ignorado se invadir a area de spawn. */
	void AddSolid(const FVector2D& Center, const FVector2D& Size, float Height);
	/** Parede alinhada a um eixo; recortada se atravessar a area de spawn. */
	void AddWallSegment(const FVector2D& From, const FVector2D& To, float Thickness);
	void BeginPlayRetry();
	void SpawnLoot(FRandomStream& Stream);
	void SpawnOfficeLoot(FRandomStream& Stream);

	FVector2D Origin2D() const { return FVector2D(GetActorLocation()); }
	FVector ToWorld(const FVector2D& P, float Z) const { return FVector(P.X, P.Y, GetActorLocation().Z + FloorOffset + Z); }

	// Andares, falhas e divisorias (NoclipLevelGeneratorFloors.cpp)
	struct FPendingWall
	{
		FVector Center;
		FVector Size;
	};
	void FlushWalls(int32 Floor, FRandomStream& GlitchStream);
	void AddFakeFloors(FRandomStream& GlitchStream);
	void GenerateSubFloor(int32 Seed);
	void AddServiceExit(const FVector2D& Center, float Yaw);
	bool IsAreaClear(const FBox2D& Area) const;
	void SpawnPowerGenerator(FRandomStream& Stream);
	void AddShifterPlug(const FVector2D& PlugA, const FVector2D& PlugB, const FVector2D& Dir, float DoorWidth, float Thickness);

	/** Altura do andar sendo gerado (0 ou -SubFloorDepth). */
	float FloorOffset = 0.f;
	bool bAllowShifting = false;
	bool bHasSubFloor = false;
	FRandomStream ShiftStream;
	TArray<FPendingWall> PendingWalls;
	/** Obstaculos do andar sendo gerado (paredes e solidos, no plano). */
	TArray<FBox2D> FloorObstacles;
	TArray<FNoclipGlitch> Glitches;
	TArray<FNoclipShifter> Shifters;
	TArray<FVector> ServiceExits;
	uint32 AppliedShiftMask = 0;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UActorComponent>> SpawnedExtras;

	UPROPERTY(Transient)
	TObjectPtr<ANoclipPowerGenerator> PowerGenerator;

	int32 GeneratedSeed = INDEX_NONE;
	ENoclipLayoutMode GeneratedMode = ENoclipLayoutMode::Grid;
	FBox2D GeneratedBounds = FBox2D(ForceInit);
	/** Area reservada para a Sala Manila (sem preenchimento, sem loot). */
	FBox2D ExtractionRect = FBox2D(ForceInit);

	UPROPERTY(Transient)
	TObjectPtr<ANoclipExtractionZone> ExtractionZone;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UActorComponent>> SpawnedLights;

	UPROPERTY(Transient)
	TArray<TObjectPtr<AActor>> SpawnedLoot;

	// Estado das luzes no colapso (local, nao replicado).
	TArray<FTransform> PanelOriginals;
	TArray<bool> LightOff;
	int32 CollapseCacheSeed = INDEX_NONE;
	bool bAnyLightOff = false;
};
