// Copyright Fuly Games. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "NoclipExtractionZone.generated.h"

class UBoxComponent;
class UStaticMeshComponent;
class UTextRenderComponent;
class UMaterialInterface;

/**
 * Sala Manila: a saida do turno. Os achados dentro dela no fim do turno contam para a quota.
 * Se todos os jogadores ficarem dentro por ExtractHoldSeconds, o elevador sobe e o turno acaba mais cedo.
 * Spawnada pelo servidor (replicada) a partir do gerador.
 */
UCLASS(Blueprintable)
class NOCLIP_API ANoclipExtractionZone : public AActor
{
	GENERATED_BODY()

public:
	ANoclipExtractionZone();

	virtual void Tick(float DeltaSeconds) override;

	/** Define o tamanho (meia-extensao no plano XY, cm) e a altura. */
	void Setup(const FVector2D& HalfExtent, float Height, UMaterialInterface* MarkerMaterial);

	/** Soma valor/itens dentro da sala e jogadores dentro dela. */
	void ComputeContents(int32& OutValue, int32& OutItems, int32& OutPlayers) const;

	void SetElevatorLocked(bool bLocked) { bElevatorLocked = bLocked; }

	UFUNCTION(BlueprintPure, Category = "Noclip|Extraction")
	bool IsInside(const FVector& WorldLocation) const;

	/** Tempo com todos dentro para o elevador subir. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Noclip|Extraction", meta = (ClampMin = "0.5"))
	float ExtractHoldSeconds = 5.f;

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UBoxComponent> Volume;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> FloorMarker;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UTextRenderComponent> Sign;

	UPROPERTY(ReplicatedUsing = OnRep_Layout)
	FVector2D ReplicatedHalfExtent = FVector2D(300.f, 300.f);

	UPROPERTY(Replicated)
	float ReplicatedHeight = 290.f;

	UPROPERTY(Replicated)
	TObjectPtr<UMaterialInterface> ReplicatedMarkerMaterial;

	UFUNCTION()
	void OnRep_Layout();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

private:
	void ApplyLayout();

	float HoldProgress = 0.f;
	bool bElevatorLocked = false;
};
