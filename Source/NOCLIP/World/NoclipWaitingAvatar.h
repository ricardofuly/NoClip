// Copyright Fuly Games. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "NoclipWaitingAvatar.generated.h"

class ANoclipWaitingRoom;
class APlayerState;
class USkeletalMeshComponent;

/**
 * Boneco de um estagiario na sala de espera (o modelo escolhido, visto pela camera de seguranca).
 * Nao replica movimento: cada maquina calcula a posicao pelo relogio do servidor (ANoclipWaitingRoom::EvaluateAvatar),
 * entao o embarque fica igual para todos sem trafego extra.
 */
UCLASS()
class NOCLIP_API ANoclipWaitingAvatar : public AActor
{
	GENERATED_BODY()

public:
	ANoclipWaitingAvatar();

	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** Servidor, antes do FinishSpawning. */
	void Setup(ANoclipWaitingRoom* InRoom, APlayerState* InPlayer, int32 InSlot);

	APlayerState* GetPlayerStateRef() const { return OwnerPlayer; }
	int32 GetSlot() const { return Slot; }
	bool IsInCabin() const { return bInCabin; }

	/** Onde o boneco estava na ultima imagem da camera de seguranca. */
	FVector GetCapturedLocation() const { return bHasCaptured ? CapturedLocation : GetActorLocation(); }
	bool WasInCabinWhenCaptured() const { return bHasCaptured ? bCapturedInCabin : bInCabin; }
	void NoteCaptured() { CapturedLocation = GetActorLocation(); bCapturedInCabin = bInCabin; bHasCaptured = true; }

	/** Altura do topo da cabeca (para a caixa de rastreio da camera). */
	static float GetHeight() { return 172.f; }

protected:
	UFUNCTION()
	void OnRep_Slot();

	UPROPERTY(VisibleAnywhere, Category = "Components") TObjectPtr<USceneComponent> Root;
	/** Modelo do estagiario escolhido pelo dono (troca sozinho quando ele troca de cracha). */
	UPROPERTY(VisibleAnywhere, Category = "Components") TObjectPtr<USkeletalMeshComponent> Mesh;

	UPROPERTY(Replicated)
	TObjectPtr<ANoclipWaitingRoom> Room;

	UPROPERTY(Replicated)
	TObjectPtr<APlayerState> OwnerPlayer;

	UPROPERTY(ReplicatedUsing = OnRep_Slot)
	int32 Slot = 0;

private:
	void SyncCharacter();
	int32 AppliedCharacter = -1;
	bool bInCabin = false;
	bool bTickLinked = false;
	bool bHadLocation = false;
	FVector CapturedLocation = FVector::ZeroVector;
	bool bCapturedInCabin = false;
	bool bHasCaptured = false;
};
