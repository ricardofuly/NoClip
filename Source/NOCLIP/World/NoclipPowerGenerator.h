// Copyright Fuly Games. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "NoclipPowerGenerator.generated.h"

class UStaticMeshComponent;
class UPointLightComponent;
class UTextRenderComponent;
class ANoclipCharacter;

/**
 * Gerador de energia do turno. No apagao (GameState::IsPowerOut) a equipe vem ate ele e cada um faz o
 * "quick time event" (acertar o ponteiro na faixa verde). Cada acerto soma carga; quanto mais gente acertando,
 * mais rapido a energia volta. Errar faz faisca e barulho (as entidades escutam) e perde um pouco de carga.
 * O servidor guarda a carga (replicada); o QTE roda no dono e manda o resultado.
 */
UCLASS()
class NOCLIP_API ANoclipPowerGenerator : public AActor
{
	GENERATED_BODY()
	friend struct FNoclipTestAccess; // testes de automacao (Tests/)

public:
	ANoclipPowerGenerator();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void Tick(float DeltaSeconds) override;

	/** 0..1 durante o apagao. */
	float GetProgress() const { return Progress; }
	/** Quantos estagiarios mexeram nele nos ultimos segundos. */
	int32 GetWorkers() const { return Workers; }

	/** Ponto de onde se opera o painel (para a distancia do QTE). */
	FVector GetPanelLocation() const;
	bool IsInReach(const FVector& Location) const;

	/** Servidor: apagao comecou (zera a carga). */
	void BeginOutage();
	/** Servidor: resultado de uma tentativa do QTE. */
	void ServerAttempt(ANoclipCharacter* By, bool bHit);

	/** Alcance para operar o painel (cm). */
	static constexpr float ReachDistance = 230.f;

protected:
	UFUNCTION()
	void OnRep_Progress(float OldProgress);

	UPROPERTY(VisibleAnywhere, Category = "Components") TObjectPtr<USceneComponent> Root;
	/** Gerador de PuzzleMachinery (230 x 82 x 164 cm, pivo no chao). */
	UPROPERTY(VisibleAnywhere, Category = "Components") TObjectPtr<UStaticMeshComponent> Body;
	/** Lampada acesa (ligado) e apagada (idle) do proprio modelo. */
	UPROPERTY(VisibleAnywhere, Category = "Components") TObjectPtr<UStaticMeshComponent> LampOn;
	UPROPERTY(VisibleAnywhere, Category = "Components") TObjectPtr<UStaticMeshComponent> LampIdle;
	UPROPERTY(VisibleAnywhere, Category = "Components") TObjectPtr<UPointLightComponent> StatusLight;
	UPROPERTY(VisibleAnywhere, Category = "Components") TObjectPtr<UTextRenderComponent> Label;

	UPROPERTY(ReplicatedUsing = OnRep_Progress)
	float Progress = 0.f;

	UPROPERTY(Replicated)
	uint8 Workers = 0;

	/** Carga por acerto e perda por erro. */
	UPROPERTY(EditAnywhere, Category = "Noclip|Power")
	float HitGain = 0.075f;

	UPROPERTY(EditAnywhere, Category = "Noclip|Power")
	float MissLoss = 0.04f;

private:
	void FeedbackSound(bool bHit) const;
	TMap<TWeakObjectPtr<ANoclipCharacter>, double> LastAttempt;
};
