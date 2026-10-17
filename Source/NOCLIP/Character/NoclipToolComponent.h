// Copyright Fuly Games. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Character/NoclipToolTypes.h"
#include "NoclipToolComponent.generated.h"

class ANoclipCharacter;
class ANoclipPlayerState;
class ANoclipToolPickup;
class UStaticMeshComponent;
class USpotLightComponent;

/** Por que uma acao com as maos foi negada (aviso na tela + "nao" com a cabeca). */
UENUM()
enum class ENoclipHandsDenyReason : uint8
{
	None,
	/** Quer pegar um achado pesado com uma ferramenta na mao. */
	HeavyNeedsBothHands,
	/** Quer tirar uma ferramenta do cinto carregando um achado pesado. */
	HandsFullHeavy,
	/** Nao tem essa ferramenta (ou acabaram os kits). */
	NotOwned,
	/** Kit medico com a vida cheia. */
	AlreadyHealthy,
	/** Lanterna sem bateria. */
	NoBattery
};

/**
 * Cinto de ferramentas do estagiario: lanterna, kit medico e celular. Uma por vez na mao direita
 * (a outra mao fica livre para achados leves). Achados pesados precisam das duas maos: com uma ferramenta
 * na mao a pegada e negada (aviso + balancar a cabeca) ate guardar [1-3] ou soltar [G] a ferramenta.
 *
 * O servidor e a autoridade (posse no PlayerState, bateria, curativo); a pose do corpo sai daqui para o
 * Control Rig das maos (UNoclipCharacterAnimInstance) e o modelo em primeira pessoa e so do dono.
 */
UCLASS(ClassGroup = (Noclip), meta = (BlueprintSpawnableComponent))
class NOCLIP_API UNoclipToolComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UNoclipToolComponent();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// ---------- Entrada do dono ----------
	/** Tecla 1..3: tira a ferramenta do cinto ou guarda se ja estiver na mao. */
	void RequestSlot(int32 Slot);
	/** Botao direito / LB: liga a lanterna, aplica o kit, levanta o celular. */
	void RequestUse(bool bPressed);
	/** G / direcional para baixo: solta a ferramenta da mao no chao. */
	void RequestDrop();
	/** Pega uma ferramenta solta no chao (chamado pelo agarrar quando a mira esta nela). */
	void RequestPickup(ANoclipToolPickup* Pickup);

	/** Feedback de "nao pode": aviso local + balancar a cabeca (todo mundo ve). */
	void Deny(ENoclipHandsDenyReason Reason);

	// ---------- Estado ----------
	ENoclipTool GetEquipped() const { return Equipped; }
	ENoclipToolAction GetAction() const { return Action; }
	bool HasToolInHand() const { return Equipped != ENoclipTool::None; }
	bool IsFlashlightOn() const { return bFlashlightOn; }
	float GetBattery() const { return BatteryPct / 255.f; }
	/** 0..1 do curativo em andamento (0 sem curativo). */
	float GetUseProgress() const;
	bool IsPhoneRaised() const { return Equipped == ENoclipTool::Phone && Action == ENoclipToolAction::Looking; }

	/** Ultimo aviso local (para a HUD): motivo e quando (segundos do mundo). */
	ENoclipHandsDenyReason GetLastDenyReason() const { return LastDenyReason; }
	double GetLastDenyTime() const { return LastDenyTime; }
	/** Segundos desde o ultimo "nao" com a cabeca (qualquer maquina). */
	float GetHeadShakeAge() const;

	bool Owns(ENoclipTool Tool) const;
	int32 GetMedkitCount() const;

	/** Debug (noclip.ThirdPerson): mostra o item do corpo para o dono. */
	void SetBodyVisibleToOwner(bool bVisible);

	/** Servidor: guarda a ferramenta (respawn, colapso, achado pesado agarrado por outro caminho). */
	void ServerHolster();

protected:
	UFUNCTION(Server, Reliable) void Server_Equip(ENoclipTool Tool);
	UFUNCTION(Server, Reliable) void Server_Use(bool bPressed);
	UFUNCTION(Server, Reliable) void Server_Drop();
	UFUNCTION(Server, Reliable) void Server_Pickup(ANoclipToolPickup* Pickup);
	UFUNCTION(Server, Unreliable) void Server_Deny();
	UFUNCTION(NetMulticast, Unreliable) void Multicast_Deny();

	UFUNCTION() void OnRep_Equipped();
	UFUNCTION() void OnRep_Action();

	UPROPERTY(ReplicatedUsing = OnRep_Equipped)
	ENoclipTool Equipped = ENoclipTool::None;

	UPROPERTY(ReplicatedUsing = OnRep_Action)
	ENoclipToolAction Action = ENoclipToolAction::Hold;

	UPROPERTY(Replicated)
	bool bFlashlightOn = false;

	/** Bateria da lanterna (0..255), so muda no servidor. */
	UPROPERTY(Replicated)
	uint8 BatteryPct = 255;

	/** Inicio do curativo (relogio do servidor). */
	UPROPERTY(Replicated)
	float UseStartServerTime = 0.f;

	// Visual (criado no BeginPlay em toda maquina com tela)
	UPROPERTY(Transient) TObjectPtr<UStaticMeshComponent> BodyItem;
	UPROPERTY(Transient) TObjectPtr<UStaticMeshComponent> ViewItem;
	UPROPERTY(Transient) TObjectPtr<USpotLightComponent> Beam;

private:
	ANoclipCharacter* GetCharacter() const;
	ANoclipPlayerState* GetNoclipPlayerState() const;
	float GetServerTime() const;
	bool IsHoldingHeavyLoot() const;
	void ApplyVisuals();
	void UpdateVisuals(float DeltaTime);
	void FinishMedkit();
	void LocalHeadShake();

	float BatteryDrain = 0.f;
	float FlickerTime = 0.f;
	float ViewBlend = 0.f;
	float ViewBob = 0.f;
	double LastDenyTime = -100.0;
	double HeadShakeTime = -100.0;
	ENoclipHandsDenyReason LastDenyReason = ENoclipHandsDenyReason::None;
	ENoclipTool VisualTool = ENoclipTool::None;
};
