// Copyright Fuly Games. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "Character/NoclipCharacterCatalog.h"
#include "NoclipCharacterAnimInstance.generated.h"

class UAnimSequence;
class UControlRig;

/** Camada de pose pronta para o worker de animacao (copiada no PreUpdate). */
struct FNoclipAnimLayer
{
	const UAnimSequence* Sequence = nullptr;
	float Time = 0.f;
	float Weight = 0.f;
	bool bLoop = true;
};

/** Entradas do Control Rig das maos (CR_NoclipHands_*), em espaco do componente do mesh. */
struct FNoclipHandsRigInput
{
	bool bActive = false;
	float WeightR = 0.f;
	float WeightL = 0.f;
	FVector TargetR = FVector::ZeroVector;
	FVector TargetL = FVector::ZeroVector;
	FVector PoleR = FVector::ZeroVector;
	FVector PoleL = FVector::ZeroVector;
	/** Rotacao extra da cabeca no espaco local do osso ("nao", olhar para o celular / para o kit). */
	FQuat HeadOffset = FQuat::Identity;
};

/**
 * Locomocao dos estagiarios sem Anim Blueprint: parado / andar / correr / andar de lado / pulo, misturados
 * em C++ pela velocidade e direcao do dono (Mover no ANoclipCharacter, ou o deslocamento do ator em qualquer
 * outro, como o boneco da sala de espera e o carrossel da ficha de admissao).
 * As animacoes sao in-place; o ciclo e sincronizado por uma fase normalizada para os pes nao "patinarem"
 * na troca andar -> correr.
 */
UCLASS(Transient, NotBlueprintable)
class NOCLIP_API UNoclipCharacterAnimInstance : public UAnimInstance
{
	GENERATED_BODY()

public:
	/** Troca o conjunto de animacoes (indice em NoclipCharacters). */
	void SetCharacter(int32 Index);
	int32 GetCharacterIndex() const { return CharacterIndex; }

	/** Forca uma velocidade (cm/s, no espaco do ator) em vez de medir; Zero com bOverride=false volta a medir. */
	void SetVelocityOverride(bool bOverride, const FVector& LocalVelocity = FVector::ZeroVector);

	/** Fase inicial diferente por boneco (para nao respirarem todos juntos). */
	void SetPhaseOffset(float Seconds) { IdleTime = Seconds; }

	/** Velocidade suavizada no plano (cm/s). */
	float GetSpeed() const { return Speed; }

protected:
	virtual void NativeInitializeAnimation() override;
	virtual void NativeUpdateAnimation(float DeltaSeconds) override;
	virtual FAnimInstanceProxy* CreateAnimInstanceProxy() override;
	virtual void DestroyAnimInstanceProxy(FAnimInstanceProxy* InProxy) override;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UAnimSequence>> Anims;

	/**
	 * Control Rig das maos: IK de dois ossos em cada braco (alvos vindos do cinto de ferramentas e do agarrar)
	 * e o deslocamento da cabeca. Roda no worker de animacao depois da locomocao.
	 */
	UPROPERTY(Transient)
	TObjectPtr<UControlRig> HandsRig;

private:
	friend struct FNoclipAnimInstanceProxy;

	void MeasureVelocity(float DeltaSeconds, FVector& OutLocal, bool& bOutGrounded);
	void CreateHandsRig();
	void UpdateHandsRig(float DeltaSeconds);

	FNoclipHandsRigInput PendingRig;
	FVector HeadUpLocal = FVector::UpVector;
	FVector HeadRightLocal = FVector::RightVector;
	float SmoothWeightR = 0.f;
	float SmoothWeightL = 0.f;
	FVector SmoothTargetR = FVector::ZeroVector;
	FVector SmoothTargetL = FVector::ZeroVector;
	float SmoothHeadPitch = 0.f;
	float PatTime = 0.f;

	int32 CharacterIndex = -1;
	FVector LastLocation = FVector::ZeroVector;
	bool bHasLastLocation = false;
	bool bOverrideVelocity = false;
	FVector OverrideLocal = FVector::ZeroVector;

	FVector SmoothedLocal = FVector::ZeroVector;
	float Speed = 0.f;
	float Phase = 0.f;
	float IdleTime = 0.f;
	float AirTime = 0.f;
	float AirAlpha = 0.f;
	float JumpPoseTime = 0.f;

	/** Montado no game thread; o proxy copia no PreUpdate. */
	TArray<FNoclipAnimLayer, TInlineAllocator<8>> PendingLayers;
};
