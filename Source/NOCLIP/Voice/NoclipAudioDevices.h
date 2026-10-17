// Copyright Fuly Games. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "AudioMixerBlueprintLibrary.h"
#include "Containers/Ticker.h"
#include "NoclipAudioDevices.generated.h"

class IVoiceCapture;

/** Um dispositivo de audio da maquina (fone/caixa ou microfone). */
struct FNoclipAudioDevice
{
	/** Saida: id do Audio Mixer. Microfone: nome do dispositivo de captura (e o que o chat de voz usa). */
	FString Id;
	FString Name;
	bool bSystemDefault = false;
};

/**
 * Escolha do fone/caixa (saida do jogo inteiro) e do microfone (chat de voz), salvas nas opcoes do jogador.
 * Vazio = padrao do sistema. A saida troca na hora pelo Audio Mixer; o microfone troca o dispositivo de
 * captura do motor de voz assim que ele existe (o subsistema de voz chama ApplyInputToVoiceEngine).
 */
UCLASS()
class NOCLIP_API UNoclipAudioDeviceSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	static UNoclipAudioDeviceSubsystem* Get(const UObject* WorldContext);

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	/** Rele a lista de dispositivos (a de saida chega um pouco depois: OnDevicesChanged). */
	void RefreshDevices();

	const TArray<FNoclipAudioDevice>& GetOutputDevices() const { return OutputDevices; }
	const TArray<FNoclipAudioDevice>& GetInputDevices() const { return InputDevices; }

	/** Id vazio = padrao do sistema. Salva e aplica. */
	void SetOutputDevice(const FString& DeviceId);
	void SetInputDevice(const FString& DeviceName);

	/** Aplica o que esta salvo (depois de restaurar a secao, por exemplo). */
	void ApplySaved();

	/** Chamado pelo subsistema de voz (barato quando nada mudou). */
	void ApplyInputToVoiceEngine(UWorld* World);

	/** A lista de dispositivos mudou (a tela de opcoes se refaz). */
	FSimpleMulticastDelegate OnDevicesChanged;

private:
	/** Pede a lista de saidas ao Audio Mixer (precisa de um mundo com audio: no Initialize ainda nao tem). */
	void RequestOutputList();
	/** Tenta de novo a cada segundo ate a lista chegar (e o fone salvo ser aplicado). */
	bool RetryOutputList(float DeltaTime);
	FTSTicker::FDelegateHandle OutputRetryHandle;
	int32 OutputRetries = 0;

	UFUNCTION()
	void HandleOutputDevices(const TArray<FAudioOutputDeviceInfo>& Devices);

	UFUNCTION()
	void HandleDeviceSwap(const FSwapAudioOutputResult& Result);

	void ApplyOutput();
	void RefreshInputDevices();

	TArray<FNoclipAudioDevice> OutputDevices;
	TArray<FNoclipAudioDevice> InputDevices;
	FString CurrentOutputId;
	FString PendingOutputId;
	bool bHasOutputList = false;

	/** O que ja foi aplicado no motor de voz (o motor pode recriar a captura). */
	IVoiceCapture* AppliedCapture = nullptr;
	FString AppliedInput;
};
