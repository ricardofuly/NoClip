// Copyright Fuly Games. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "NoclipElevator.generated.h"

class UStaticMeshComponent;
class UPointLightComponent;
class UTextRenderComponent;
class UMaterialInterface;

/**
 * Elevador da Baseline Recuperacoes: cabine de 3,6 x 3 m para ate 6 estagiarios, com portas de correr.
 * A origem fica no centro da porta, no chao; a cabine se estende para tras (-X) e as portas abrem para +X.
 * Replicado: o servidor manda abrir/fechar e cada maquina anima as portas pelo relogio do servidor.
 * Usado na sala de espera (embarque) e no Level 0 (chegada de cada turno).
 */
UCLASS()
class NOCLIP_API ANoclipElevator : public AActor
{
	GENERATED_BODY()

public:
	ANoclipElevator();

	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** Servidor: abre ou fecha. bInstant pula a animacao (respawn com a cabine cheia). */
	void SetDoorsOpen(bool bOpen, bool bInstant = false);

	bool AreDoorsOpen() const { return bDoorsOpen; }

	/** 0 = fechada, 1 = aberta (animado). */
	UFUNCTION(BlueprintPure, Category = "Noclip|Elevator")
	float GetDoorAlpha() const;

	/** Lugar N (0..5) dentro da cabine, em coordenadas do mundo, de frente para a porta. Z = centro da capsula. */
	FTransform GetCabinSlot(int32 Index, float CapsuleHalfHeight = 88.f) const;

	/** Ponto logo na frente da porta, do lado de fora (mundo). */
	FVector GetDoorFront(float Distance = 120.f) const;

	/** Segundos que as portas levam para abrir/fechar. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Noclip|Elevator")
	float DoorTravelSeconds = 1.4f;

	/** Andar mostrado no painel acima da porta. */
	UPROPERTY(EditAnywhere, ReplicatedUsing = OnRep_Floor, BlueprintReadOnly, Category = "Noclip|Elevator")
	FString FloorLabel = TEXT("0");

	void SetFloorLabel(const FString& InLabel);

	/** Eventos locais de game feel (som de campainha, motor). */
	UFUNCTION(BlueprintImplementableEvent, Category = "Noclip|Elevator")
	void OnDoorsStartMoving(bool bOpening);

protected:
	virtual void BeginPlay() override;

	UFUNCTION()
	void OnRep_Doors();

	UFUNCTION()
	void OnRep_Floor();

	double GetServerNow() const;

	UPROPERTY(VisibleAnywhere, Category = "Components") TObjectPtr<USceneComponent> Root;
	UPROPERTY(VisibleAnywhere, Category = "Components") TObjectPtr<UStaticMeshComponent> DoorLeft;
	UPROPERTY(VisibleAnywhere, Category = "Components") TObjectPtr<UStaticMeshComponent> DoorRight;
	UPROPERTY(VisibleAnywhere, Category = "Components") TObjectPtr<UPointLightComponent> CabinLight;
	UPROPERTY(VisibleAnywhere, Category = "Components") TObjectPtr<UTextRenderComponent> FloorText;
	UPROPERTY(VisibleAnywhere, Category = "Components") TObjectPtr<UStaticMeshComponent> CallLight;

	UPROPERTY(ReplicatedUsing = OnRep_Doors)
	bool bDoorsOpen = false;

	/** Tempo do servidor em que a ultima abertura/fechamento comecou (negativo = sem animacao). */
	UPROPERTY(ReplicatedUsing = OnRep_Doors)
	double DoorChangeServerTime = -100.0;

private:
	UPROPERTY(Transient) TObjectPtr<UMaterialInterface> MetalMaterial;
	UPROPERTY(Transient) TObjectPtr<UMaterialInterface> CallOnMaterial;
	UPROPERTY(Transient) TObjectPtr<UMaterialInterface> CallOffMaterial;
	bool bLastFiredOpen = false;
	bool bFiredOnce = false;
};
