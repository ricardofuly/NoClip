// Copyright Fuly Games. All Rights Reserved.

#include "Audio/NoclipAudioSubsystem.h"
#include "Character/NoclipCharacter.h"
#include "Enemy/NoclipEnemy.h"
#include "Game/NoclipGameState.h"
#include "Game/NoclipPlayerController.h"
#include "Game/NoclipPlayerState.h"
#include "Game/NoclipUserSettings.h"
#include "World/NoclipCorpse.h"
#include "World/NoclipElevator.h"
#include "World/NoclipExtractionZone.h"
#include "World/NoclipLevelGenerator.h"
#include "World/NoclipLootActor.h"
#include "World/NoclipWaitingRoom.h"
#include "World/NoclipPowerGenerator.h"
#include "NOCLIP.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/AudioComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "HAL/IConsoleManager.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInterface.h"
#include "Sound/SoundAttenuation.h"
#include "Sound/SoundBase.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(NoclipAudioSubsystem)

static TAutoConsoleVariable<int32> CVarNoclipAudioDebug(TEXT("noclip.AudioDebug"), 0,
	TEXT("Mostra na tela o estado da musica, a faixa e o ambiente (1)."));

namespace NoclipSounds
{
	FSoftObjectPath Path(const TCHAR* RelativePath)
	{
		const FString Rel(RelativePath);
		FString Name;
		Rel.Split(TEXT("/"), nullptr, &Name, ESearchCase::IgnoreCase, ESearchDir::FromEnd);
		if (Name.IsEmpty())
		{
			Name = Rel;
		}
		return FSoftObjectPath(FString::Printf(TEXT("/Game/NOCLIP/Sounds/%s.%s"), *Rel, *Name));
	}

	struct FMusicDef
	{
		TArray<const TCHAR*> Tracks;
		float Volume = 0.5f;
		float FadeIn = 3.f;
		/** Quanto a faixa anterior leva para sair quando este estado entra. */
		float FadeOutOld = 3.f;
	};

	FMusicDef MakeDef(TArray<const TCHAR*> Tracks, float Volume, float FadeIn, float FadeOutOld)
	{
		FMusicDef D;
		D.Tracks = MoveTemp(Tracks);
		D.Volume = Volume;
		D.FadeIn = FadeIn;
		D.FadeOutOld = FadeOutOld;
		return D;
	}

	const FMusicDef& Music(ENoclipMusicState State)
	{
		static const FMusicDef Menu = MakeDef({ TEXT("Main_Menu_Tracks/Main_Menu_Track_1_Master"), TEXT("Main_Menu_Tracks/Main_Menu_Track_2_Master"), TEXT("Main_Menu_Tracks/Main_Menu_Track_3_Master"),
			TEXT("Main_Menu_Tracks/Main_Menu_Track_4_master"), TEXT("Main_Menu_Tracks/Main_Menu_Track_5"), TEXT("Main_Menu_Tracks/Main_Menu_Track_6") }, 0.55f, 3.f, 3.f);
		static const FMusicDef Waiting = MakeDef({ TEXT("Safe_Room_Tracks/Safe_Room_Track_2_Master"), TEXT("Safe_Room_Tracks/Safe_Room_Track_3") }, 0.38f, 3.5f, 3.5f);
		static const FMusicDef Descending = MakeDef({ TEXT("Main_Menu_Tracks/Tense_Music_Track_1") }, 0.5f, 3.f, 3.f);
		static const FMusicDef Lobby = MakeDef({ TEXT("Safe_Room_Tracks/Safe_Room_Track_3"), TEXT("Safe_Room_Tracks/Safe_Room_Track_2_Master") }, 0.36f, 3.5f, 3.5f);
		static const FMusicDef Explore = MakeDef({ TEXT("Ambience_Tracks/Creepy_Ambience_1_master"), TEXT("Ambience_Tracks/Creepy_Ambience_2_Master"), TEXT("Ambience_Tracks/Creepy_Ambience_3_Master"),
			TEXT("Ambience_Tracks/Creepy_Ambience_4_Track"), TEXT("Ambience_Tracks/Creepy_Ambience_5_track") }, 0.42f, 4.f, 4.f);
		static const FMusicDef Tension = MakeDef({ TEXT("Ambience_Tracks/Tense_Ambience_1_Master"), TEXT("Ambience_Tracks/Tense_Ambience_2"), TEXT("Ambience_Tracks/Tense_Ambience_3_Track_"),
			TEXT("Ambience_Tracks/Tense_Ambience_4") }, 0.52f, 2.5f, 3.f);
		static const FMusicDef Chase = MakeDef({ TEXT("Chase_Tracks/Chase_Track_1_Master"), TEXT("Chase_Tracks/Chase_Track_2_Master"), TEXT("Chase_Tracks/Chase_Track_3_Master") }, 0.72f, 0.8f, 1.2f);
		static const FMusicDef Collapse = MakeDef({ TEXT("Combat_Tracks/Combat_Music_Track_Master"), TEXT("Combat_Tracks/Combat_Track_2_master"), TEXT("Combat_Tracks/Combat_Track_3_master") }, 0.62f, 2.f, 2.5f);
		static const FMusicDef Safe = MakeDef({ TEXT("Safe_Room_Tracks/Safe_Room-_Safe_Point_Track_Master") }, 0.42f, 3.f, 3.f);
		static const FMusicDef Dead = MakeDef({ TEXT("Other_Tracks/Psychological_-Unsettling_Track_Master"), TEXT("Other_Tracks/Psychological_2-Unsettling_Track_Master") }, 0.5f, 2.5f, 2.f);
		static const FMusicDef Good = MakeDef({ TEXT("Other_Tracks/Main_Menu_-Credits_Track") }, 0.5f, 2.5f, 2.5f);
		static const FMusicDef Fired = MakeDef({ TEXT("Credits_Tracks/Credits_Track_1_Master"), TEXT("Credits_Tracks/Credits_Track_2_Master") }, 0.55f, 2.5f, 2.5f);
		static const FMusicDef Empty;
		switch (State)
		{
		case ENoclipMusicState::Menu:        return Menu;
		case ENoclipMusicState::WaitingRoom: return Waiting;
		case ENoclipMusicState::Descending:  return Descending;
		case ENoclipMusicState::Lobby:       return Lobby;
		case ENoclipMusicState::Explore:     return Explore;
		case ENoclipMusicState::Tension:     return Tension;
		case ENoclipMusicState::Chase:       return Chase;
		case ENoclipMusicState::Collapse:    return Collapse;
		case ENoclipMusicState::SafeRoom:    return Safe;
		case ENoclipMusicState::Dead:        return Dead;
		case ENoclipMusicState::ReportGood:  return Good;
		case ENoclipMusicState::ReportFired: return Fired;
		default:                             return Empty;
		}
	}

	const TArray<const TCHAR*>& StepPaths(ENoclipSurface Surface)
	{
		static const TArray<const TCHAR*> Concrete = { TEXT("Footstep/Concrete/Footstep_Concrete_1"), TEXT("Footstep/Concrete/Footstep_Concrete_2"), TEXT("Footstep/Concrete/Footstep_Concrete_3"), TEXT("Footstep/Concrete/Footstep_Concrete_4"), TEXT("Footstep/Concrete/Footstep_Concrete_5") };
		static const TArray<const TCHAR*> Metal = { TEXT("Footstep/Metal/Foodstep_Metal_1"), TEXT("Footstep/Metal/Foodstep_Metal_2"), TEXT("Footstep/Metal/Foodstep_Metal_3"), TEXT("Footstep/Metal/Foodstep_Metal_4"), TEXT("Footstep/Metal/Foodstep_Metal_5") };
		static const TArray<const TCHAR*> Wood = { TEXT("Footstep/Wood/Footstep_Wood_1"), TEXT("Footstep/Wood/Footstep_Wood_2"), TEXT("Footstep/Wood/Footstep_Wood_3"), TEXT("Footstep/Wood/Footstep_Wood_4"), TEXT("Footstep/Wood/Footstep_Wood_5") };
		static const TArray<const TCHAR*> Stone = { TEXT("Footstep/Stone/Footstep_Stone_1"), TEXT("Footstep/Stone/Footstep_Stone_2"), TEXT("Footstep/Stone/Footstep_Stone_3"), TEXT("Footstep/Stone/Footstep_Stone_4") };
		static const TArray<const TCHAR*> Stairs = { TEXT("Footstep/Stairs/Footstep_Stairs_1"), TEXT("Footstep/Stairs/Footstep_Stairs_2"), TEXT("Footstep/Stairs/Footstep_Stairs_3"), TEXT("Footstep/Stairs/Footstep_Stairs_4"), TEXT("Footstep/Stairs/Footstep_Stairs_5") };
		static const TArray<const TCHAR*> Dirt = { TEXT("Footstep/Dirt/Footstep_Dirt_1"), TEXT("Footstep/Dirt/Footstep_Dirt_2"), TEXT("Footstep/Dirt/Footstep_Dirt_3"), TEXT("Footstep/Dirt/Footstep_Dirt_4"), TEXT("Footstep/Dirt/Footstep_Dirt_5") };
		static const TArray<const TCHAR*> Grass = { TEXT("Footstep/Grass/Footstep_Grass_1"), TEXT("Footstep/Grass/Footstep_Grass_2"), TEXT("Footstep/Grass/Footstep_Grass_3"), TEXT("Footstep/Grass/Footstep_Grass_4"), TEXT("Footstep/Grass/Footstep_Grass_5"), TEXT("Footstep/Grass/Footstep_Grass_6") };
		static const TArray<const TCHAR*> Gravel = { TEXT("Footstep/Gravel/Footstep_Gravel_1"), TEXT("Footstep/Gravel/Footstep_Gravel_2"), TEXT("Footstep/Gravel/Footstep_Gravel_3"), TEXT("Footstep/Gravel/Footstep_Gravel_4"), TEXT("Footstep/Gravel/Footstep_Gravel_5") };
		switch (Surface)
		{
		case ENoclipSurface::Metal:  return Metal;
		case ENoclipSurface::Wood:   return Wood;
		case ENoclipSurface::Stone:  return Stone;
		case ENoclipSurface::Stairs: return Stairs;
		case ENoclipSurface::Dirt:   return Dirt;
		case ENoclipSurface::Grass:  return Grass;
		case ENoclipSurface::Gravel: return Gravel;
		default:                     return Concrete; // carpete usa concreto abafado
		}
	}

	// Sons distantes nos corredores (curtos o bastante para nao virar trilha).
	const TArray<const TCHAR*> Distant = {
		TEXT("SFX/Creepy_Sound_Sfx_2"), TEXT("SFX/Creepy_Sound_Sfx_3"), TEXT("SFX/Creepy_Sound_Sfx_4"),
		TEXT("SFX/Mysterious_Sound_Sfx"), TEXT("SFX/Mysterious_Sound_Sfx_2"), TEXT("SFX/Mysterious_Sound_Sfx_3"), TEXT("SFX/Mysterious_Sound_Sfx_4"),
		TEXT("SFX/Mysterious_Sound_Sfx_5"), TEXT("SFX/Mysterious_Sound_Sfx_6"), TEXT("SFX/Mysterious_Sound_Sfx_7"), TEXT("SFX/Creepy_Sound_Sfx")
	};
	const TArray<const TCHAR*> DeathOthers = { TEXT("SFX/Death_Sfx_2"), TEXT("SFX/Death_Sfx_3") };

	const TCHAR* StateName(ENoclipMusicState S)
	{
		switch (S)
		{
		case ENoclipMusicState::Menu: return TEXT("Menu");
		case ENoclipMusicState::WaitingRoom: return TEXT("Sala de espera");
		case ENoclipMusicState::Descending: return TEXT("Elevador");
		case ENoclipMusicState::Lobby: return TEXT("Saguao");
		case ENoclipMusicState::Explore: return TEXT("Exploracao");
		case ENoclipMusicState::Tension: return TEXT("Tensao");
		case ENoclipMusicState::Chase: return TEXT("Perseguicao");
		case ENoclipMusicState::Collapse: return TEXT("Colapso");
		case ENoclipMusicState::SafeRoom: return TEXT("Sala Manila");
		case ENoclipMusicState::Dead: return TEXT("Morto");
		case ENoclipMusicState::ReportGood: return TEXT("Relatorio (quota)");
		case ENoclipMusicState::ReportFired: return TEXT("Relatorio (demitidos)");
		default: return TEXT("-");
		}
	}

	/**
	 * Todos os sons com caminho fixo (musica de cada estado, passos, sons distantes, avisos e ambiente).
	 * Usado pelo teste NOCLIP.Unit.Audio.SoundAssets: um som renomeado/apagado na pasta Sounds aparece la.
	 * Mantenha em dia com os Sfx(TEXT(...)) deste arquivo.
	 */
	void CollectStaticPaths(TArray<FSoftObjectPath>& Out)
	{
		for (int32 S = static_cast<int32>(ENoclipMusicState::Menu); S <= static_cast<int32>(ENoclipMusicState::ReportFired); ++S)
		{
			for (const TCHAR* Track : Music(static_cast<ENoclipMusicState>(S)).Tracks)
			{
				Out.AddUnique(Path(Track));
			}
		}
		for (int32 i = 0; i < static_cast<int32>(ENoclipSurface::Count); ++i)
		{
			for (const TCHAR* Step : StepPaths(static_cast<ENoclipSurface>(i)))
			{
				Out.AddUnique(Path(Step));
			}
		}
		for (const TCHAR* P : Distant) { Out.AddUnique(Path(P)); }
		for (const TCHAR* P : DeathOthers) { Out.AddUnique(Path(P)); }
		static const TCHAR* Fixed[] = {
			TEXT("SFX/White_Noise_Sfx_"), TEXT("SFX/Wind_Sfx"), TEXT("SFX/Creepy_Sound_Sfx_3"), TEXT("SFX/Creepy_Sound_Sfx_4"),
			TEXT("SFX/Dark_Riser"), TEXT("SFX/Death_Sfx"), TEXT("SFX/Deep_breath"), TEXT("SFX/Jumpscare_Sfx"), TEXT("SFX/Jumpscare_Sfx_2"),
			TEXT("SFX/Mysterious_Sound_Sfx_5"), TEXT("SFX/New_Mision_Sfx")
		};
		for (const TCHAR* P : Fixed) { Out.AddUnique(Path(P)); }
	}

	void ConfigureFlat(UAudioComponent* C)
	{
		// Sem posicao e sem o reverb dos corredores (a musica nao esta "na sala").
		C->bAllowSpatialization = false;
		C->bOverrideAttenuation = true;
		FSoundAttenuationSettings& S = C->AttenuationOverrides;
		S.bAttenuate = false;
		S.bSpatialize = false;
		S.bAttenuateWithLPF = false;
		S.bEnableOcclusion = false;
		S.bEnableReverbSend = false;
		S.bEnableListenerFocus = false;
		S.bEnablePriorityAttenuation = false;
	}
}

UNoclipAudioSubsystem* UNoclipAudioSubsystem::Get(const UObject* WorldContext)
{
	const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	const UGameInstance* GI = World ? World->GetGameInstance() : nullptr;
	return GI ? GI->GetSubsystem<UNoclipAudioSubsystem>() : nullptr;
}

bool UNoclipAudioSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	return !IsRunningDedicatedServer() && Super::ShouldCreateSubsystem(Outer);
}

USoundAttenuation* UNoclipAudioSubsystem::MakeAttenuation(float Inner, float Falloff, float LPFAtMax, bool bOcclusion, float ReverbMax)
{
	USoundAttenuation* A = NewObject<USoundAttenuation>(this);
	FSoundAttenuationSettings& S = A->Attenuation;
	S.bAttenuate = true;
	S.DistanceAlgorithm = EAttenuationDistanceModel::NaturalSound;
	S.AttenuationShape = EAttenuationShape::Sphere;
	S.AttenuationShapeExtents = FVector(Inner, 0.f, 0.f);
	S.FalloffDistance = Falloff;
	S.dBAttenuationAtMax = -60.f;
	S.bSpatialize = true;
	S.SpatializationAlgorithm = SPATIALIZATION_Default;
	S.NonSpatializedRadiusStart = 120.f;
	S.NonSpatializedRadiusEnd = 40.f;
	S.StereoSpread = 0.f;
	S.bAttenuateWithLPF = true;
	S.LPFRadiusMin = Inner;
	S.LPFRadiusMax = Inner + Falloff;
	S.LPFFrequencyAtMin = 20000.f;
	S.LPFFrequencyAtMax = LPFAtMax;
	S.bEnableOcclusion = bOcclusion;
	S.OcclusionTraceChannel = ECC_Visibility;
	S.OcclusionLowPassFilterFrequency = 900.f;
	S.OcclusionVolumeAttenuation = 0.45f;
	S.OcclusionInterpolationTime = 0.2f;
	S.bUseComplexCollisionForOcclusion = false;
	S.bEnableReverbSend = true;
	S.ReverbWetLevelMin = 0.1f;
	S.ReverbWetLevelMax = ReverbMax;
	S.ReverbDistanceMin = Inner;
	S.ReverbDistanceMax = Inner + Falloff;
	S.bEnableListenerFocus = false;
	S.bEnablePriorityAttenuation = false;
	return A;
}

void UNoclipAudioSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	FootstepAttenuation = MakeAttenuation(150.f, 2000.f, 2500.f, true, 0.35f);
	EnemyStepAttenuation = MakeAttenuation(200.f, 2800.f, 1800.f, true, 0.5f);
	DistantAttenuation = MakeAttenuation(400.f, 4200.f, 1200.f, false, 0.85f);
	EventAttenuation = MakeAttenuation(300.f, 3200.f, 2000.f, true, 0.6f);

	// Passos e efeitos sao pequenos: carregam ja (sem engasgo na primeira vez).
	Steps.SetNum(static_cast<int32>(ENoclipSurface::Count));
	for (int32 i = 0; i < static_cast<int32>(ENoclipSurface::Count); ++i)
	{
		for (const TCHAR* P : NoclipSounds::StepPaths(static_cast<ENoclipSurface>(i)))
		{
			if (USoundBase* S = Cast<USoundBase>(NoclipSounds::Path(P).TryLoad()))
			{
				Steps[i].Sounds.Add(S);
			}
		}
		LastStep[i] = -1;
	}

	Music.bPersistent = true;
	Bed.bPersistent = false;
	Bed.CrossFade = 2.5f;
	bReady = true;
}

void UNoclipAudioSubsystem::Deinitialize()
{
	bReady = false;
	for (FNoclipAudioLayer* L : { &Music, &Bed })
	{
		if (IsValid(L->Current))
		{
			L->Current->Stop();
			L->Current->DestroyComponent();
		}
		for (UAudioComponent* C : L->Fading)
		{
			if (IsValid(C))
			{
				C->Stop();
				C->DestroyComponent();
			}
		}
		L->Current = nullptr;
		L->Fading.Reset();
	}
	Super::Deinitialize();
}

TStatId UNoclipAudioSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UNoclipAudioSubsystem, STATGROUP_Tickables);
}

UWorld* UNoclipAudioSubsystem::GetTickableGameObjectWorld() const
{
	const UGameInstance* GI = GetGameInstance();
	return GI ? GI->GetWorld() : nullptr;
}

APlayerController* UNoclipAudioSubsystem::GetLocalController(UWorld* World) const
{
	const UGameInstance* GI = GetGameInstance();
	return (GI && World) ? GI->GetFirstLocalPlayerController(World) : nullptr;
}

float UNoclipAudioSubsystem::GetMusicScale() const
{
	return FMath::Clamp(FNoclipUserSettings::Get().MusicVolume / 100.f, 0.f, 1.f);
}

// ---------------------------------------------------------------------------------------------------------
// Tick
// ---------------------------------------------------------------------------------------------------------

void UNoclipAudioSubsystem::Tick(float DeltaTime)
{
	UWorld* World = GetTickableGameObjectWorld();
	if (!World || World->GetNetMode() == NM_DedicatedServer)
	{
		return;
	}

	// Mapa novo: o ambiente era do mundo antigo (ja foi junto com ele). A musica continua.
	if (LastWorld.Get() != World)
	{
		LastWorld = World;
		Bed.Current = nullptr;
		Bed.Fading.Reset();
		Bed.CurrentPath.Reset();
		Bed.PendingPath.Reset();
		BedState = EBed::None;
		SeenCorpses.Reset();
		EnemyStepPhase.Reset();
		HissComps.Reset();
		HissGlitch.Reset();
		bPrevPowerOut = false;
		PrevPhase = 255;
		NextDistantTime = 0.0;
	}

	const double Now = FPlatformTime::Seconds();
	if (Now >= NextEvaluateTime)
	{
		NextEvaluateTime = Now + 0.25;
		const ENoclipMusicState Wanted = EvaluateMusicState(World, Now);
		if (Wanted != ENoclipMusicState::None && Wanted != MusicState)
		{
			ApplyMusicState(Wanted);
		}
		const EBed WantedBed = EvaluateBed(World);
		if (WantedBed != BedState)
		{
			ApplyBed(WantedBed);
		}
	}

	// Volume da musica nas opcoes: segue na hora.
	const float Scale = GetMusicScale();
	if (!FMath::IsNearlyEqual(Scale, AppliedMusicScale))
	{
		AppliedMusicScale = Scale;
		if (IsValid(Music.Current))
		{
			Music.Current->AdjustVolume(0.5f, Music.Volume * Scale);
		}
	}

	TickLayer(0, DeltaTime);
	// O ambiente e do mundo: pausado, ele para junto (o relogio dele tambem).
	TickLayer(1, World->IsPaused() ? 0.f : DeltaTime);

	if (!World->IsPaused())
	{
		TickEvents(World, DeltaTime, Now);
		TickEnemySteps(World, DeltaTime);
		TickDistantSounds(World, Now);
		if (Now >= NextHissCheck)
		{
			NextHissCheck = Now + 0.25;
			TickGlitchHiss(World);
		}
	}

	if (CVarNoclipAudioDebug.GetValueOnGameThread() > 0 && GEngine)
	{
		GEngine->AddOnScreenDebugMessage(static_cast<uint64>(0xA0D10001), 0.f, FColor::Cyan,
			FString::Printf(TEXT("[Audio] musica: %s · %s (%.0f/%.0f s)  ambiente: %d"), NoclipSounds::StateName(MusicState),
				*Music.CurrentPath.GetAssetName(), Music.Elapsed, Music.Duration, static_cast<int32>(BedState)));
	}
}

ENoclipMusicState UNoclipAudioSubsystem::EvaluateMusicState(UWorld* World, double Now)
{
	const ANoclipPlayerController* PC = Cast<ANoclipPlayerController>(GetLocalController(World));
	if (!PC)
	{
		return ENoclipMusicState::None; // carregando: mantem o que esta tocando
	}
	if (PC->IsFrontend())
	{
		return ENoclipMusicState::Menu;
	}
	if (PC->IsWaitingRoom())
	{
		for (TActorIterator<ANoclipWaitingRoom> It(World); It; ++It)
		{
			if (It->GetPhase() == ENoclipWaitingPhase::Departing)
			{
				return ENoclipMusicState::Descending;
			}
		}
		return ENoclipMusicState::WaitingRoom;
	}

	const ANoclipGameState* GS = World->GetGameState<ANoclipGameState>();
	if (!GS)
	{
		return ENoclipMusicState::None;
	}
	switch (GS->GetPhase())
	{
	case ENoclipShiftPhase::Lobby:
		return GS->IsArrivalPending() ? ENoclipMusicState::Descending : ENoclipMusicState::Lobby;
	case ENoclipShiftPhase::Preparing:
		return ENoclipMusicState::Descending;
	case ENoclipShiftPhase::Ended:
		return GS->GetLastResult().bFired ? ENoclipMusicState::ReportFired : ENoclipMusicState::ReportGood;
	default:
		break;
	}

	// Turno.
	if (PC->IsSpectating())
	{
		return ENoclipMusicState::Dead;
	}
	const FVector Ear = PC->PlayerCameraManager ? PC->PlayerCameraManager->GetCameraLocation() : FVector::ZeroVector;
	bool bChase = false, bTension = false;
	for (TActorIterator<ANoclipEnemy> It(World); It; ++It)
	{
		const float Dist = FVector::Dist(It->GetActorLocation(), Ear);
		switch (It->GetState())
		{
		case ENoclipEnemyState::Chase:
		case ENoclipEnemyState::Attack:
		case ENoclipEnemyState::Alert:
			bChase |= Dist < 2200.f;
			bTension |= Dist < 4500.f;
			break;
		case ENoclipEnemyState::Investigate:
			bTension |= Dist < 2000.f;
			break;
		default:
			bTension |= Dist < 900.f; // algo andando muito perto
			break;
		}
	}
	// Histerese: a perseguicao segura alguns segundos depois de despistar (sem vai-e-volta).
	if (bChase) { ChaseUntil = Now + 6.0; }
	if (bTension) { TensionUntil = Now + 10.0; }
	if (Now < ChaseUntil)
	{
		return ENoclipMusicState::Chase;
	}
	if (GS->GetPhase() == ENoclipShiftPhase::Collapse)
	{
		return ENoclipMusicState::Collapse;
	}
	const APawn* Pawn = PC->GetPawn();
	ANoclipExtractionZone* Zone = nullptr;
	for (TActorIterator<ANoclipExtractionZone> It(World); It; ++It)
	{
		Zone = *It;
		break;
	}
	if (Pawn && Zone && Zone->IsInside(Pawn->GetActorLocation()))
	{
		SafeUntil = Now + 3.0;
	}
	if (Now < SafeUntil)
	{
		return ENoclipMusicState::SafeRoom;
	}
	// Apagao: nada de musica tranquila no escuro.
	return (Now < TensionUntil || GS->IsPowerOut()) ? ENoclipMusicState::Tension : ENoclipMusicState::Explore;
}

void UNoclipAudioSubsystem::ApplyMusicState(ENoclipMusicState NewState)
{
	const ENoclipMusicState Old = MusicState;
	MusicState = NewState;
	const NoclipSounds::FMusicDef& Def = NoclipSounds::Music(NewState);

	// Saindo da exploracao: guarda o ponto para voltar dali.
	if (Old == ENoclipMusicState::Explore && Music.CurrentPath.IsValid())
	{
		ExploreResumePath = Music.CurrentPath;
		ExploreResumeAt = Music.Elapsed;
	}

	// Saindo da perseguicao: a adrenalina baixa devagar.
	float FadeOutOld = Def.FadeOutOld;
	if (Old == ENoclipMusicState::Chase)
	{
		FadeOutOld = 4.f;
	}

	FSoftObjectPath Path;
	float StartAt = 0.f;
	if (NewState == ENoclipMusicState::Explore && ExploreResumePath.IsValid() && ExploreResumeAt < 80.f)
	{
		Path = ExploreResumePath;
		StartAt = ExploreResumeAt;
	}
	else
	{
		Path = PickFromPool(NewState, Music.CurrentPath);
	}
	// Turno novo: a exploracao comeca do zero.
	if (NewState == ENoclipMusicState::Descending || NewState == ENoclipMusicState::Lobby)
	{
		ExploreResumePath.Reset();
	}

	if (!Path.IsValid())
	{
		FadeOutLayer(0, FadeOutOld);
		return;
	}
	PlayOnLayer(0, Path, Def.Volume, Def.FadeIn, FadeOutOld, StartAt);
	UE_LOG(LogNoclip, Verbose, TEXT("[Audio] Musica: %s -> %s"), NoclipSounds::StateName(NewState), *Path.GetAssetName());
}

FSoftObjectPath UNoclipAudioSubsystem::PickFromPool(ENoclipMusicState State, const FSoftObjectPath& Avoid) const
{
	const NoclipSounds::FMusicDef& Def = NoclipSounds::Music(State);
	if (Def.Tracks.Num() == 0)
	{
		return FSoftObjectPath();
	}
	TArray<FSoftObjectPath> Options;
	for (const TCHAR* T : Def.Tracks)
	{
		const FSoftObjectPath P = NoclipSounds::Path(T);
		if (P != Avoid || Def.Tracks.Num() == 1)
		{
			Options.Add(P);
		}
	}
	return Options.Num() > 0 ? Options[FMath::RandRange(0, Options.Num() - 1)] : NoclipSounds::Path(Def.Tracks[0]);
}

UNoclipAudioSubsystem::EBed UNoclipAudioSubsystem::EvaluateBed(UWorld* World) const
{
	const ANoclipPlayerController* PC = Cast<ANoclipPlayerController>(GetLocalController(World));
	if (!PC || PC->IsFrontend())
	{
		return EBed::None;
	}
	if (PC->IsWaitingRoom())
	{
		for (TActorIterator<ANoclipWaitingRoom> It(World); It; ++It)
		{
			// Portas fechando: o saguao some antes da viagem (sem corte).
			return It->GetPhase() == ENoclipWaitingPhase::Departing ? EBed::None : EBed::WaitingHum;
		}
		return EBed::WaitingHum;
	}
	const ANoclipGameState* GS = World->GetGameState<ANoclipGameState>();
	if (!GS)
	{
		return EBed::None;
	}
	switch (GS->GetPhase())
	{
	case ENoclipShiftPhase::Active:   return GS->IsPowerOut() ? EBed::None : EBed::LevelHum; // sem energia, sem zumbido
	case ENoclipShiftPhase::Collapse: return EBed::CollapseWind;
	case ENoclipShiftPhase::Ended:    return EBed::None;
	default:                          return EBed::CabinHum;
	}
}

void UNoclipAudioSubsystem::ApplyBed(EBed NewBed)
{
	const EBed Old = BedState;
	BedState = NewBed;
	if (NewBed == EBed::None)
	{
		FadeOutLayer(1, 3.f);
		return;
	}
	const FSoftObjectPath Hum = NoclipSounds::Path(TEXT("SFX/White_Noise_Sfx_"));
	const FSoftObjectPath Wind = NoclipSounds::Path(TEXT("SFX/Wind_Sfx"));
	float Volume = 0.08f;
	float LowPass = 700.f;
	FSoftObjectPath Path = Hum;
	switch (NewBed)
	{
	case EBed::WaitingHum:   Volume = 0.05f; LowPass = 500.f; break;           // ar-condicionado do saguao
	case EBed::CabinHum:     Volume = 0.06f; LowPass = 450.f; break;           // motor do elevador
	case EBed::LevelHum:     Volume = 0.11f; LowPass = 1100.f; break;          // zumbido das fluorescentes
	case EBed::CollapseWind: Volume = 0.14f; LowPass = 1600.f; Path = Wind; break; // a energia caindo
	default: break;
	}
	// Mesmo som: so muda volume e filtro devagar, sem reiniciar.
	if (Path == Bed.CurrentPath && IsValid(Bed.Current))
	{
		Bed.Volume = Volume;
		Bed.LowPass = LowPass;
		Bed.Current->AdjustVolume(2.5f, Volume);
		Bed.Current->SetLowPassFilterFrequency(LowPass);
		return;
	}
	Bed.LowPass = LowPass;
	PlayOnLayer(1, Path, Volume, Old == EBed::None ? 4.f : 3.f, NewBed == EBed::CollapseWind ? 8.f : 3.f);
}

// ---------------------------------------------------------------------------------------------------------
// Camadas
// ---------------------------------------------------------------------------------------------------------

void UNoclipAudioSubsystem::PlayOnLayer(int32 LayerIndex, const FSoftObjectPath& Path, float Volume, float FadeIn, float FadeOutOld, float StartAt)
{
	FNoclipAudioLayer& L = Layer(LayerIndex);
	L.PendingPath = Path;
	L.PendingVolume = Volume;
	L.PendingFadeIn = FadeIn;
	L.PendingFadeOut = FadeOutOld;
	L.PendingStartAt = StartAt;
	const uint32 Serial = ++L.RequestSerial;

	if (const TObjectPtr<USoundBase>* Cached = TrackCache.Find(FName(*Path.ToString())))
	{
		if (IsValid(*Cached))
		{
			StartPending(LayerIndex, *Cached);
			return;
		}
	}
	if (USoundBase* Loaded = Cast<USoundBase>(Path.ResolveObject()))
	{
		StartPending(LayerIndex, Loaded);
		return;
	}
	// Carrega sem travar o jogo; a faixa atual continua ate a nova estar pronta.
	Streamable.RequestAsyncLoad(Path, FStreamableDelegate::CreateUObject(this, &UNoclipAudioSubsystem::OnTrackLoaded, LayerIndex, Serial));
}

void UNoclipAudioSubsystem::OnTrackLoaded(int32 LayerIndex, uint32 Serial)
{
	FNoclipAudioLayer& L = Layer(LayerIndex);
	if (Serial != L.RequestSerial || !bReady)
	{
		return; // ja pediram outra
	}
	if (USoundBase* Sound = Cast<USoundBase>(L.PendingPath.ResolveObject()))
	{
		StartPending(LayerIndex, Sound);
	}
}

void UNoclipAudioSubsystem::StartPending(int32 LayerIndex, USoundBase* Sound)
{
	FNoclipAudioLayer& L = Layer(LayerIndex);
	UWorld* World = GetTickableGameObjectWorld();
	if (!World || !Sound)
	{
		return;
	}

	// Guarda as ultimas faixas carregadas (as outras o GC solta quando param de tocar).
	const FName Key(*L.PendingPath.ToString());
	if (!TrackCache.Contains(Key))
	{
		TrackCache.Add(Key, Sound);
		TrackCacheOrder.Add(Key);
		while (TrackCacheOrder.Num() > 6)
		{
			TrackCache.Remove(TrackCacheOrder[0]);
			TrackCacheOrder.RemoveAt(0);
		}
	}

	FadeOutLayer(LayerIndex, L.PendingFadeOut);

	const float StartAt = FMath::Clamp(L.PendingStartAt, 0.f, FMath::Max(0.f, Sound->GetDuration() - 5.f));
	UAudioComponent* C = UGameplayStatics::CreateSound2D(World, Sound, 1.f, 1.f, 0.f, nullptr, L.bPersistent, false);
	if (!C)
	{
		return;
	}
	NoclipSounds::ConfigureFlat(C);
	C->bIsUISound = L.bPersistent; // musica toca com o jogo pausado
	if (L.LowPass > 0.f)
	{
		C->SetLowPassFilterEnabled(true);
		C->SetLowPassFilterFrequency(L.LowPass);
	}
	const float Volume = L.PendingVolume * (LayerIndex == 0 ? GetMusicScale() : 1.f);
	C->FadeIn(FMath::Max(L.PendingFadeIn, 0.05f), Volume, StartAt);

	L.Current = C;
	L.CurrentPath = L.PendingPath;
	L.Volume = L.PendingVolume;
	L.Elapsed = StartAt;
	L.Duration = Sound->GetDuration();
	L.PendingPath.Reset();
}

void UNoclipAudioSubsystem::FadeOutLayer(int32 LayerIndex, float Seconds)
{
	FNoclipAudioLayer& L = Layer(LayerIndex);
	if (IsValid(L.Current))
	{
		L.Current->FadeOut(FMath::Max(Seconds, 0.05f), 0.f);
		L.Fading.Add(L.Current);
	}
	L.Current = nullptr;
	L.CurrentPath.Reset();
	L.Duration = 0.f;
	L.Elapsed = 0.f;
}

void UNoclipAudioSubsystem::TickLayer(int32 LayerIndex, float DeltaTime)
{
	FNoclipAudioLayer& L = Layer(LayerIndex);

	for (int32 i = L.Fading.Num() - 1; i >= 0; --i)
	{
		UAudioComponent* C = L.Fading[i];
		if (!IsValid(C))
		{
			L.Fading.RemoveAt(i);
		}
		else if (!C->IsPlaying())
		{
			C->DestroyComponent();
			L.Fading.RemoveAt(i);
		}
	}

	if (!IsValid(L.Current))
	{
		L.Current = nullptr;
		return;
	}
	L.Elapsed += DeltaTime;
	// Acabou antes da conta (o relogio para durante o carregamento de mapa): segue sem esperar.
	if (!L.Current->IsPlaying() && L.Elapsed > 1.f && !L.PendingPath.IsValid())
	{
		L.Elapsed = FMath::Max(L.Elapsed, L.Duration);
	}
	// Perto do fim: a proxima ja entra por baixo (crossfade, nunca silencio nem corte).
	if (L.Duration > 0.f && L.Duration < 3600.f && L.Elapsed >= L.Duration - L.CrossFade && !L.PendingPath.IsValid())
	{
		if (LayerIndex == 0)
		{
			const NoclipSounds::FMusicDef& Def = NoclipSounds::Music(MusicState);
			const FSoftObjectPath Next = PickFromPool(MusicState, L.CurrentPath);
			if (Next.IsValid())
			{
				PlayOnLayer(0, Next, Def.Volume, L.CrossFade, L.CrossFade);
			}
			if (MusicState == ENoclipMusicState::Explore)
			{
				ExploreResumePath.Reset();
			}
		}
		else
		{
			const FSoftObjectPath Same = L.CurrentPath;
			PlayOnLayer(1, Same, L.Volume, L.CrossFade, L.CrossFade);
		}
	}
}

// ---------------------------------------------------------------------------------------------------------
// Eventos e efeitos
// ---------------------------------------------------------------------------------------------------------

USoundBase* UNoclipAudioSubsystem::Sfx(const TCHAR* Name)
{
	const FName Key(Name);
	if (const TObjectPtr<USoundBase>* Found = SfxCache.Find(Key))
	{
		return *Found;
	}
	USoundBase* S = Cast<USoundBase>(NoclipSounds::Path(Name).TryLoad());
	SfxCache.Add(Key, S);
	return S;
}

USoundBase* UNoclipAudioSubsystem::PickSfx(const TArray<const TCHAR*>& Names, int32& LastIndex)
{
	if (Names.Num() == 0)
	{
		return nullptr;
	}
	int32 Index = FMath::RandRange(0, Names.Num() - 1);
	if (Names.Num() > 1 && Index == LastIndex)
	{
		Index = (Index + 1) % Names.Num();
	}
	LastIndex = Index;
	return Sfx(Names[Index]);
}

void UNoclipAudioSubsystem::Play2D(USoundBase* Sound, float Volume, float Pitch)
{
	UWorld* World = GetTickableGameObjectWorld();
	if (World && Sound)
	{
		if (UAudioComponent* C = UGameplayStatics::CreateSound2D(World, Sound, Volume, Pitch, 0.f, nullptr, false, true))
		{
			NoclipSounds::ConfigureFlat(C);
			C->Play();
		}
	}
}

void UNoclipAudioSubsystem::PlayAt(USoundBase* Sound, const FVector& Location, float Volume, USoundAttenuation* Attenuation, float Pitch, float LowPass)
{
	UWorld* World = GetTickableGameObjectWorld();
	if (!World || !Sound)
	{
		return;
	}
	if (UAudioComponent* C = UGameplayStatics::SpawnSoundAtLocation(World, Sound, Location, FRotator::ZeroRotator, Volume, Pitch, 0.f, Attenuation))
	{
		if (LowPass > 0.f)
		{
			C->SetLowPassFilterEnabled(true);
			C->SetLowPassFilterFrequency(LowPass);
		}
	}
}

void UNoclipAudioSubsystem::TickEvents(UWorld* World, float DeltaTime, double Now)
{
	ANoclipPlayerController* PC = Cast<ANoclipPlayerController>(GetLocalController(World));
	const ANoclipGameState* GS = World->GetGameState<ANoclipGameState>();
	if (!PC || PC->IsFrontend() || PC->IsWaitingRoom() || !GS)
	{
		return;
	}

	// Fases: comeco do turno e colapso.
	const uint8 Phase = static_cast<uint8>(GS->GetPhase());
	if (PrevPhase != 255 && Phase != PrevPhase)
	{
		if (GS->GetPhase() == ENoclipShiftPhase::Active)
		{
			Play2D(Sfx(TEXT("SFX/New_Mision_Sfx")), 0.45f);
			bQuotaChimed = false;
		}
		else if (GS->GetPhase() == ENoclipShiftPhase::Collapse)
		{
			Play2D(Sfx(TEXT("SFX/Dark_Riser")), 0.7f);
		}
	}
	PrevPhase = Phase;

	const bool bInShift = GS->GetPhase() == ENoclipShiftPhase::Active || GS->GetPhase() == ENoclipShiftPhase::Collapse;

	// Energia caiu / voltou.
	if (GS->IsPowerOut() != bPrevPowerOut)
	{
		bPrevPowerOut = GS->IsPowerOut();
		if (bPrevPowerOut)
		{
			Play2D(Sfx(TEXT("SFX/Dark_Riser")), 0.5f, 0.85f);
			Play2D(Sfx(TEXT("SFX/Creepy_Sound_Sfx_4")), 0.45f);
		}
		else if (bInShift)
		{
			Play2D(Sfx(TEXT("SFX/New_Mision_Sfx")), 0.5f, 0.88f);
		}
	}

	// Estatica do noclip: corta curta.
	if (IsValid(NoclipSting) && Now >= NoclipStingStopTime && NoclipStingStopTime > 0.0)
	{
		NoclipSting->FadeOut(0.6f, 0.f);
		NoclipStingStopTime = 0.0;
	}

	// Quota batida dentro da Sala Manila.
	if (bInShift && !bQuotaChimed && GS->GetQuota() > 0 && GS->GetValueInExtraction() >= GS->GetQuota())
	{
		bQuotaChimed = true;
		Play2D(Sfx(TEXT("SFX/New_Mision_Sfx")), 0.6f, 1.12f);
	}

	// Perseguicao comecou: um susto curto por cima da musica.
	if (MusicState == ENoclipMusicState::Chase && LastEventMusicState != ENoclipMusicState::Chase && Now >= NextChaseStingTime)
	{
		NextChaseStingTime = Now + 25.0;
		Play2D(Sfx(TEXT("SFX/Jumpscare_Sfx")), 0.38f);
	}
	LastEventMusicState = MusicState;

	// Morte: a sua (sem posicao) e a dos colegas (no corpo).
	const bool bDead = PC->IsSpectating();
	if (bDead && !bPrevDead)
	{
		Play2D(Sfx(TEXT("SFX/Death_Sfx")), 0.8f);
	}
	bPrevDead = bDead;
	for (TActorIterator<ANoclipCorpse> It(World); It; ++It)
	{
		ANoclipCorpse* Corpse = *It;
		if (SeenCorpses.Contains(Corpse))
		{
			continue;
		}
		SeenCorpses.Add(Corpse);
		if (Corpse->GetOwnerPlayer() != PC->PlayerState && bInShift)
		{
			PlayAt(PickSfx(NoclipSounds::DeathOthers, LastDeathSfx), Corpse->GetBodyLocation(), 0.85f, EventAttenuation);
		}
	}

	const ANoclipCharacter* Pawn = Cast<ANoclipCharacter>(PC->GetPawn());
	if (!Pawn)
	{
		PrevStamina = 1.f;
		PrevHurtAge = 100.f;
		return;
	}

	// Sem folego: respiracao funda.
	const float Stamina = Pawn->GetStamina();
	if (Stamina < 0.03f && PrevStamina >= 0.03f && Now >= NextBreathTime)
	{
		NextBreathTime = Now + 12.0;
		Play2D(Sfx(TEXT("SFX/Deep_breath")), 0.55f);
	}
	PrevStamina = Stamina;

	// Levou um golpe forte (entidade): susto.
	const float HurtAge = Pawn->GetHurtAge();
	if (HurtAge < PrevHurtAge - 0.05f && HurtAge < 0.2f && Pawn->GetHurtStrength() >= 0.6f && Now >= NextJumpscareTime)
	{
		NextJumpscareTime = Now + 6.0;
		Play2D(Sfx(TEXT("SFX/Jumpscare_Sfx_2")), 0.5f);
	}
	PrevHurtAge = HurtAge;
}

void UNoclipAudioSubsystem::TickDistantSounds(UWorld* World, double Now)
{
	const APlayerController* PC = GetLocalController(World);
	const ANoclipGameState* GS = World->GetGameState<ANoclipGameState>();
	const bool bInShift = GS && (GS->GetPhase() == ENoclipShiftPhase::Active || GS->GetPhase() == ENoclipShiftPhase::Collapse);
	if (!PC || !bInShift || !PC->PlayerCameraManager)
	{
		NextDistantTime = 0.0;
		return;
	}
	if (NextDistantTime <= 0.0)
	{
		NextDistantTime = Now + FMath::FRandRange(25.f, 45.f); // nao logo de cara
		return;
	}
	if (Now < NextDistantTime)
	{
		return;
	}
	// Na perseguicao, a musica ja diz tudo.
	if (MusicState == ENoclipMusicState::Chase || MusicState == ENoclipMusicState::SafeRoom)
	{
		NextDistantTime = Now + 10.0;
		return;
	}
	NextDistantTime = Now + FMath::FRandRange(35.f, 80.f);
	const FVector Ear = PC->PlayerCameraManager->GetCameraLocation();
	const FVector Dir = FRotator(0.f, FMath::FRandRange(0.f, 360.f), 0.f).Vector();
	const FVector Where = Ear + Dir * FMath::FRandRange(1400.f, 3200.f) + FVector(0.f, 0.f, FMath::FRandRange(-40.f, 60.f));
	PlayAt(PickSfx(NoclipSounds::Distant, LastDistant), Where, 0.65f, DistantAttenuation, FMath::FRandRange(0.9f, 1.04f));
}

void UNoclipAudioSubsystem::TickEnemySteps(UWorld* World, float DeltaTime)
{
	// Passos das entidades: pesados e lentos (paciente) ou leves e rapidos (cao). Ajudam a saber de onde vem.
	for (TActorIterator<ANoclipEnemy> It(World); It; ++It)
	{
		ANoclipEnemy* Enemy = *It;
		const float Speed = Enemy->GetVelocity().Size2D();
		float& Phase = EnemyStepPhase.FindOrAdd(Enemy);
		if (Speed < 40.f)
		{
			continue;
		}
		const bool bHound = Enemy->GetKind() == ENoclipEnemyKind::Hound;
		const float Stride = bHound ? 95.f : 115.f;
		const float Prev = Phase;
		Phase += DeltaTime * Speed / Stride;
		if (FMath::FloorToInt(Prev) != FMath::FloorToInt(Phase))
		{
			FVector Feet;
			const ENoclipSurface Surface = DetectSurface(Enemy, Feet);
			const float Running = FMath::Clamp(Speed / 500.f, 0.3f, 1.2f);
			PlayAt(PickStep(Surface), Feet, (bHound ? 0.45f : 0.8f) * Running, EnemyStepAttenuation,
				bHound ? FMath::FRandRange(1.15f, 1.3f) : FMath::FRandRange(0.66f, 0.74f), Surface == ENoclipSurface::Carpet ? 1500.f : 0.f);
		}
	}
	for (auto It = EnemyStepPhase.CreateIterator(); It; ++It)
	{
		if (!It.Key().IsValid())
		{
			It.RemoveCurrent();
		}
	}
}

// ---------------------------------------------------------------------------------------------------------
// Passos
// ---------------------------------------------------------------------------------------------------------

ENoclipSurface UNoclipAudioSubsystem::DetectSurface(const AActor* Walker, FVector& OutFeet) const
{
	OutFeet = Walker ? Walker->GetActorLocation() : FVector::ZeroVector;
	UWorld* World = Walker ? Walker->GetWorld() : nullptr;
	if (!World)
	{
		return ENoclipSurface::Carpet;
	}
	FCollisionQueryParams Params(SCENE_QUERY_STAT(NoclipFootstep), false, Walker);
	FHitResult Hit;
	const FVector Start = Walker->GetActorLocation();
	if (!World->LineTraceSingleByChannel(Hit, Start, Start - FVector(0.f, 0.f, 220.f), ECC_Visibility, Params))
	{
		return ENoclipSurface::Carpet;
	}
	OutFeet = Hit.ImpactPoint;

	const AActor* HitActor = Hit.GetActor();
	if (Cast<ANoclipLootActor>(HitActor))
	{
		return ENoclipSurface::Wood; // pisando num movel / caixa
	}
	// Pelo nome do material do piso (MI_L0_Carpet, M_Metal..., etc.).
	FString Name;
	if (const UPrimitiveComponent* Comp = Hit.GetComponent())
	{
		if (const UMaterialInterface* Mat = Comp->GetMaterial(0))
		{
			Name = Mat->GetName();
		}
		if (Name.IsEmpty())
		{
			Name = Comp->GetName();
		}
	}
	Name.ToLowerInline();
	if (Name.Contains(TEXT("carpet")) || Name.Contains(TEXT("rug")))    return ENoclipSurface::Carpet;
	if (Name.Contains(TEXT("metal")) || Name.Contains(TEXT("steel")))   return ENoclipSurface::Metal;
	if (Name.Contains(TEXT("wood")) || Name.Contains(TEXT("madeira")))  return ENoclipSurface::Wood;
	if (Name.Contains(TEXT("stair")) || Name.Contains(TEXT("escada")))  return ENoclipSurface::Stairs;
	if (Name.Contains(TEXT("tile")) || Name.Contains(TEXT("stone")) || Name.Contains(TEXT("marble")) || Name.Contains(TEXT("piso"))) return ENoclipSurface::Stone;
	if (Name.Contains(TEXT("concrete")) || Name.Contains(TEXT("cement"))) return ENoclipSurface::Concrete;
	if (Name.Contains(TEXT("gravel")))                                  return ENoclipSurface::Gravel;
	if (Name.Contains(TEXT("grass")))                                   return ENoclipSurface::Grass;
	if (Name.Contains(TEXT("dirt")) || Name.Contains(TEXT("mud")))      return ENoclipSurface::Dirt;
	// Sala de espera (piso do saguao) e o resto do Level 0 (carpete umido).
	if (Cast<ANoclipWaitingRoom>(HitActor))
	{
		return ENoclipSurface::Stone;
	}
	return ENoclipSurface::Carpet;
}

USoundBase* UNoclipAudioSubsystem::PickStep(ENoclipSurface Surface)
{
	const int32 S = static_cast<int32>(Surface == ENoclipSurface::Carpet ? ENoclipSurface::Concrete : Surface);
	if (!Steps.IsValidIndex(S) || Steps[S].Sounds.Num() == 0)
	{
		return nullptr;
	}
	const TArray<TObjectPtr<USoundBase>>& List = Steps[S].Sounds;
	int32 Index = FMath::RandRange(0, List.Num() - 1);
	if (List.Num() > 1 && Index == LastStep[S])
	{
		Index = (Index + 1 + FMath::RandRange(0, List.Num() - 2)) % List.Num();
	}
	LastStep[S] = Index;
	return List[Index];
}

void UNoclipAudioSubsystem::PlayFootstep(const AActor* Walker, bool bSprinting, bool bLocal)
{
	if (!bReady || !Walker)
	{
		return;
	}
	FVector Feet;
	const ENoclipSurface Surface = DetectSurface(Walker, Feet);
	USoundBase* Sound = PickStep(Surface);
	if (!Sound)
	{
		return;
	}
	const bool bCarpet = Surface == ENoclipSurface::Carpet;
	// Carpete: o mesmo passo, mais baixo e abafado (o carpete umido do Level 0 engole o salto).
	const float Base = (bSprinting ? 1.f : 0.62f) * (bCarpet ? 0.8f : 1.f);
	const float Pitch = FMath::FRandRange(0.93f, 1.07f);
	const float LowPass = bCarpet ? 1900.f : 0.f;
	if (bLocal)
	{
		UWorld* World = GetTickableGameObjectWorld();
		if (UAudioComponent* C = World ? UGameplayStatics::SpawnSound2D(World, Sound, Base * 0.42f, Pitch) : nullptr)
		{
			if (LowPass > 0.f)
			{
				C->SetLowPassFilterEnabled(true);
				C->SetLowPassFilterFrequency(LowPass);
			}
		}
		return;
	}
	PlayAt(Sound, Feet, Base * 0.9f, FootstepAttenuation, Pitch, LowPass);
}

void UNoclipAudioSubsystem::PlayLanding(const AActor* Walker, float Intensity, bool bLocal)
{
	if (!bReady || !Walker)
	{
		return;
	}
	FVector Feet;
	const ENoclipSurface Surface = DetectSurface(Walker, Feet);
	USoundBase* Sound = PickStep(Surface);
	if (!Sound)
	{
		return;
	}
	const float Volume = FMath::Lerp(0.7f, 1.3f, FMath::Clamp(Intensity, 0.f, 1.f));
	const float LowPass = Surface == ENoclipSurface::Carpet ? 1500.f : 0.f;
	if (bLocal)
	{
		UWorld* World = GetTickableGameObjectWorld();
		if (UAudioComponent* C = World ? UGameplayStatics::SpawnSound2D(World, Sound, Volume * 0.5f, 0.82f) : nullptr)
		{
			if (LowPass > 0.f)
			{
				C->SetLowPassFilterEnabled(true);
				C->SetLowPassFilterFrequency(LowPass);
			}
		}
		return;
	}
	PlayAt(Sound, Feet, Volume, FootstepAttenuation, 0.82f, LowPass);
}

// ---------------------------------------------------------------------------------------------------------
// Noclip, gerador e divisorias
// ---------------------------------------------------------------------------------------------------------

void UNoclipAudioSubsystem::PlayWallShift(const FVector& Location)
{
	if (bReady)
	{
		// Rangido grave e lento: algo grande se mexeu do outro lado.
		PlayAt(Sfx(TEXT("SFX/Creepy_Sound_Sfx_3")), Location, 0.75f, EventAttenuation, 0.62f, 2500.f);
	}
}

void UNoclipAudioSubsystem::PlayGeneratorFeedback(const FVector& Location, bool bHit, float Progress)
{
	if (!bReady)
	{
		return;
	}
	USoundBase* Clank = PickStep(ENoclipSurface::Metal);
	if (bHit)
	{
		// Estalo metalico que sobe de tom conforme carrega.
		PlayAt(Clank, Location, 0.85f, FootstepAttenuation, 1.15f + Progress * 0.5f);
	}
	else
	{
		// Faisca: pancada grave e um chiado curto.
		PlayAt(Clank, Location, 1.2f, EventAttenuation, 0.55f);
		PlayAt(Sfx(TEXT("SFX/Jumpscare_Sfx")), Location, 0.35f, EventAttenuation, 1.6f, 6000.f);
	}
}

void UNoclipAudioSubsystem::PlayNoclipSting()
{
	UWorld* World = GetTickableGameObjectWorld();
	USoundBase* Noise = Sfx(TEXT("SFX/White_Noise_Sfx_"));
	if (!World || !Noise)
	{
		return;
	}
	if (IsValid(NoclipSting))
	{
		NoclipSting->Stop();
	}
	NoclipSting = UGameplayStatics::CreateSound2D(World, Noise, 1.f, 1.35f, 0.f, nullptr, false, true);
	if (NoclipSting)
	{
		NoclipSounds::ConfigureFlat(NoclipSting);
		NoclipSting->FadeIn(0.04f, 0.6f, FMath::FRandRange(0.f, 15.f));
		NoclipStingStopTime = FPlatformTime::Seconds() + 0.55;
	}
	Play2D(Sfx(TEXT("SFX/Mysterious_Sound_Sfx_5")), 0.55f);
}

void UNoclipAudioSubsystem::TickGlitchHiss(UWorld* World)
{
	const APlayerController* PC = GetLocalController(World);
	ANoclipLevelGenerator* Gen = nullptr;
	for (TActorIterator<ANoclipLevelGenerator> It(World); It; ++It)
	{
		Gen = *It;
		break;
	}
	const ANoclipGameState* GS = World->GetGameState<ANoclipGameState>();
	const bool bInShift = GS && (GS->GetPhase() == ENoclipShiftPhase::Active || GS->GetPhase() == ENoclipShiftPhase::Collapse);

	// As duas falhas mais perto (ate 7 m) chiam baixinho.
	TArray<int32> Wanted;
	if (PC && PC->PlayerCameraManager && Gen && bInShift)
	{
		const FVector Ear = PC->PlayerCameraManager->GetCameraLocation();
		TArray<TPair<float, int32>> Near;
		const TArray<FNoclipGlitch>& Glitches = Gen->GetGlitches();
		for (int32 i = 0; i < Glitches.Num(); ++i)
		{
			if (Glitches[i].Kind == ENoclipGlitchKind::ServiceExit)
			{
				continue;
			}
			const float Dist = FVector::Dist(Glitches[i].Box.GetCenter(), Ear);
			if (Dist < 700.f)
			{
				Near.Add(TPair<float, int32>(Dist, i));
			}
		}
		Near.Sort([](const TPair<float, int32>& A, const TPair<float, int32>& B) { return A.Key < B.Key; });
		for (int32 i = 0; i < FMath::Min(2, Near.Num()); ++i)
		{
			Wanted.Add(Near[i].Value);
		}
	}

	// Some quem saiu da lista (ou terminou o arquivo).
	for (int32 i = HissComps.Num() - 1; i >= 0; --i)
	{
		UAudioComponent* C = HissComps[i];
		if (!IsValid(C) || !Wanted.Contains(HissGlitch[i]) || !C->IsPlaying())
		{
			if (IsValid(C))
			{
				C->FadeOut(0.8f, 0.f);
			}
			HissComps.RemoveAt(i);
			HissGlitch.RemoveAt(i);
		}
	}
	USoundBase* Noise = Sfx(TEXT("SFX/White_Noise_Sfx_"));
	if (!Noise || !Gen)
	{
		return;
	}
	for (const int32 G : Wanted)
	{
		if (HissGlitch.Contains(G))
		{
			continue;
		}
		const FVector Where = Gen->GetGlitches()[G].Box.GetCenter();
		UAudioComponent* C = UGameplayStatics::SpawnSoundAtLocation(World, Noise, Where, FRotator::ZeroRotator, 0.f, 1.9f, FMath::FRandRange(0.f, 20.f), FootstepAttenuation);
		if (C)
		{
			C->SetLowPassFilterEnabled(true);
			C->SetLowPassFilterFrequency(6000.f);
			C->FadeIn(1.2f, 0.09f);
			HissComps.Add(C);
			HissGlitch.Add(G);
		}
	}
}
