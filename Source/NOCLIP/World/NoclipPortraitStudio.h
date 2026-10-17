// Copyright Fuly Games. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "NoclipPortraitStudio.generated.h"

class UPointLightComponent;
class USceneCaptureComponent2D;
class USkeletalMeshComponent;
class UStaticMeshComponent;
class UTextureRenderTarget2D;
class UMaterialInstanceDynamic;

/**
 * "Estudio de foto 3x4": renderiza o rosto do estagiario (modelo de verdade, em idle) numa textura pequena
 * com filtro nearest, para a foto do cracha e da ficha de admissao. Ator local, criado pela tela que mostra
 * a foto e destruido junto com ela; a captura usa lista "show only", entao pode ficar em qualquer lugar.
 */
UCLASS(NotPlaceable, Transient)
class NOCLIP_API ANoclipPortraitStudio : public AActor
{
	GENERATED_BODY()

public:
	ANoclipPortraitStudio();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	/** Cria um estudio longe de tudo (cada um num ponto diferente). */
	static ANoclipPortraitStudio* SpawnStudio(UWorld* World, int32 CharacterSlot);

	/** Troca quem esta na foto (indice em NoclipCharacters; vagas trancadas viram silhueta). */
	void SetCharacter(int32 CharacterSlot);

	UTextureRenderTarget2D* GetTarget() const { return Target; }

	/** Material de UI pronto (so RGB, opaco) para um UImage. Criado sob demanda com Outer = InOuter. */
	UMaterialInstanceDynamic* MakeUIMaterial(UObject* InOuter) const;

	static constexpr int32 TargetWidth = 96;
	static constexpr int32 TargetHeight = 128;

protected:
	UPROPERTY(VisibleAnywhere, Category = "Components") TObjectPtr<USceneComponent> Root;
	UPROPERTY(VisibleAnywhere, Category = "Components") TObjectPtr<USkeletalMeshComponent> Mesh;
	UPROPERTY(VisibleAnywhere, Category = "Components") TObjectPtr<UStaticMeshComponent> Backdrop;
	UPROPERTY(VisibleAnywhere, Category = "Components") TObjectPtr<UPointLightComponent> KeyLight;
	UPROPERTY(VisibleAnywhere, Category = "Components") TObjectPtr<UPointLightComponent> FillLight;
	UPROPERTY(VisibleAnywhere, Category = "Components") TObjectPtr<USceneCaptureComponent2D> Capture;

	UPROPERTY(Transient) TObjectPtr<UTextureRenderTarget2D> Target;

private:
	void UpdateFraming(float DeltaSeconds, bool bSnap);

	int32 CharacterSlot = -1;
	FVector SmoothedHead = FVector::ZeroVector;
	bool bHasHead = false;
	float CaptureAccumulator = 0.f;
	int32 WarmupFrames = 0;
};
