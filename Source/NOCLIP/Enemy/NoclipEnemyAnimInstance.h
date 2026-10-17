// Copyright Fuly Games. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "Enemy/NoclipEnemyTypes.h"
#include "NoclipEnemyAnimInstance.generated.h"

class UAnimSequence;

/** Camada de pose pronta para o worker de animacao. */
struct FNoclipEnemyAnimLayer
{
	const UAnimSequence* Sequence = nullptr;
	float Time = 0.f;
	float Weight = 0.f;
	bool bLoop = true;
};

/**
 * Animacao das entidades sem Anim Blueprint: parado / andar / correr pela velocidade (fase sincronizada) e,
 * por cima, a acao de uma vez so que o servidor manda (ataque, uivo/grito, atingido), com blend de entrada e
 * saida. Roda em todas as maquinas a partir do estado replicado.
 */
UCLASS(Transient, NotBlueprintable)
class NOCLIP_API UNoclipEnemyAnimInstance : public UAnimInstance
{
	GENERATED_BODY()

public:
	void SetEnemy(ENoclipEnemyKind InKind, int32 InVariant);

protected:
	virtual void NativeInitializeAnimation() override;
	virtual void NativeUpdateAnimation(float DeltaSeconds) override;
	virtual FAnimInstanceProxy* CreateAnimInstanceProxy() override;
	virtual void DestroyAnimInstanceProxy(FAnimInstanceProxy* InProxy) override;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UAnimSequence>> Anims;

private:
	friend struct FNoclipEnemyAnimInstanceProxy;

	ENoclipEnemyKind Kind = ENoclipEnemyKind::Hound;
	int32 Variant = INDEX_NONE;

	float Speed = 0.f;
	float Phase = 0.f;
	float IdleTime = 0.f;

	// Acao atual (copiada do ator) e a anterior saindo em blend.
	ENoclipEnemyAction Action = ENoclipEnemyAction::None;
	uint8 ActionSerial = 0;
	float ActionTime = 0.f;
	float ActionRate = 1.f;
	float ActionWeight = 0.f;

	TArray<FNoclipEnemyAnimLayer, TInlineAllocator<6>> PendingLayers;
};
