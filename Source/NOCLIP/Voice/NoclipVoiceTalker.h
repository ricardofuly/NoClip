// Copyright Fuly Games. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Net/VoiceConfig.h"
#include "Sound/SoundAttenuation.h"
#include "NoclipVoiceTalker.generated.h"

class UAudioComponent;
class USceneComponent;

/** Como a voz de um colega chega ate quem escuta. */
enum class ENoclipVoiceRoute : uint8
{
	/** Sem corpo para sair a voz (sala de espera, saguao sem pawn): voz "de radio" sem posicao, limpa. */
	Flat,
	/** Proximidade: sai da cabeca do estagiario, some com a distancia, abafa atras de parede, ecoa no corredor. */
	Proximity,
	/** Ligacao pelo celular: sem posicao, faixa de telefone (300–3400 Hz), cortes conforme o sinal. */
	Phone,
	/** Longe demais e sem ligacao: nao se ouve. */
	Silent
};

/**
 * VOIPTalker de um colega (criado localmente em cada maquina, um por PlayerState remoto).
 * O motor de voz cria o componente de audio quando o colega comeca a falar; aqui ele e preso na cabeca do
 * personagem e reconfigurado ao vivo (proximidade <-> celular) sem esperar a proxima frase.
 */
UCLASS(NotBlueprintable)
class NOCLIP_API UNoclipVoiceTalker : public UVOIPTalker
{
	GENERATED_BODY()

public:
	UNoclipVoiceTalker(const FObjectInitializer& ObjectInitializer);

	/**
	 * Chamado pelo UNoclipVoiceSubsystem a cada atualizacao.
	 * @param AttachTo componente de onde a voz sai (mesh do personagem) e o osso da cabeca.
	 * @param Volume 0..1 (configuracao "Voz dos colegas").
	 * @param PhoneQuality 0..1 (sinal do celular: 1 = limpo, 0 = picotando).
	 */
	void UpdateRoute(ENoclipVoiceRoute InRoute, USceneComponent* AttachTo, FName Bone, float Volume, float PhoneQuality, float DeltaSeconds);

	ENoclipVoiceRoute GetRoute() const { return Route; }
	bool IsTalking() const { return AudioComponent.IsValid(); }

	/** Atenuacao da proximidade (compartilhada; criada pelo subsystem). */
	static FSoundAttenuationSettings MakeProximitySettings();
	static FSoundAttenuationSettings MakeFlatSettings();

	virtual void OnTalkingBegin(UAudioComponent* InAudioComponent) override;
	virtual void OnTalkingEnd() override;

private:
	void ApplyToComponent(bool bForce);

	TWeakObjectPtr<UAudioComponent> AudioComponent;
	TWeakObjectPtr<USceneComponent> AttachParent;
	FName AttachBone;

	ENoclipVoiceRoute Route = ENoclipVoiceRoute::Flat;
	ENoclipVoiceRoute AppliedRoute = ENoclipVoiceRoute::Silent;
	float TargetVolume = 1.f;
	float PhoneQuality = 1.f;
	float AppliedVolume = -1.f;
	double NextReapplyTime = 0.0;

	// Cortes do celular sem sinal.
	float DropTimer = 0.f;
	bool bDropped = false;
};
