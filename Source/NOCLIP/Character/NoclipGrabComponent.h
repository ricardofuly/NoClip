// Copyright Fuly Games. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "NoclipGrabComponent.generated.h"

class UPhysicsHandleComponent;
class UPrimitiveComponent;
class ANoclipCharacter;
struct FHitResult;

/**
 * Agarrar e arremessar objetos com fisica. O servidor e a autoridade (physics handle so roda la);
 * o dono le o peso carregado replicado para montar o input do Mover.
 */
UCLASS(ClassGroup = (Noclip), meta = (BlueprintSpawnableComponent))
class NOCLIP_API UNoclipGrabComponent : public UActorComponent
{
	GENERATED_BODY()
	friend struct FNoclipTestAccess; // testes de automacao (Tests/)

public:
	UNoclipGrabComponent();

	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** Chamado pelo dono: agarra o que estiver na mira, ou solta o que estiver segurando. */
	UFUNCTION(BlueprintCallable, Category = "Noclip|Grab")
	void ToggleGrab();

	/** Chamado pelo dono ao apertar o botao de arremesso. */
	UFUNCTION(BlueprintCallable, Category = "Noclip|Grab")
	void BeginThrowCharge();

	/** Chamado pelo dono ao soltar o botao de arremesso. */
	UFUNCTION(BlueprintCallable, Category = "Noclip|Grab")
	void ReleaseThrow();

	UFUNCTION(BlueprintPure, Category = "Noclip|Grab")
	bool IsHolding() const { return HeldActor != nullptr; }

	UFUNCTION(BlueprintPure, Category = "Noclip|Grab")
	AActor* GetHeldActor() const { return HeldActor; }

	/** Segurando um achado que precisa das duas maos (NoclipTools::TwoHandMassKg ou mais)? */
	bool IsHoldingTwoHanded() const;
	static bool IsTwoHandedActor(const AActor* Actor);

	/** Servidor: solta o que estiver segurando, sem arremesso (colapso, respawn). */
	void ForceDrop();

	/** 1 = sem penalidade; menor = mais lento. Abaixo de 0.75 o sprint trava. */
	UFUNCTION(BlueprintPure, Category = "Noclip|Grab")
	float GetCarrySpeedScale() const { return HeldActor ? CarrySpeedScale : 1.f; }

	/**
	 * O que esta na mira e pode ser agarrado (consulta local, para a HUD).
	 * bOutTooHeavy = true quando ha um objeto com fisica, mas acima de MaxGrabMassKg.
	 */
	AActor* FindGrabCandidate(bool& bOutTooHeavy) const;

	// ---------- Carregar em equipe ----------
	/** Servidor: a parte do peso que cabe a este carregador (chamado pelo achado quando alguem entra/sai). */
	void ApplyCarryShare(int32 Carriers, int32 Required, float MassKg);
	/** Onde este jogador "segura" agora (olho + mira * distancia). */
	FVector GetHoldTarget() const;
	/** Achado de equipe ainda sem gente suficiente: so arrasta. */
	bool IsDragging() const;

	/** Velocidade arrastando sozinho um achado de equipe. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Noclip|Grab", meta = (ClampMin = "0.1", ClampMax = "1"))
	float DragSpeedScale = 0.32f;

	/** Carga atual do arremesso, 0..1 (so no dono). */
	UFUNCTION(BlueprintPure, Category = "Noclip|Grab")
	float GetThrowCharge() const;

	// ---------- Ajustes ----------
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Noclip|Grab")
	float GrabReach = 230.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Noclip|Grab")
	float GrabSweepRadius = 14.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Noclip|Grab")
	float MinHoldDistance = 90.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Noclip|Grab")
	float MaxHoldDistance = 190.f;

	/** Se o objeto ficar mais longe que isso do ponto de segurar, escapa da mao. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Noclip|Grab")
	float BreakDistance = 320.f;

	/** Massa maxima que um jogador consegue agarrar (kg). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Noclip|Grab")
	float MaxGrabMassKg = 120.f;

	/** Velocidade da mola com objeto leve / pesado (maior = segue a mao mais rapido). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Noclip|Grab")
	float LightInterpSpeed = 18.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Noclip|Grab")
	float HeavyInterpSpeed = 4.f;

	/** Penalidade maxima de velocidade com o objeto mais pesado (0.55 = anda a 45%). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Noclip|Grab", meta = (ClampMin = "0", ClampMax = "0.9"))
	float MaxCarryPenalty = 0.55f;

	/** Multiplicador extra de velocidade com achados de duas maos. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Noclip|Grab", meta = (ClampMin = "0.3", ClampMax = "1"))
	float TwoHandedSpeedFactor = 0.8f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Noclip|Throw")
	float ThrowMaxChargeTime = 1.f;

	/** Velocidade aplicada ao objeto no arremesso sem carga / com carga total (cm/s). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Noclip|Throw")
	float ThrowMinSpeed = 350.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Noclip|Throw")
	float ThrowMaxSpeed = 1500.f;

	/** Eventos locais de game feel (som, shake, hitstop). */
	UFUNCTION(BlueprintImplementableEvent, Category = "Noclip|Grab")
	void OnGrabFeel(AActor* GrabbedActor);

	UFUNCTION(BlueprintImplementableEvent, Category = "Noclip|Grab")
	void OnThrowFeel(float Charge);

protected:
	UFUNCTION(Server, Reliable)
	void Server_TryGrab();

	UFUNCTION(Server, Reliable)
	void Server_Release();

	UFUNCTION(Server, Reliable)
	void Server_Throw(float Charge);

	UFUNCTION()
	void OnRep_HeldActor();

	/** Servidor: agarra o que este hit acertou (o Server_TryGrab faz o trace da mira e chama isto). */
	void ServerGrabFromHit(const FHitResult& Hit);

	void ReleaseInternal();
	/** Servidor: passa a guiar a fisica do achado de equipe (o lider anterior soltou). */
	void BecomeLead();
	bool IsLead() const;
	void TickTeamCarry(float DeltaTime);
	double NextDragNoiseTime = 0.0;
	ANoclipCharacter* GetNoclipOwner() const;

	UPROPERTY(ReplicatedUsing = OnRep_HeldActor)
	TObjectPtr<AActor> HeldActor;

	UPROPERTY(Replicated)
	float CarrySpeedScale = 1.f;

	UPROPERTY(Transient)
	TObjectPtr<UPhysicsHandleComponent> PhysicsHandle;

	UPROPERTY(Transient)
	TObjectPtr<UPrimitiveComponent> HeldComponent;

	float HoldDistance = 140.f;
	double ThrowChargeStartTime = -1.0;
};
