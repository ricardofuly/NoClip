// Copyright Fuly Games. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "DefaultMovementSet/CharacterMoverComponent.h"
#include "NoclipMoverComponent.generated.h"

/**
 * CharacterMoverComponent do NOCLIP:
 * - troca o Walking padrao pelo UNoclipWalkingMode (sprint e peso);
 * - adiciona coyote time ao pulo.
 */
UCLASS(BlueprintType, Blueprintable, meta = (BlueprintSpawnableComponent))
class NOCLIP_API UNoclipMoverComponent : public UCharacterMoverComponent
{
	GENERATED_BODY()

public:
	UNoclipMoverComponent();

	virtual bool CanActorJump() const override;
	virtual bool Jump() override;

	/** Janela apos sair do chao em que o pulo ainda e aceito (ms). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Noclip|Jump", meta = (ClampMin = "0", Units = "ms"))
	float CoyoteTimeMs = 120.f;

protected:
	virtual void OnMoverPreSimulationTick(const FMoverTimeStep& TimeStep, const FMoverInputCmdContext& InputCmd) override;

private:
	double LastGroundedSimTimeMs = -1.0;
	double CurrentSimTimeMs = 0.0;
	bool bJumpedSinceGrounded = false;
};
