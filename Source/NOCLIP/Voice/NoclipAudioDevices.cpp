// Copyright Fuly Games. All Rights Reserved.

#include "Voice/NoclipAudioDevices.h"
#include "Game/NoclipUserSettings.h"
#include "NOCLIP.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Interfaces/VoiceCapture.h"
#include "Net/VoiceConfig.h"
#include "OnlineSubsystemUtils.h"
#include "VoiceEngineImpl.h"
#include "VoiceInterfaceImpl.h"
#include "VoiceModule.h"

#if PLATFORM_WINDOWS
#include "Windows/AllowWindowsPlatformTypes.h"
#include <mmsystem.h>
#include <dsound.h>
#include "Windows/HideWindowsPlatformTypes.h"
#endif

#include UE_INLINE_GENERATED_CPP_BY_NAME(NoclipAudioDevices)

namespace
{
	/**
	 * O motor de voz da Unreal nao expoe a captura: os acessos abaixo usam ponteiro-para-membro por uma
	 * classe derivada (C++ permite nomear membros protegidos assim). FOnlineVoiceSteam e o Null derivam de
	 * FOnlineVoiceImpl, e FVoiceEngineSteam de FVoiceEngineImpl.
	 */
	struct FOnlineVoiceAccess : public FOnlineVoiceImpl
	{
		static IVoiceEnginePtr FOnlineVoiceImpl::* EngineMember() { return &FOnlineVoiceAccess::VoiceEngine; }
	};

	struct FVoiceEngineAccess : public FVoiceEngineImpl
	{
		using FGetCapture = TSharedPtr<IVoiceCapture>& (FVoiceEngineImpl::*)();
		static FGetCapture CaptureGetter() { return static_cast<FGetCapture>(&FVoiceEngineAccess::GetVoiceCapture); }
	};

#if PLATFORM_WINDOWS
	/** Mesma enumeracao (DirectSound) que o modulo de voz usa: os nomes batem com os que ele aceita. */
	BOOL CALLBACK EnumCaptureDevice(LPGUID Guid, LPCWSTR Description, LPCWSTR Module, LPVOID Context)
	{
		if (Guid && Description)
		{
			static_cast<TArray<FString>*>(Context)->Add(FString(Description));
		}
		return true;
	}
#endif
}

UNoclipAudioDeviceSubsystem* UNoclipAudioDeviceSubsystem::Get(const UObject* WorldContext)
{
	const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	const UGameInstance* GI = World ? World->GetGameInstance() : nullptr;
	return GI ? GI->GetSubsystem<UNoclipAudioDeviceSubsystem>() : nullptr;
}

void UNoclipAudioDeviceSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	RefreshDevices(); // a saida salva e aplicada quando a lista chega
	// No Initialize do GameInstance ainda nao existe mundo/audio device: o pedido da lista de saidas nao volta.
	// Tenta de novo ate chegar (sem isso, o fone salvo so era aplicado ao abrir as Opcoes).
	OutputRetryHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateUObject(this, &UNoclipAudioDeviceSubsystem::RetryOutputList), 1.f);
}

void UNoclipAudioDeviceSubsystem::Deinitialize()
{
	if (OutputRetryHandle.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(OutputRetryHandle);
		OutputRetryHandle.Reset();
	}
	Super::Deinitialize();
}

bool UNoclipAudioDeviceSubsystem::RetryOutputList(float DeltaTime)
{
	if (bHasOutputList || ++OutputRetries > 120)
	{
		OutputRetryHandle.Reset();
		return false;
	}
	const UGameInstance* GI = GetGameInstance();
	if (GI && GI->GetWorld() && GI->GetWorld()->GetAudioDeviceRaw())
	{
		RequestOutputList();
	}
	return true;
}

void UNoclipAudioDeviceSubsystem::RefreshDevices()
{
	RefreshInputDevices();
	RequestOutputList();
}

void UNoclipAudioDeviceSubsystem::RequestOutputList()
{
	FOnAudioOutputDevicesObtained Callback;
	Callback.BindUFunction(this, GET_FUNCTION_NAME_CHECKED(UNoclipAudioDeviceSubsystem, HandleOutputDevices));
	UAudioMixerBlueprintLibrary::GetAvailableAudioOutputDevices(GetGameInstance(), Callback);
}

void UNoclipAudioDeviceSubsystem::RefreshInputDevices()
{
	TArray<FString> Names;
#if PLATFORM_WINDOWS
	DirectSoundCaptureEnumerateW(&EnumCaptureDevice, &Names);
#endif
	TArray<FNoclipAudioDevice> NewList;
	// O "driver primario" do DirectSound vem sem GUID e ja fica de fora: ele e a opcao "Padrao do sistema".
	for (const FString& Name : Names)
	{
		FNoclipAudioDevice D;
		D.Id = Name;
		D.Name = Name;
		NewList.Add(D);
	}
	const bool bChanged = NewList.Num() != InputDevices.Num()
		|| NewList.ContainsByPredicate([this](const FNoclipAudioDevice& D) { return !InputDevices.ContainsByPredicate([&D](const FNoclipAudioDevice& O) { return O.Id == D.Id; }); });
	InputDevices = MoveTemp(NewList);
	if (bChanged)
	{
		OnDevicesChanged.Broadcast();
	}
}

void UNoclipAudioDeviceSubsystem::HandleOutputDevices(const TArray<FAudioOutputDeviceInfo>& Devices)
{
	TArray<FNoclipAudioDevice> NewList;
	for (const FAudioOutputDeviceInfo& Info : Devices)
	{
		FNoclipAudioDevice D;
		D.Id = Info.DeviceId;
		D.Name = Info.Name;
		D.bSystemDefault = Info.bIsSystemDefault;
		NewList.Add(D);
		if (Info.bIsCurrentDevice)
		{
			CurrentOutputId = Info.DeviceId;
		}
	}
	const bool bChanged = NewList.Num() != OutputDevices.Num()
		|| NewList.ContainsByPredicate([this](const FNoclipAudioDevice& D) { return !OutputDevices.ContainsByPredicate([&D](const FNoclipAudioDevice& O) { return O.Id == D.Id; }); });
	OutputDevices = MoveTemp(NewList);
	const bool bFirst = !bHasOutputList;
	bHasOutputList = true;
	if (bChanged || bFirst)
	{
		OnDevicesChanged.Broadcast();
	}
	if (bFirst)
	{
		ApplyOutput();
	}
}

void UNoclipAudioDeviceSubsystem::SetOutputDevice(const FString& DeviceId)
{
	FNoclipUserSettings& S = FNoclipUserSettings::Get();
	S.OutputDevice = DeviceId;
	S.Save();
	ApplyOutput();
}

void UNoclipAudioDeviceSubsystem::SetInputDevice(const FString& DeviceName)
{
	FNoclipUserSettings& S = FNoclipUserSettings::Get();
	S.InputDevice = DeviceName;
	S.Save();
	if (const UGameInstance* GI = GetGameInstance())
	{
		ApplyInputToVoiceEngine(GI->GetWorld());
	}
}

void UNoclipAudioDeviceSubsystem::ApplySaved()
{
	ApplyOutput();
	if (const UGameInstance* GI = GetGameInstance())
	{
		ApplyInputToVoiceEngine(GI->GetWorld());
	}
}

void UNoclipAudioDeviceSubsystem::ApplyOutput()
{
	if (!bHasOutputList || OutputDevices.Num() == 0)
	{
		return;
	}
	// Salvo e ainda conectado; senao o padrao do sistema (fone desconectado volta para a caixa).
	const FString& Saved = FNoclipUserSettings::Get().OutputDevice;
	const FNoclipAudioDevice* Wanted = Saved.IsEmpty() ? nullptr : OutputDevices.FindByPredicate([&Saved](const FNoclipAudioDevice& D) { return D.Id == Saved; });
	if (!Wanted)
	{
		Wanted = OutputDevices.FindByPredicate([](const FNoclipAudioDevice& D) { return D.bSystemDefault; });
	}
	if (!Wanted || Wanted->Id == CurrentOutputId || Wanted->Id == PendingOutputId)
	{
		return;
	}
	PendingOutputId = Wanted->Id;
	UE_LOG(LogNoclip, Log, TEXT("[Audio] Saida -> %s"), *Wanted->Name);
	FOnCompletedDeviceSwap Callback;
	Callback.BindUFunction(this, GET_FUNCTION_NAME_CHECKED(UNoclipAudioDeviceSubsystem, HandleDeviceSwap));
	UAudioMixerBlueprintLibrary::SwapAudioOutputDevice(GetGameInstance(), Wanted->Id, Callback);
}

void UNoclipAudioDeviceSubsystem::HandleDeviceSwap(const FSwapAudioOutputResult& Result)
{
	PendingOutputId.Reset();
	// CurrentDeviceId e o de antes da troca.
	CurrentOutputId = Result.Result == ESwapAudioOutputDeviceResultState::Success ? Result.RequestedDeviceId : Result.CurrentDeviceId;
	if (Result.Result != ESwapAudioOutputDeviceResultState::Success)
	{
		UE_LOG(LogNoclip, Warning, TEXT("[Audio] Nao deu para trocar a saida para %s."), *Result.RequestedDeviceId);
	}
}

void UNoclipAudioDeviceSubsystem::ApplyInputToVoiceEngine(UWorld* World)
{
	if (!World)
	{
		return;
	}
	IOnlineVoicePtr Voice = Online::GetVoiceInterface(World);
	if (!Voice.IsValid())
	{
		return;
	}
	FOnlineVoiceImpl* VoiceImpl = static_cast<FOnlineVoiceImpl*>(Voice.Get());
	IVoiceEnginePtr& Engine = VoiceImpl->*FOnlineVoiceAccess::EngineMember();
	if (!Engine.IsValid())
	{
		return;
	}
	FVoiceEngineImpl* EngineImpl = static_cast<FVoiceEngineImpl*>(Engine.Get());
	TSharedPtr<IVoiceCapture>& Capture = (EngineImpl->*FVoiceEngineAccess::CaptureGetter())();

	// Microfone salvo; se ele sumiu (desconectado), o padrao.
	FString Wanted = FNoclipUserSettings::Get().InputDevice;
	if (!Wanted.IsEmpty() && !InputDevices.ContainsByPredicate([&Wanted](const FNoclipAudioDevice& D) { return D.Id == Wanted; }))
	{
		Wanted.Reset();
	}
	if (Capture.IsValid() && Capture.Get() == AppliedCapture && Wanted == AppliedInput)
	{
		return;
	}

	if (!Capture.IsValid())
	{
		// O motor ainda nao abriu o microfone: abre o escolhido antes dele (ele usa o que ja existir).
		if (Wanted.IsEmpty())
		{
			return;
		}
		Capture = FVoiceModule::Get().CreateVoiceCapture(Wanted);
		if (!Capture.IsValid())
		{
			return;
		}
	}
	else if (Capture.Get() != AppliedCapture && Wanted.IsEmpty())
	{
		// Captura nova do proprio motor, ja no padrao: nada a trocar.
	}
	else
	{
		const bool bWasCapturing = Capture->IsCapturing();
		Capture->ChangeDevice(Wanted, UVOIPStatics::GetVoiceSampleRate(), UVOIPStatics::GetVoiceNumChannels());
		if (bWasCapturing)
		{
			Capture->Start();
		}
	}
	AppliedCapture = Capture.Get();
	AppliedInput = Wanted;
	UE_LOG(LogNoclip, Log, TEXT("[Audio] Microfone -> %s"), Wanted.IsEmpty() ? TEXT("padrao do sistema") : *Wanted);
}
