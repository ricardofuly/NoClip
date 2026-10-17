// Copyright Fuly Games. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class UWorld;

/**
 * Grade de navegacao das entidades (so no servidor), montada uma vez por turno a partir do nivel gerado.
 * Cada celula guarda a folga ate a parede mais proxima, entao o mesmo mapa serve para o cao (largo) e para o
 * shambler (estreito). A* em 8 direcoes + "puxar o fio" para o caminho sair em linhas retas.
 * Nao usa NavMesh: o nivel nasce em tempo de jogo e muda a cada turno.
 */
class NOCLIP_API FNoclipNavGrid
{
public:
	/**
	 * Testa cada celula contra a geometria estatica (paredes, pilares, elevador) entre 40 e 170 cm do chao.
	 * Forbidden: retangulos onde as entidades nunca entram (Sala Manila, elevador).
	 */
	void Build(UWorld* World, const FBox2D& Bounds, float FloorZ, float InCellSize, const TArray<FBox2D>& Forbidden);
	void Reset();

	bool IsValid() const { return Width > 0 && Height > 0; }
	float GetFloorZ() const { return FloorZ; }
	float GetCellSize() const { return CellSize; }
	int32 NumWalkable(float Clearance) const;

	bool IsWalkable(const FVector& Location, float Clearance) const;
	/** Celula andavel mais proxima (procura num raio de ~5 m). */
	bool ProjectToWalkable(const FVector& Location, float Clearance, FVector& OutLocation) const;
	/** Linha reta andavel entre dois pontos. */
	bool HasStraightPath(const FVector& From, const FVector& To, float Clearance) const;

	/** Caminho em pontos do mundo (sem o ponto de partida). Falso se nao houver caminho. */
	bool FindPath(const FVector& From, const FVector& To, float Clearance, TArray<FVector>& OutPoints) const;

	/** Ponto andavel aleatorio a [MinDistance, MaxDistance] de Around (ou em qualquer lugar se MaxDistance <= 0). */
	bool RandomWalkable(FRandomStream& Stream, float Clearance, const FVector& Around, float MinDistance, float MaxDistance, FVector& OutLocation) const;

	/** Desenha as celulas bloqueadas perto de um ponto (depuracao). */
	void DebugDraw(UWorld* World, const FVector& Around, float Radius, float Clearance) const;

private:
	FIntPoint ToCell(const FVector2D& P) const;
	FVector CellCenter(int32 X, int32 Y) const;
	int32 Index(int32 X, int32 Y) const { return Y * Width + X; }
	bool InGrid(int32 X, int32 Y) const { return X >= 0 && Y >= 0 && X < Width && Y < Height; }
	uint8 RequiredClearance(float Clearance) const;
	bool CellOk(int32 X, int32 Y, uint8 Need) const { return InGrid(X, Y) && Clear[Index(X, Y)] >= Need; }
	bool FindNearestCell(const FIntPoint& Start, uint8 Need, int32 MaxRadius, FIntPoint& Out) const;
	bool LineOk(const FIntPoint& A, const FIntPoint& B, uint8 Need) const;

	int32 Width = 0;
	int32 Height = 0;
	float CellSize = 50.f;
	float FloorZ = 0.f;
	FVector2D Origin = FVector2D::ZeroVector;
	/** Distancia ate a celula bloqueada mais proxima, em celulas (0 = bloqueada). */
	TArray<uint8> Clear;

	// Buffers do A* (reaproveitados; so o game thread usa).
	mutable TArray<int32> GScore;
	mutable TArray<int32> Parent;
	mutable TArray<uint32> Visited;
	mutable uint32 SearchStamp = 0;
};
