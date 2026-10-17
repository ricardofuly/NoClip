// Copyright Fuly Games. All Rights Reserved.

#include "Movement/NoclipMoverComponent.h"
#include "Movement/NoclipWalkingMode.h"
#include "MoverSimulationTypes.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(NoclipMoverComponent)

UNoclipMoverComponent::UNoclipMoverComponent()
{
	// Substitui o Walking criado pelo UCharacterMoverComponent.
	MovementModes.Add(DefaultModeNames::Walking, CreateDefaultSubobject<UNoclipWalkingMode>(TEXT("NoclipWalkingMode")));
	StartingMovementMode = DefaultModeNames::Falling;

	// Agachar/slide entram como modo proprio do NOCLIP depois.
	bHandleStanceChanges = false;
}

void UNoclipMoverComponent::OnMoverPreSimulationTick(const FMoverTimeStep& TimeStep, const FMoverInputCmdContext& InputCmd)
{
	CurrentSimTimeMs = TimeStep.BaseSimTimeMs;

	if (IsOnGround())
	{
		LastGroundedSimTimeMs = CurrentSimTimeMs;
		bJumpedSinceGrounded = false;
	}

	Super::OnMoverPreSimulationTick(TimeStep, InputCmd);
}

bool UNoclipMoverComponent::CanActorJump() const
{
	if (IsOnGround())
	{
		return true;
	}

	const bool bInCoyoteWindow = LastGroundedSimTimeMs >= 0.0 && (CurrentSimTimeMs - LastGroundedSimTimeMs) <= CoyoteTimeMs;
	return IsFalling() && bInCoyoteWindow && !bJumpedSinceGrounded;
}

bool UNoclipMoverComponent::Jump()
{
	const bool bJumped = Super::Jump();
	if (bJumped)
	{
		bJumpedSinceGrounded = true;
	}
	return bJumped;
}
