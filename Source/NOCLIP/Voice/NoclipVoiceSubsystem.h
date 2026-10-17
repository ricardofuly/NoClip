// Copyright Fuly Games. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Voice/NoclipVoiceTalker.h"
#include "NoclipVoiceSubsystem.generated.h"

class APlayerState;
class APlayerController;
class ANoclipCharacter;
class UReverbEffect;
class FNoclipPushToTalkProcessor;

/**
 * Chat de voz do NOCLIP (lado de quem escuta e de quem fala; roda em cada maquina com jogador local).
 *
 * Transporte e codec sao os da Unreal (Opus pelo canal de voz da conexao, Steam no jogo final); aqui
 * entra o resto:
 *  - Qualidade: Opus a 24 kHz (config), complexidade 10 e 32 kbit/s VBR no codificador local.
 *  - Microfone: deteccao de voz (sensibilidade das configuracoes) ou apertar para falar (T / RB).
 *  - Espacial: a voz sai da cabeca do colega, some em ~20 m, abafa atras de parede e ecoa no corredor.
 *  - Celular: quem esta com o celular levantado entra na "ligacao do grupo" e ouve quem tambem esta,
 *    de qualquer distancia, com som de telefone e cortes conforme o sinal (Sala Manila = antena).
 *  - Sala de espera / saguao sem corpo: voz limpa sem posicao.
 */
UCLASS()
class NOCLIP_API UNoclipVoiceSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	static UNoclipVoiceSubsystem* Get(const UObject* WorldContext);

	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;

	/** Este colega esta falando agora (indicador na HUD)? */
	bool IsPlayerTalking(const APlayerState* PlayerState) const;
	/** O jogador local esta transmitindo (microfone aberto e acima do limiar)? */
	bool IsLocalPlayerTalking() const;
	/** Modo "apertar para falar" ligado nas configuracoes? */
	bool IsPushToTalk() const;
	/** Tecla de falar apertada agora (so faz sentido no modo apertar para falar). */
	bool IsPushToTalkHeld() const;
	/** Esse colega esta com o celular levantado (na ligacao do grupo)? */
	static bool IsOnPhoneLine(const APlayerState* PlayerState);
	/** Nomes de quem esta na ligacao junto com o jogador local (vazio se ele nao esta na linha). */
	void GetPhoneLineNames(TArray<FString>& OutNames) const;

	// ---------- Ajustes ----------
	/** Alem disso a voz de proximidade nao chega (cm). */
	static constexpr float ProximityMaxDistance = 2400.f;
	/** Perto assim, mesmo na ligacao, ouve-se a voz de verdade (cm). */
	static constexpr float PhoneMinDistance = 700.f;

private:
	APlayerController* GetLocalController() const;
	void UpdateTransmission(APlayerController* PC, double Now);
	void ApplyMicSettings();
	void ApplyEncoderQuality();
	void UpdateTalkers(APlayerController* PC, float DeltaSeconds);
	UNoclipVoiceTalker* FindOrCreateTalker(APlayerState* PlayerState);

	TSharedPtr<FNoclipPushToTalkProcessor> PushToTalk;

	UPROPERTY(Transient)
	TObjectPtr<UReverbEffect> BackroomsReverb;

	TMap<TWeakObjectPtr<const APlayerState>, TWeakObjectPtr<UNoclipVoiceTalker>> Talkers;

	bool bTransmitting = false;
	double NextTransmitAssertTime = 0.0;
	double QualityApplyTime = -1.0;
	int32 QualityApplyCount = 0;
	float AppliedMicThreshold = -1.f;
	float Accumulator = 0.f;
	double NextDeviceCheckTime = 0.0;
};
