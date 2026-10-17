// Copyright Fuly Games. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "NoclipToolTypes.generated.h"

class UStaticMesh;
class UWorld;

/** Ferramentas de mao do estagiario (cinto com 3 vagas: 1 lanterna, 2 kit medico, 3 celular). */
UENUM(BlueprintType)
enum class ENoclipTool : uint8
{
	None = 0,
	Flashlight = 1,
	Medkit = 2,
	Phone = 3
};

/** O que a ferramenta na mao esta fazendo (replicado: decide a pose do Control Rig para todo mundo). */
UENUM(BlueprintType)
enum class ENoclipToolAction : uint8
{
	/** So segurando. */
	Hold = 0,
	/** Kit medico: aplicando o curativo. */
	Using = 1,
	/** Celular levantado na frente do rosto. */
	Looking = 2
};

namespace NoclipTools
{
	struct FToolDef
	{
		ENoclipTool Tool = ENoclipTool::None;
		FText Name;
		FText Short;
		/** Tecla do cinto (1..3). */
		int32 Slot = 0;
		FString MeshPath;
		float MeshScale = 1.f;
		/** Rotacao do modelo em relacao a mira (X = para onde o estagiario olha). */
		FRotator AimRotation = FRotator::ZeroRotator;
		/** Deslocamento do modelo a partir do osso da mao, no espaco da mira (cm). */
		FVector GripOffset = FVector::ZeroVector;
		/** Primeira pessoa: posicao no espaco da camera (X frente, Y direita, Z cima). */
		FVector ViewOffset = FVector::ZeroVector;
		FRotator ViewRotation = FRotator::ZeroRotator;
	};

	NOCLIP_API const FToolDef& Get(ENoclipTool Tool);
	NOCLIP_API UStaticMesh* LoadMesh(ENoclipTool Tool);

	/**
	 * Sinal do celular (0..3 barras) num ponto: a "antena" e a Sala Manila (as Backrooms nao tem torre).
	 * Sem Sala Manila no mapa (saguao, sala de espera): -1 (sem localizacao).
	 * Usado pela tela do celular e pela qualidade da ligacao de voz.
	 */
	NOCLIP_API int32 PhoneSignalBars(const UWorld* World, const FVector& Location);

	/** Bit da ferramenta na mascara de posse do PlayerState. */
	inline uint8 Bit(ENoclipTool Tool) { return static_cast<uint8>(1u << static_cast<uint8>(Tool)); }

	/** Tecla 1..3 -> ferramenta. */
	inline ENoclipTool FromSlot(int32 Slot)
	{
		return Slot == 1 ? ENoclipTool::Flashlight : (Slot == 2 ? ENoclipTool::Medkit : (Slot == 3 ? ENoclipTool::Phone : ENoclipTool::None));
	}

	/** Achados a partir desse peso precisam das duas maos (kg). */
	constexpr float TwoHandMassKg = 15.f;

	/** Kit medico: duracao do curativo (s) e quanto cura. */
	constexpr float MedkitUseSeconds = 2.4f;
	constexpr float MedkitHeal = 60.f;
	constexpr int32 MaxMedkits = 3;

	/** Lanterna: segundos de luz com a bateria cheia; abaixo de LowBattery ela pisca. */
	constexpr float FlashlightBatterySeconds = 240.f;
	constexpr float FlashlightLowBattery = 0.2f;
}
