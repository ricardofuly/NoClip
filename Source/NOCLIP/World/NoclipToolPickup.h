// Copyright Fuly Games. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Character/NoclipToolTypes.h"
#include "NoclipToolPickup.generated.h"

class UStaticMeshComponent;

/**
 * Ferramenta solta no chao (lanterna, kit medico, celular). Qualquer estagiario pega com [E]
 * se tiver a vaga livre no cinto; da para passar o kit medico para um colega assim.
 */
UCLASS(NotPlaceable)
class NOCLIP_API ANoclipToolPickup : public AActor
{
	GENERATED_BODY()

public:
	ANoclipToolPickup();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** Servidor, antes do FinishSpawning. */
	void Setup(ENoclipTool InTool, uint8 InBattery);

	ENoclipTool GetTool() const { return Tool; }
	uint8 GetBattery() const { return Battery; }

protected:
	UFUNCTION()
	void OnRep_Tool();

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UStaticMeshComponent> Mesh;

	UPROPERTY(ReplicatedUsing = OnRep_Tool)
	ENoclipTool Tool = ENoclipTool::None;

	UPROPERTY(Replicated)
	uint8 Battery = 255;
};
