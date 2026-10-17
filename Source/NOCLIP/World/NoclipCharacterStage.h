// Copyright Fuly Games. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "NoclipCharacterStage.generated.h"

class USceneCaptureComponent2D;
class USkeletalMeshComponent;
class UStaticMeshComponent;
class USpotLightComponent;
class UPointLightComponent;
class UTextureRenderTarget2D;

/**
 * Carrossel 3D da ficha de admissao (CharacterSelect.dc.html): os estagiarios num pedestal giratorio
 * de carpete, filmados por uma SceneCapture que a tela desenha na UI. So existe na maquina local
 * (a tela cria ao abrir e destroi ao fechar), longe de tudo e com lista "show only".
 */
UCLASS(NotPlaceable, Transient)
class NOCLIP_API ANoclipCharacterStage : public AActor
{
	GENERATED_BODY()

public:
	ANoclipCharacterStage();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	/** Posicao do carrossel (pode passar das bordas: gira sem fim). O escolhido fica na frente. */
	void SetPosition(int32 InPosition) { Position = InPosition; }

	UTextureRenderTarget2D* GetTarget() const { return Target; }

	static constexpr float CaptureWidth = 760.f;
	static constexpr float CaptureHeight = 420.f;

protected:
	UPROPERTY(VisibleAnywhere, Category = "Components") TObjectPtr<USceneComponent> Root;
	UPROPERTY(VisibleAnywhere, Category = "Components") TObjectPtr<USceneComponent> Ring;
	UPROPERTY(VisibleAnywhere, Category = "Components") TObjectPtr<UStaticMeshComponent> Turntable;
	UPROPERTY(VisibleAnywhere, Category = "Components") TObjectPtr<UStaticMeshComponent> TurntableRim;
	UPROPERTY(VisibleAnywhere, Category = "Components") TObjectPtr<USpotLightComponent> KeyLight;
	UPROPERTY(VisibleAnywhere, Category = "Components") TObjectPtr<UPointLightComponent> FillLight;
	UPROPERTY(VisibleAnywhere, Category = "Components") TObjectPtr<USceneCaptureComponent2D> Capture;

	UPROPERTY(Transient) TArray<TObjectPtr<USkeletalMeshComponent>> Slots;
	UPROPERTY(Transient) TObjectPtr<UTextureRenderTarget2D> Target;

private:
	int32 Position = 0;
	float RingYaw = 0.f;
	float CaptureAccumulator = 0.f;
};
