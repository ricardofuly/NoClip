// Copyright Fuly Games. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Enemy/NoclipEnemyTypes.h"
#include "Enemy/NoclipNavGrid.h"
#include "NoclipEnemyDirector.generated.h"

class ANoclipEnemy;
class ANoclipLevelGenerator;
class ANoclipCharacter;

/**
 * Diretor das entidades (so no servidor; criado pelo GameMode). A cada turno monta a grade de navegacao do
 * nivel novo, solta as entidades aos poucos (curva de tensao do GDD: comeco calmo, pressao no meio, colapso
 * no fim), escuta os passos dos estagiarios e limpa tudo no fim do turno.
 * Sala Manila e elevador sao zonas seguras: as entidades nao entram e nao atacam quem esta la dentro.
 */
UCLASS(NotBlueprintable)
class NOCLIP_API ANoclipEnemyDirector : public AActor
{
	GENERATED_BODY()

public:
	ANoclipEnemyDirector();

	virtual void Tick(float DeltaSeconds) override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** Comeco do expediente (portas do elevador abrindo). */
	void StartShift(int32 ShiftIndex);
	/** O nivel comeca a desligar: entidades no frenesi e mais um cao. */
	void SetCollapse(bool bCollapse);
	/** Fim do turno: some com todas as entidades. */
	void StopShift();

	/** Solta uma entidade (aleatoria se Near == nullptr; senao perto desse ponto). */
	ANoclipEnemy* SpawnEnemy(ENoclipEnemyKind Kind, const FVector* Near = nullptr);
	void DespawnAll();

	const FNoclipNavGrid& GetNavGrid() const { return Grid; }
	bool IsSafeLocation(const FVector& Location) const;
	const TArray<TObjectPtr<ANoclipEnemy>>& GetEnemies() const { return Enemies; }

	/** Monta a grade a partir do gerador do mapa (se ainda nao existe ou se o nivel mudou). */
	bool EnsureGrid();

	// ---------- Ajustes ----------
	/** Primeira entidade depois de tanto tempo de expediente (s); as outras vem a cada SpawnInterval. */
	UPROPERTY(EditAnywhere, Category = "Noclip|Enemies")
	float FirstSpawnDelay = 40.f;

	UPROPERTY(EditAnywhere, Category = "Noclip|Enemies")
	float SpawnInterval = 45.f;

	/** Nasce longe de todo mundo (cm) e fora da vista. */
	UPROPERTY(EditAnywhere, Category = "Noclip|Enemies")
	float MinSpawnDistance = 2500.f;

	UPROPERTY(EditAnywhere, Category = "Noclip|Enemies")
	float NavCellSize = 50.f;

private:
	void BuildGrid(ANoclipLevelGenerator* Generator);
	bool FindSpawnLocation(float Clearance, FVector& OutLocation);
	void ReportPlayerNoises();

	struct FPendingSpawn
	{
		ENoclipEnemyKind Kind;
		double At;
	};

	FNoclipNavGrid Grid;
	TArray<FBox2D> SafeRects;
	int32 GridSeed = INDEX_NONE;
	int32 GridLayoutVersion = INDEX_NONE;

	UPROPERTY(Transient)
	TArray<TObjectPtr<ANoclipEnemy>> Enemies;

	TArray<FPendingSpawn> Pending;
	bool bActive = false;
	bool bCollapse = false;
	int32 Shift = 1;
	double NextNoiseTime = 0.0;
	FRandomStream Stream;
};
