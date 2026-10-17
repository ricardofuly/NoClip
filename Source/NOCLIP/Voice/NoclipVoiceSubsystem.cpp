// Copyright Fuly Games. All Rights Reserved.

#include "Voice/NoclipVoiceSubsystem.h"
#include "Character/NoclipCharacter.h"
#include "Character/NoclipCharacterCatalog.h"
#include "Character/NoclipToolComponent.h"
#include "Character/NoclipToolTypes.h"
#include "Game/NoclipUserSettings.h"
#include "Game/NoclipPlayerState.h"
#include "UI/NoclipConsoleSubsystem.h"
#include "Voice/NoclipAudioDevices.h"
#include "NOCLIP.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Framework/Application/IInputProcessor.h"
#include "Framework/Application/SlateApplication.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "HAL/IConsoleManager.h"
#include "Interfaces/VoiceInterface.h"
#include "Kismet/GameplayStatics.h"
#include "Camera/PlayerCameraManager.h"
#include "Net/VoiceConfig.h"
#include "OnlineSubsystemUtils.h"
#include "Sound/ReverbEffect.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(NoclipVoiceSubsystem)

static TAutoConsoleVariable<int32> CVarNoclipVoiceBitrate(TEXT("noclip.VoiceBitrate"), 32000,
	TEXT("Bitrate do Opus da voz (bit/s). 24000..64000; 32000 ja e voz muito limpa a 24 kHz."));

static TAutoConsoleVariable<int32> CVarNoclipVoiceDebug(TEXT("noclip.VoiceDebug"), 0,
	TEXT("1 = mostra na tela a rota de cada voz (proximidade / celular / sem posicao) e quem esta falando."));

/**
 * Apertar para falar (T no teclado, RB/R1 no controle). Um pre-processador do Slate enxerga a tecla em
 * qualquer tela (jogo, sala de espera, pausa) sem consumir o evento; ignora quando um campo de texto
 * esta focado (digitando o codigo da sala, por exemplo).
 */
class FNoclipPushToTalkProcessor : public IInputProcessor
{
public:
	virtual void Tick(const float DeltaTime, FSlateApplication& SlateApp, TSharedRef<ICursor> Cursor) override {}

	virtual bool HandleKeyDownEvent(FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent) override
	{
		if (IsTalkKey(InKeyEvent.GetKey()) && !IsTyping(SlateApp))
		{
			bHeld = true;
		}
		return false;
	}

	virtual bool HandleKeyUpEvent(FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent) override
	{
		if (IsTalkKey(InKeyEvent.GetKey()))
		{
			bHeld = false;
		}
		return false;
	}

	static bool IsTalkKey(const FKey& Key)
	{
		return Key == EKeys::T || Key == EKeys::Gamepad_RightShoulder;
	}

	static bool IsTyping(FSlateApplication& SlateApp)
	{
		// Digitando no console: T e so a letra T.
		if (UNoclipConsoleSubsystem::IsConsoleOpen())
		{
			return true;
		}
		const TSharedPtr<SWidget> Focused = SlateApp.GetKeyboardFocusedWidget();
		if (!Focused.IsValid())
		{
			return false;
		}
		const FName Type = Focused->GetType();
		return Type == TEXT("SEditableText") || Type == TEXT("SMultiLineEditableText");
	}

	bool bHeld = false;
};

namespace
{
	/** Sensibilidade 0..100 -> limiar de deteccao de voz (envelope do microfone). */
	float MicThresholdFromSensitivity(int32 Sensitivity)
	{
		const float T = FMath::Pow(FMath::Clamp(Sensitivity / 100.f, 0.f, 1.f), 0.7f);
		return FMath::Lerp(0.15f, 0.005f, T);
	}

	float PhoneQualityFromBars(int32 Bars)
	{
		switch (Bars)
		{
		case 3:  return 1.f;
		case 2:  return 0.75f;
		case 1:  return 0.45f;
		case 0:  return 0.15f;
		default: return 0.8f; // sem localizacao (saguao): todo mundo perto da "antena" do elevador
		}
	}

	bool IsPhoneRaised(const ANoclipCharacter* C)
	{
		const UNoclipToolComponent* T = C ? C->GetToolComponent() : nullptr;
		return T && T->IsPhoneRaised();
	}

	const TCHAR* RouteName(ENoclipVoiceRoute R)
	{
		switch (R)
		{
		case ENoclipVoiceRoute::Proximity: return TEXT("proximidade");
		case ENoclipVoiceRoute::Phone:     return TEXT("celular");
		case ENoclipVoiceRoute::Flat:      return TEXT("sem posicao");
		default:                           return TEXT("longe (mudo)");
		}
	}
}

UNoclipVoiceSubsystem* UNoclipVoiceSubsystem::Get(const UObject* WorldContext)
{
	const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	return World ? World->GetSubsystem<UNoclipVoiceSubsystem>() : nullptr;
}

bool UNoclipVoiceSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	if (!Super::ShouldCreateSubsystem(Outer))
	{
		return false;
	}
	const UWorld* World = Cast<UWorld>(Outer);
	return World && (World->WorldType == EWorldType::Game || World->WorldType == EWorldType::PIE) && !IsRunningDedicatedServer();
}

void UNoclipVoiceSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	if (FSlateApplication::IsInitialized())
	{
		PushToTalk = MakeShared<FNoclipPushToTalkProcessor>();
		FSlateApplication::Get().RegisterInputPreProcessor(PushToTalk);
	}
}

void UNoclipVoiceSubsystem::Deinitialize()
{
	if (PushToTalk.IsValid() && FSlateApplication::IsInitialized())
	{
		FSlateApplication::Get().UnregisterInputPreProcessor(PushToTalk);
	}
	PushToTalk.Reset();
	Talkers.Reset();
	Super::Deinitialize();
}

void UNoclipVoiceSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);

	// Eco das Backrooms: carpete umido e teto baixo (a voz de proximidade manda um pouco para ca).
	BackroomsReverb = NewObject<UReverbEffect>(this, TEXT("NoclipBackroomsReverb"), RF_Transient);
	BackroomsReverb->bBypassEarlyReflections = false;
	BackroomsReverb->ReflectionsDelay = 0.012f;
	BackroomsReverb->ReflectionsGain = 0.28f;
	BackroomsReverb->GainHF = 0.72f;
	BackroomsReverb->bBypassLateReflections = false;
	BackroomsReverb->LateDelay = 0.02f;
	BackroomsReverb->DecayTime = 1.55f;
	BackroomsReverb->Density = 0.85f;
	BackroomsReverb->Diffusion = 0.82f;
	BackroomsReverb->AirAbsorptionGainHF = 0.99f;
	BackroomsReverb->DecayHFRatio = 0.6f;
	BackroomsReverb->LateGain = 1.1f;
	BackroomsReverb->Gain = 0.32f;
	UGameplayStatics::ActivateReverbEffect(&InWorld, BackroomsReverb, TEXT("NoclipBackrooms"), 0.f, 0.6f, 1.f);

	bTransmitting = false;
	QualityApplyCount = 0;
	QualityApplyTime = -1.0;
	AppliedMicThreshold = -1.f;
}

TStatId UNoclipVoiceSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UNoclipVoiceSubsystem, STATGROUP_Tickables);
}

APlayerController* UNoclipVoiceSubsystem::GetLocalController() const
{
	UWorld* World = GetWorld();
	APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
	return (PC && PC->IsLocalController()) ? PC : nullptr;
}

bool UNoclipVoiceSubsystem::IsPushToTalk() const
{
	return FNoclipUserSettings::Get().MicMode == 1;
}

bool UNoclipVoiceSubsystem::IsPushToTalkHeld() const
{
	return PushToTalk.IsValid() && PushToTalk->bHeld;
}

bool UNoclipVoiceSubsystem::IsLocalPlayerTalking() const
{
	if (!bTransmitting)
	{
		return false;
	}
	IOnlineVoicePtr Voice = Online::GetVoiceInterface(GetWorld());
	return Voice.IsValid() && Voice->IsLocalPlayerTalking(0);
}

bool UNoclipVoiceSubsystem::IsPlayerTalking(const APlayerState* PlayerState) const
{
	if (!PlayerState)
	{
		return false;
	}
	if (const APlayerController* PC = GetLocalController(); PC && PC->PlayerState == PlayerState)
	{
		return IsLocalPlayerTalking();
	}
	if (const TWeakObjectPtr<UNoclipVoiceTalker>* Found = Talkers.Find(PlayerState))
	{
		if (UNoclipVoiceTalker* Talker = Found->Get())
		{
			return Talker->IsTalking() && Talker->GetVoiceLevel() > 0.004f;
		}
	}
	return false;
}

bool UNoclipVoiceSubsystem::IsOnPhoneLine(const APlayerState* PlayerState)
{
	return PlayerState && IsPhoneRaised(Cast<ANoclipCharacter>(PlayerState->GetPawn()));
}

void UNoclipVoiceSubsystem::GetPhoneLineNames(TArray<FString>& OutNames) const
{
	OutNames.Reset();
	const APlayerController* PC = GetLocalController();
	const UWorld* World = GetWorld();
	const AGameStateBase* GS = World ? World->GetGameState() : nullptr;
	if (!PC || !GS || !IsOnPhoneLine(PC->PlayerState))
	{
		return;
	}
	for (const APlayerState* PS : GS->PlayerArray)
	{
		if (PS && PS != PC->PlayerState && IsOnPhoneLine(PS))
		{
			OutNames.Add(PS->GetPlayerName());
		}
	}
}

void UNoclipVoiceSubsystem::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	UWorld* World = GetWorld();
	APlayerController* PC = GetLocalController();
	if (!World || !PC || !World->HasBegunPlay())
	{
		return;
	}
	const double Now = World->GetTimeSeconds();
	UpdateTransmission(PC, Now);

	// Microfone escolhido nas opcoes: o motor de voz pode nascer (ou recriar a captura) a qualquer hora.
	if (Now >= NextDeviceCheckTime)
	{
		NextDeviceCheckTime = Now + 1.0;
		if (UNoclipAudioDeviceSubsystem* Devices = UNoclipAudioDeviceSubsystem::Get(World))
		{
			Devices->ApplyInputToVoiceEngine(World);
		}
	}

	// Rotas a ~30 Hz (os cortes do celular precisam de passo curto).
	Accumulator += DeltaTime;
	if (Accumulator >= 1.f / 30.f)
	{
		UpdateTalkers(PC, Accumulator);
		Accumulator = 0.f;
	}
}

void UNoclipVoiceSubsystem::UpdateTransmission(APlayerController* PC, double Now)
{
	UWorld* World = GetWorld();
	// Menu principal (sem rede): microfone fechado.
	const bool bNetworked = World->GetNetMode() != NM_Standalone;
	const bool bWant = bNetworked && (!IsPushToTalk() || IsPushToTalkHeld());

	if (bWant != bTransmitting)
	{
		bTransmitting = bWant;
		NextTransmitAssertTime = Now + 5.0;
		if (bWant)
		{
			PC->StartTalking();
			ApplyMicSettings();
			if (QualityApplyCount < 2)
			{
				QualityApplyTime = Now; // configura o codificador assim que ele existir
			}
		}
		else
		{
			PC->StopTalking();
		}
	}
	else if (bWant && !IsPushToTalk() && Now >= NextTransmitAssertTime)
	{
		// Deteccao de voz: reafirma de vez em quando (o motor reinicia a captura em troca de mapa / dispositivo).
		NextTransmitAssertTime = Now + 5.0;
		PC->StartTalking();
	}

	if (bTransmitting)
	{
		ApplyMicSettings();
	}
	// Duas passadas no codificador (agora e 2 s depois, caso a interface de voz ainda estivesse subindo).
	if (QualityApplyTime >= 0.0 && Now >= QualityApplyTime)
	{
		ApplyEncoderQuality();
		++QualityApplyCount;
		QualityApplyTime = QualityApplyCount < 2 ? Now + 2.0 : -1.0;
	}
}

void UNoclipVoiceSubsystem::ApplyMicSettings()
{
	const float Threshold = MicThresholdFromSensitivity(FNoclipUserSettings::Get().MicSensitivity);
	if (FMath::IsNearlyEqual(Threshold, AppliedMicThreshold, 0.0005f))
	{
		return;
	}
	AppliedMicThreshold = Threshold;
	// Limiar de "esta falando" e o portao de ruido logo abaixo (corta chiado entre as frases).
	UVOIPStatics::SetMicThreshold(Threshold);
	if (IConsoleVariable* Gate = IConsoleManager::Get().FindConsoleVariable(TEXT("voice.MicNoiseGateThreshold")))
	{
		Gate->Set(Threshold * 0.7f, ECVF_SetByGameSetting);
	}
	// Segura a voz aberta um pouco depois da ultima silaba (nao come o fim das palavras).
	if (IConsoleVariable* Release = IConsoleManager::Get().FindConsoleVariable(TEXT("voice.SilenceDetectionReleaseTime")))
	{
		Release->Set(1100.f, ECVF_SetByGameSetting);
	}
}

void UNoclipVoiceSubsystem::ApplyEncoderQuality()
{
	UWorld* World = GetWorld();
	if (!GEngine || !World || !Online::GetVoiceInterface(World).IsValid())
	{
		return;
	}
	// Opus: complexidade maxima (o padrao da Unreal e 1) e bitrate de voz limpa, com VBR.
	const int32 Bitrate = FMath::Clamp(CVarNoclipVoiceBitrate.GetValueOnGameThread(), 12000, 64000);
	GEngine->Exec(World, TEXT("vcvbr 1"));
	GEngine->Exec(World, TEXT("vccomplexity 10"));
	GEngine->Exec(World, *FString::Printf(TEXT("vcbitrate %d"), Bitrate));
	UE_LOG(LogNoclip, Log, TEXT("[Voz] Opus configurado: %d bit/s, complexidade 10, VBR."), Bitrate);
}

UNoclipVoiceTalker* UNoclipVoiceSubsystem::FindOrCreateTalker(APlayerState* PlayerState)
{
	if (const TWeakObjectPtr<UNoclipVoiceTalker>* Found = Talkers.Find(PlayerState))
	{
		if (UNoclipVoiceTalker* Existing = Found->Get())
		{
			return Existing;
		}
	}
	if (!PlayerState->GetUniqueId().IsValid())
	{
		return nullptr; // ainda sem id de rede; tenta de novo no proximo passo
	}
	UNoclipVoiceTalker* Talker = NewObject<UNoclipVoiceTalker>(PlayerState, NAME_None, RF_Transient);
	Talker->RegisterComponent();
	Talker->RegisterWithPlayerState(PlayerState);
	Talkers.Add(PlayerState, Talker);
	UE_LOG(LogNoclip, Log, TEXT("[Voz] Canal de voz de %s pronto."), *PlayerState->GetPlayerName());
	return Talker;
}

void UNoclipVoiceSubsystem::UpdateTalkers(APlayerController* PC, float DeltaSeconds)
{
	UWorld* World = GetWorld();
	const AGameStateBase* GS = World->GetGameState();
	if (!GS)
	{
		return;
	}
	for (auto It = Talkers.CreateIterator(); It; ++It)
	{
		if (!It.Key().IsValid() || !It.Value().IsValid())
		{
			It.RemoveCurrent();
		}
	}

	const FNoclipUserSettings& Settings = FNoclipUserSettings::Get();
	const float Volume = 1.2f * Settings.VoiceVolume / 90.f;

	ANoclipCharacter* Me = Cast<ANoclipCharacter>(PC->GetPawn());
	const FVector Ear = PC->PlayerCameraManager ? PC->PlayerCameraManager->GetCameraLocation() : (Me ? Me->GetEyeLocation() : FVector::ZeroVector);
	const bool bMeOnLine = IsPhoneRaised(Me);
	// Morto: escuta os vivos em volta da camera de espectador e os outros mortos sem posicao; os vivos nao escutam os mortos.
	const ANoclipPlayerState* MyNPS = Cast<ANoclipPlayerState>(PC->PlayerState);
	const bool bMeDead = MyNPS && MyNPS->IsDead();
	const int32 MyBars = Me ? NoclipTools::PhoneSignalBars(World, Me->GetActorLocation()) : -1;
	const bool bDebug = CVarNoclipVoiceDebug.GetValueOnGameThread() > 0;

	int32 DebugLine = 0;
	for (APlayerState* PS : GS->PlayerArray)
	{
		if (!PS || PS == PC->PlayerState || PS->IsInactive())
		{
			continue;
		}
		UNoclipVoiceTalker* Talker = FindOrCreateTalker(PS);
		if (!Talker)
		{
			continue;
		}

		ANoclipCharacter* Speaker = Cast<ANoclipCharacter>(PS->GetPawn());
		ENoclipVoiceRoute Route = ENoclipVoiceRoute::Flat;
		float Quality = 1.f;
		USceneComponent* AttachTo = nullptr;
		FName Bone = NAME_None;

		const ANoclipPlayerState* SpeakerNPS = Cast<ANoclipPlayerState>(PS);
		const bool bSpeakerDead = SpeakerNPS && SpeakerNPS->IsDead();
		if (bSpeakerDead)
		{
			Route = bMeDead ? ENoclipVoiceRoute::Flat : ENoclipVoiceRoute::Silent;
		}
		else if (Speaker && (Me || bMeDead))
		{
			const float Dist = FVector::Dist(Speaker->GetEyeLocation(), Ear);
			const bool bLine = bMeOnLine && IsPhoneRaised(Speaker);
			if (bLine && Dist > PhoneMinDistance)
			{
				Route = ENoclipVoiceRoute::Phone;
				const int32 Bars = FMath::Min(MyBars, NoclipTools::PhoneSignalBars(World, Speaker->GetActorLocation()));
				Quality = PhoneQualityFromBars(MyBars < 0 ? -1 : Bars);
			}
			else if (Dist <= ProximityMaxDistance)
			{
				Route = ENoclipVoiceRoute::Proximity;
				AttachTo = Speaker->GetCharacterMesh();
				Bone = NoclipCharacters::Get(Speaker->GetAppliedCharacter()).Head;
			}
			else
			{
				Route = ENoclipVoiceRoute::Silent;
			}
		}

		Talker->UpdateRoute(Route, AttachTo, Bone, Volume, Quality, DeltaSeconds);

		if (bDebug && GEngine)
		{
			GEngine->AddOnScreenDebugMessage(static_cast<uint64>(0x5701CE00 + DebugLine++), 0.2f, IsPlayerTalking(PS) ? FColor::Yellow : FColor::Silver,
				FString::Printf(TEXT("[Voz] %s: %s%s nivel %.3f"), *PS->GetPlayerName(), RouteName(Route),
					Route == ENoclipVoiceRoute::Phone ? *FString::Printf(TEXT(" (sinal %.0f%%)"), Quality * 100.f) : TEXT(""), Talker->GetVoiceLevel()));
		}
	}

	if (bDebug && GEngine)
	{
		GEngine->AddOnScreenDebugMessage(static_cast<uint64>(0x5701CEFF), 0.2f, FColor::Cyan,
			FString::Printf(TEXT("[Voz] voce: %s, %s, limiar %.3f%s"), bTransmitting ? TEXT("microfone aberto") : TEXT("microfone fechado"),
				IsPushToTalk() ? TEXT("apertar para falar") : TEXT("deteccao de voz"), AppliedMicThreshold,
				IsLocalPlayerTalking() ? TEXT(" — FALANDO") : TEXT("")));
	}
}
