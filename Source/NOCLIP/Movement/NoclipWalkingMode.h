// Copyright Fuly Games. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "DefaultMovementSet/Modes/WalkingMode.h"
#include "NoclipWalkingMode.generated.h"

/**
 * Walking do NOCLIP: igual ao WalkingMode padrao, mas a velocidade maxima e a aceleracao
 * sao escaladas pelos inputs de sprint e peso carregado (FNoclipMoverInputs).
 */
UCLASS(Blueprintable, BlueprintType)
class NOCLIP_API UNoclipWalkingMode : public UWalkingMode
{
	GENERATED_UCLASS_BODY()

public:
	virtual void GenerateMove_Implementation(const FMoverSimContext& SimContext, const FMoverTickStartData& StartState, const FMoverTimeStep& TimeStep, FProposedMove& OutProposedMove) const override;

	/** Multiplicador de velocidade ao correr. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Noclip|Sprint", meta = (ClampMin = "1.0"))
	float SprintSpeedMultiplier = 1.65f;

	/** Multiplicador de aceleracao ao correr (arrancada mais forte). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Noclip|Sprint", meta = (ClampMin = "1.0"))
	float SprintAccelerationMultiplier = 1.25f;

	/** Abaixo desta escala de peso o sprint fica bloqueado. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Noclip|Sprint", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float SprintBlockedBelowCarryScale = 0.75f;

	/** Sprint so vale se o input aponta para frente (dot com a camera acima deste valor). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Noclip|Sprint", meta = (ClampMin = "-1.0", ClampMax = "1.0"))
	float SprintMinForwardDot = 0.35f;
};
