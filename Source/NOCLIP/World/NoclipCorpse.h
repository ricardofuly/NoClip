// Copyright Fuly Games. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "NoclipCorpse.generated.h"

class APlayerState;
class USkeletalMeshComponent;

/**
 * Corpo de um estagiario que morreu no turno. O servidor cria (com o modelo e o empurrao do golpe) e cada
 * maquina simula o ragdoll por conta propria: nao precisa bater igual, so precisa estar la. Some quando o
 * turno acaba (o nivel e regerado).
 */
UCLASS()
class NOCLIP_API ANoclipCorpse : public AActor
{
	GENERATED_BODY()

public:
	ANoclipCorpse();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** Servidor, antes do FinishSpawning. */
	void Setup(APlayerState* InOwner, int32 InCharacterIndex, const FVector& InImpulse);

	APlayerState* GetOwnerPlayer() const { return OwnerPlayer; }

	/** Onde o corpo esta agora (o ragdoll anda; o ator nao). */
	FVector GetBodyLocation() const;

protected:
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<USkeletalMeshComponent> Mesh;

	UPROPERTY(Replicated)
	TObjectPtr<APlayerState> OwnerPlayer;

	UPROPERTY(Replicated)
	int32 CharacterIndex = 0;

	/** Velocidade (cm/s) aplicada ao ragdoll quando ele cai. */
	UPROPERTY(Replicated)
	FVector_NetQuantize10 Impulse = FVector::ZeroVector;

private:
	void StartRagdoll();
};
