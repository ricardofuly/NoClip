// Copyright Fuly Games. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class UStaticMesh;
struct FRandomStream;

/**
 * Um achado do escritorio (Assets/Office). Cada modelo e uma entrada propria (as variantes de cor
 * repetem nome e texto e dividem o peso de sorteio).
 */
struct NOCLIP_API FNoclipLootDef
{
	FName Id;
	FText Name;
	/** Uma linha de humor da Baseline (aparece no cartao de quem carrega). */
	FText Description;
	FString MeshPath;
	int32 MinValue = 10;
	int32 MaxValue = 20;
	float MassKg = 5.f;
	/** Fracao do valor perdida a cada 1000 cm/s de impacto (vidro alto, papel zero). */
	float Fragility = 0.25f;
	float Scale = 1.f;
	/** Chance relativa de aparecer. */
	float SpawnWeight = 1.f;
	/** Quantos estagiarios para levantar (menos que isso, so arrasta). O peso se divide entre quem carrega. */
	int32 Carriers = 1;
	/** Onde aparece: bit 0 = Level 0, bit 1 = subsolo. */
	uint8 FloorMask = 1;
};

namespace NoclipLoot
{
	NOCLIP_API int32 Num();
	NOCLIP_API const FNoclipLootDef* Get(int32 Index);
	NOCLIP_API UStaticMesh* LoadMesh(int32 Index);
	/** Sorteio pelo peso de cada entrada, so entre as que aparecem neste andar (0 = Level 0, 1 = subsolo). */
	NOCLIP_API int32 PickWeighted(FRandomStream& Stream, int32 Floor = 0);
}
