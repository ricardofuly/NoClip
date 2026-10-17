// Copyright Fuly Games. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "NoclipLootActor.generated.h"

class UStaticMeshComponent;
class ANoclipPlayerState;
class ANoclipCharacter;

/**
 * "Achado": objeto do mundo real perdido nas Backrooms. Fisica simulada no servidor e replicada.
 * Perde valor ao bater forte.
 */
UCLASS(Blueprintable)
class NOCLIP_API ANoclipLootActor : public AActor
{
	GENERATED_BODY()
	friend struct FNoclipTestAccess; // testes de automacao (Tests/)

public:
	ANoclipLootActor();

	virtual void BeginPlay() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UFUNCTION(BlueprintPure, Category = "Noclip|Loot")
	int32 GetCurrentValue() const { return CurrentValue; }

	UFUNCTION(BlueprintPure, Category = "Noclip|Loot")
	int32 GetBaseValue() const { return BaseValue; }

	UFUNCTION(BlueprintPure, Category = "Noclip|Loot")
	FText GetDisplayName() const { return DisplayName; }

	UFUNCTION(BlueprintPure, Category = "Noclip|Loot")
	FText GetDescription() const { return Description; }

	/**
	 * Servidor, entre o SpawnActorDeferred e o FinishSpawning: veste um achado do catalogo do escritorio
	 * (modelo, nome, texto, massa, fragilidade) e sorteia o valor. Os clientes recebem so o indice.
	 */
	void SetupFromCatalog(int32 Index, FRandomStream& Stream);

	UFUNCTION(BlueprintPure, Category = "Noclip|Loot")
	float GetMassKg() const { return MassKg; }

	/** Servidor: quem agarrou por ultimo (para as estatisticas do turno). */
	void NotifyGrabbed(ANoclipPlayerState* Holder);

	/** Servidor: foi arremessado agora deste ponto (mede o arremesso no primeiro impacto). */
	void NotifyThrown(ANoclipPlayerState* Thrower);

	ANoclipPlayerState* GetLastHolder() const { return LastHolder.Get(); }

	// ---------- Carregar em equipe ----------
	/** Quantos estagiarios para levantar (menos que isso, o achado so e arrastado). */
	int32 GetRequiredCarriers() const { return RequiredCarriers; }
	/** Quantos estao segurando agora (replicado). */
	int32 GetNumCarriers() const { return NumCarriers; }
	bool IsTeamCarry() const { return RequiredCarriers > 1; }
	/** Gente suficiente: sai do chao. */
	bool IsLifted() const { return NumCarriers >= RequiredCarriers; }

	/** Servidor. */
	void AddCarrier(ANoclipCharacter* Carrier);
	void RemoveCarrier(ANoclipCharacter* Carrier);
	bool IsCarriedBy(const ANoclipCharacter* Carrier) const;
	/** Quem guia a fisica (o primeiro que agarrou). */
	ANoclipCharacter* GetLeadCarrier() const;
	const TArray<TWeakObjectPtr<ANoclipCharacter>>& GetCarriers() const { return Carriers; }
	/** Servidor: recalcula a parte do peso de cada um (velocidade) depois que alguem entra ou sai. */
	void RefreshCarriers();
	/** Servidor: para onde a equipe esta levando (media dos pontos de segurar, na altura da cintura). */
	FVector ComputeTeamTarget() const;

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> Mesh;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Noclip|Loot")
	FText DisplayName;

	/** Linha de humor mostrada no cartao de quem carrega. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Noclip|Loot")
	FText Description;

	UPROPERTY(EditAnywhere, Replicated, BlueprintReadOnly, Category = "Noclip|Loot", meta = (ClampMin = "0"))
	int32 BaseValue = 50;

	/** Entrada do NoclipLoot (catalogo do escritorio); INDEX_NONE = achado configurado no Blueprint. */
	UPROPERTY(ReplicatedUsing = OnRep_CatalogIndex)
	int16 CatalogIndex = INDEX_NONE;

	UFUNCTION()
	void OnRep_CatalogIndex();

	void ApplyCatalog();

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Noclip|Loot", meta = (ClampMin = "0.1"))
	float MassKg = 15.f;

	/** Fracao do valor base perdida a cada 1000 cm/s de impacto acima do limiar. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Noclip|Loot", meta = (ClampMin = "0"))
	float Fragility = 0.25f;

	/** Impactos abaixo desta velocidade nao causam dano (cm/s). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Noclip|Loot")
	float DamageSpeedThreshold = 380.f;

	UPROPERTY(ReplicatedUsing = OnRep_CurrentValue, BlueprintReadOnly, Category = "Noclip|Loot")
	int32 CurrentValue = 0;

	/** Estagiarios para levantar (vem do catalogo; no Blueprint, edite aqui). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Noclip|Loot", meta = (ClampMin = "1", ClampMax = "4"))
	int32 RequiredCarriers = 1;

	UPROPERTY(Replicated)
	uint8 NumCarriers = 0;

	UFUNCTION()
	void OnRep_CurrentValue(int32 OldValue);

	/** Evento local de feedback de dano (numero flutuante vermelho, som de vidro). */
	UFUNCTION(BlueprintImplementableEvent, Category = "Noclip|Loot")
	void OnValueLost(int32 Lost, FVector ImpactLocation);

	UFUNCTION(NetMulticast, Unreliable)
	void Multicast_ImpactFeedback(int32 Lost, FVector_NetQuantize ImpactLocation);

	UFUNCTION()
	void HandleHit(UPrimitiveComponent* HitComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit);

private:
	double LastDamageTime = -10.0;
	double LastNoiseTime = -10.0;

	TWeakObjectPtr<ANoclipPlayerState> LastHolder;
	TArray<TWeakObjectPtr<ANoclipCharacter>> Carriers;
	TWeakObjectPtr<ANoclipPlayerState> PendingThrower;
	FVector ThrowOrigin = FVector::ZeroVector;
	double ThrowTime = -1.0;
};
