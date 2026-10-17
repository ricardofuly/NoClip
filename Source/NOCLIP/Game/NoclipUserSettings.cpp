// Copyright Fuly Games. All Rights Reserved.

#include "Game/NoclipUserSettings.h"
#include "Misc/ConfigCacheIni.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Engine/PostProcessVolume.h"
#include "Engine/UserInterfaceSettings.h"
#include "AudioDevice.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/Material.h"
#include "EngineUtils.h"
#include "UObject/UObjectIterator.h"

namespace
{
	const TCHAR* Section = TEXT("NoclipUserSettings");

	template <typename T>
	void Read(const TCHAR* Key, T& Value);

	template <> void Read<int32>(const TCHAR* Key, int32& Value) { GConfig->GetInt(Section, Key, Value, GGameUserSettingsIni); }
	template <> void Read<float>(const TCHAR* Key, float& Value) { GConfig->GetFloat(Section, Key, Value, GGameUserSettingsIni); }
	template <> void Read<bool>(const TCHAR* Key, bool& Value) { GConfig->GetBool(Section, Key, Value, GGameUserSettingsIni); }
	template <> void Read<FString>(const TCHAR* Key, FString& Value) { GConfig->GetString(Section, Key, Value, GGameUserSettingsIni); }

	void Write(const TCHAR* Key, int32 Value) { GConfig->SetInt(Section, Key, Value, GGameUserSettingsIni); }
	void Write(const TCHAR* Key, float Value) { GConfig->SetFloat(Section, Key, Value, GGameUserSettingsIni); }
	void Write(const TCHAR* Key, bool Value) { GConfig->SetBool(Section, Key, Value, GGameUserSettingsIni); }
	void Write(const TCHAR* Key, const FString& Value) { GConfig->SetString(Section, Key, *Value, GGameUserSettingsIni); }

	/** MIDs criados a partir dos materiais PSX originais (um por material de origem). */
	TMap<TWeakObjectPtr<UMaterialInterface>, TWeakObjectPtr<UMaterialInstanceDynamic>> GWobbleMIDs;
	TMap<TWeakObjectPtr<UMaterialInterface>, float> GWobbleOriginal;
}

FNoclipUserSettings& FNoclipUserSettings::Get()
{
	static FNoclipUserSettings Settings;
	if (!Settings.bLoaded)
	{
		Settings.Load();
	}
	return Settings;
}

#define NOCLIP_SETTINGS_FIELDS(X) \
	X(PSXLines) X(Dither) X(bVertexWobble) X(FieldOfView) X(FPSLimit) X(bScanlines) \
	X(MasterVolume) X(VoiceVolume) X(MusicVolume) X(MicMode) X(MicSensitivity) X(OutputDevice) X(InputDevice) \
	X(MouseSensitivity) X(StickSensitivity) X(bInvertY) X(bRumble) X(PadGlyphs) \
	X(bReduceFlashing) X(CameraBob) X(TextScale) X(Colorblind) X(bShowValueOnAim) \
	X(LastShiftSummary) X(CharacterIndex)

void FNoclipUserSettings::Load()
{
	bLoaded = true;
	if (!GConfig)
	{
		return;
	}
#define NOCLIP_READ(Name) Read(TEXT(#Name), Name);
	NOCLIP_SETTINGS_FIELDS(NOCLIP_READ)
#undef NOCLIP_READ

	PSXLines = FMath::Clamp(PSXLines, 0, 3);
	Dither = FMath::Clamp(Dither, 0, 100);
	FieldOfView = FMath::Clamp(FieldOfView, 70, 110);
	FPSLimit = FMath::Clamp(FPSLimit, 0, 3);
	MasterVolume = FMath::Clamp(MasterVolume, 0, 100);
	VoiceVolume = FMath::Clamp(VoiceVolume, 0, 100);
	MusicVolume = FMath::Clamp(MusicVolume, 0, 100);
	MicMode = FMath::Clamp(MicMode, 0, 1);
	MicSensitivity = FMath::Clamp(MicSensitivity, 0, 100);
	MouseSensitivity = FMath::Clamp(MouseSensitivity, MinSensitivity, MaxSensitivity);
	StickSensitivity = FMath::Clamp(StickSensitivity, MinSensitivity, MaxSensitivity);
	PadGlyphs = FMath::Clamp(PadGlyphs, 0, 2);
	CameraBob = FMath::Clamp(CameraBob, 0, 100);
	TextScale = FMath::Clamp(TextScale, 0, 2);
	Colorblind = FMath::Clamp(Colorblind, 0, 3);

	ApplyGlobal();
}

void FNoclipUserSettings::Save() const
{
	if (!GConfig)
	{
		return;
	}
#define NOCLIP_WRITE(Name) Write(TEXT(#Name), Name);
	NOCLIP_SETTINGS_FIELDS(NOCLIP_WRITE)
#undef NOCLIP_WRITE
	GConfig->Flush(false, GGameUserSettingsIni);
	ApplyGlobal();
}

void FNoclipUserSettings::ResetSection(ENoclipSettingsSection InSection)
{
	const FNoclipUserSettings D;
	switch (InSection)
	{
	case ENoclipSettingsSection::Video:
		PSXLines = D.PSXLines; Dither = D.Dither; bVertexWobble = D.bVertexWobble; FieldOfView = D.FieldOfView; FPSLimit = D.FPSLimit; bScanlines = D.bScanlines;
		break;
	case ENoclipSettingsSection::Audio:
		MasterVolume = D.MasterVolume; VoiceVolume = D.VoiceVolume; MusicVolume = D.MusicVolume; MicMode = D.MicMode; MicSensitivity = D.MicSensitivity;
		OutputDevice = D.OutputDevice; InputDevice = D.InputDevice;
		break;
	case ENoclipSettingsSection::Controls:
		MouseSensitivity = D.MouseSensitivity; StickSensitivity = D.StickSensitivity; bInvertY = D.bInvertY; bRumble = D.bRumble; PadGlyphs = D.PadGlyphs;
		break;
	case ENoclipSettingsSection::Accessibility:
		bReduceFlashing = D.bReduceFlashing; CameraBob = D.CameraBob; TextScale = D.TextScale; Colorblind = D.Colorblind; bShowValueOnAim = D.bShowValueOnAim;
		break;
	}
}

void FNoclipUserSettings::ApplyGlobal() const
{
	static const float FPSValues[] = { 60.f, 120.f, 144.f, 0.f };
	if (GEngine)
	{
		GEngine->SetMaxFPS(FPSValues[FMath::Clamp(FPSLimit, 0, 3)]);
		if (FAudioDevice* Audio = GEngine->GetMainAudioDeviceRaw())
		{
			Audio->SetTransientPrimaryVolume(MasterVolume / 100.f);
		}
	}

	static const float TextScales[] = { 1.f, 1.25f, 1.5f };
	if (UUserInterfaceSettings* UI = GetMutableDefault<UUserInterfaceSettings>())
	{
		UI->ApplicationScale = TextScales[FMath::Clamp(TextScale, 0, 2)];
	}
}

void FNoclipUserSettings::ApplyToWorld(UWorld* World) const
{
	if (!World)
	{
		return;
	}

	// ---------- Pos-processo PSX ----------
	static const float Lines[] = { 240.f, 360.f, 480.f, 0.f };
	const float VerticalResolution = Lines[FMath::Clamp(PSXLines, 0, 3)];
	for (TActorIterator<APostProcessVolume> It(World); It; ++It)
	{
		for (FWeightedBlendable& Blendable : It->Settings.WeightedBlendables.Array)
		{
			UMaterialInterface* Material = Cast<UMaterialInterface>(Blendable.Object);
			if (!Material || !Material->GetBaseMaterial() || !Material->GetBaseMaterial()->GetName().Equals(TEXT("M_PP_PSX")))
			{
				continue;
			}
			UMaterialInstanceDynamic* MID = Cast<UMaterialInstanceDynamic>(Material);
			if (!MID)
			{
				MID = UMaterialInstanceDynamic::Create(Material, *It);
				Blendable.Object = MID;
			}
			Blendable.Weight = VerticalResolution > 0.f ? 1.f : 0.f;
			if (VerticalResolution > 0.f)
			{
				MID->SetScalarParameterValue(TEXT("VerticalResolution"), VerticalResolution);
			}
			MID->SetScalarParameterValue(TEXT("DitherAmount"), Dither / 100.f);
		}
	}

	// ---------- Tremido dos vertices (M_PSX_Surface.VertexSnap) ----------
	for (TObjectIterator<UStaticMeshComponent> It; It; ++It)
	{
		UStaticMeshComponent* Mesh = *It;
		if (!Mesh || Mesh->GetWorld() != World || !Mesh->IsRegistered())
		{
			continue;
		}
		for (int32 Index = 0; Index < Mesh->GetNumMaterials(); ++Index)
		{
			UMaterialInterface* Current = Mesh->GetMaterial(Index);
			if (!Current || !Current->GetBaseMaterial() || !Current->GetBaseMaterial()->GetName().Equals(TEXT("M_PSX_Surface")))
			{
				continue;
			}
			UMaterialInstanceDynamic* MID = Cast<UMaterialInstanceDynamic>(Current);
			UMaterialInterface* Source = MID ? MID->Parent.Get() : Current;
			if (!MID)
			{
				TWeakObjectPtr<UMaterialInstanceDynamic>& Cached = GWobbleMIDs.FindOrAdd(Source);
				if (!Cached.IsValid())
				{
					float Original = 1.f;
					Source->GetScalarParameterValue(FHashedMaterialParameterInfo(FMaterialParameterInfo(FName(TEXT("VertexSnap")))), Original);
					GWobbleOriginal.Add(Source, Original);
					Cached = UMaterialInstanceDynamic::Create(Source, GetTransientPackage());
				}
				MID = Cached.Get();
				Mesh->SetMaterial(Index, MID);
			}
			const float* Original = GWobbleOriginal.Find(Source);
			MID->SetScalarParameterValue(TEXT("VertexSnap"), bVertexWobble ? (Original ? *Original : 1.f) : 0.f);
		}
	}
}
