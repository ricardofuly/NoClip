// Copyright Fuly Games. All Rights Reserved.

#include "World/NoclipPortraitStudio.h"
#include "Character/NoclipCharacterAnimInstance.h"
#include "Character/NoclipCharacterCatalog.h"
#include "Components/PointLightComponent.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/World.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(NoclipPortraitStudio)

namespace
{
	const FName HeadBone(TEXT("head"));
	/** Distancia da camera ate o rosto e altura do centro da foto acima do osso da cabeca (cm). */
	constexpr float CameraDistance = 62.f;
	constexpr float FaceAboveHeadBone = 8.f;
	/** Luzes a ~1,6 m do rosto (luz mais uniforme que um ponto colado na cara); exposicao manual 1x. */
	constexpr float KeyCandelas = 3.2f;
	constexpr float FillCandelas = 1.2f;
	/** Quadros de "sessao de foto": a pose assenta, congela no meio e as ultimas capturas repetem a mesma imagem
	 *  (a primeira captura de um estudio recem-criado as vezes nao chega na textura). */
	constexpr int32 PhotoWarmupFrames = 14;
	constexpr int32 PhotoFreezeAt = 8;
	int32 GStudioCounter = 0;
}

ANoclipPortraitStudio::ANoclipPortraitStudio()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = false;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;

	Mesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("Mesh"));
	Mesh->SetupAttachment(Root);
	Mesh->SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
	Mesh->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
	Mesh->SetAnimationMode(EAnimationMode::AnimationBlueprint);
	Mesh->AnimClass = UNoclipCharacterAnimInstance::StaticClass();

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeFinder(TEXT("/Engine/BasicShapes/Cube.Cube"));
	Backdrop = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Backdrop"));
	Backdrop->SetupAttachment(Root);
	Backdrop->SetStaticMesh(CubeFinder.Object);
	Backdrop->SetRelativeScale3D(FVector(0.02f, 2.4f, 2.4f));
	Backdrop->SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
	Backdrop->SetCastShadow(false);

	// Flash de foto 3x4: luz forte de frente um pouco acima, e um preenchimento fraco de lado.
	KeyLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("KeyLight"));
	KeyLight->SetupAttachment(Root);
	KeyLight->SetIntensityUnits(ELightUnits::Candelas);
	KeyLight->SetIntensity(18.f);
	KeyLight->SetAttenuationRadius(400.f);
	KeyLight->SetLightColor(FLinearColor(1.f, 0.95f, 0.85f));
	KeyLight->SetCastShadows(false);

	FillLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("FillLight"));
	FillLight->SetupAttachment(Root);
	FillLight->SetIntensityUnits(ELightUnits::Candelas);
	FillLight->SetIntensity(4.f);
	FillLight->SetAttenuationRadius(300.f);
	FillLight->SetLightColor(FLinearColor(0.8f, 0.85f, 1.f));
	FillLight->SetCastShadows(false);

	Capture = CreateDefaultSubobject<USceneCaptureComponent2D>(TEXT("Capture"));
	Capture->SetupAttachment(Root);
	Capture->FOVAngle = 26.f;
	Capture->bCaptureEveryFrame = false;
	Capture->bCaptureOnMovement = false;
	Capture->bAlwaysPersistRenderingState = true;
	Capture->CaptureSource = ESceneCaptureSource::SCS_FinalColorLDR;
	Capture->PrimitiveRenderMode = ESceneCapturePrimitiveRenderMode::PRM_UseShowOnlyList;
	Capture->ShowFlags.SetTemporalAA(false);
	Capture->ShowFlags.SetAntiAliasing(false);
	Capture->ShowFlags.SetMotionBlur(false);
	Capture->ShowFlags.SetAtmosphere(false);
	Capture->ShowFlags.SetFog(false);
	Capture->ShowFlags.SetBloom(false);

	FPostProcessSettings& PP = Capture->PostProcessSettings;
	PP.bOverride_AutoExposureMethod = true;
	PP.AutoExposureMethod = EAutoExposureMethod::AEM_Manual;
	PP.bOverride_AutoExposureApplyPhysicalCameraExposure = true;
	PP.AutoExposureApplyPhysicalCameraExposure = false;
	PP.bOverride_AutoExposureBias = true;
	PP.AutoExposureBias = 0.f;
	PP.bOverride_VignetteIntensity = true;
	PP.VignetteIntensity = 0.f;
	PP.bOverride_ColorSaturation = true;
	PP.ColorSaturation = FVector4(0.85f, 0.85f, 0.85f, 1.f);
	PP.bOverride_ColorGain = true;
	PP.ColorGain = FVector4(1.05f, 1.f, 0.9f, 1.f);
}

ANoclipPortraitStudio* ANoclipPortraitStudio::SpawnStudio(UWorld* World, int32 InSlot)
{
	if (!World || World->GetNetMode() == NM_DedicatedServer)
	{
		return nullptr;
	}
	// Longe do mapa e do carrossel; cada estudio num ponto proprio (as luzes nao se misturam).
	const int32 N = GStudioCounter++ % 16;
	const FVector Location(-150000.f - N * 2000.f, 150000.f, -60000.f);
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ANoclipPortraitStudio* Studio = World->SpawnActor<ANoclipPortraitStudio>(ANoclipPortraitStudio::StaticClass(), FTransform(Location), Params);
	if (Studio)
	{
		Studio->SetCharacter(InSlot);
	}
	return Studio;
}

void ANoclipPortraitStudio::BeginPlay()
{
	Super::BeginPlay();

	// Fundo bege claro (parede de foto de documento).
	if (UMaterialInterface* White = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/NOCLIP/Materials/MI_Loot_White.MI_Loot_White")))
	{
		UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(White, this);
		MID->SetVectorParameterValue(TEXT("Tint"), FLinearColor(0.85f, 0.8f, 0.64f, 1.f));
		MID->SetScalarParameterValue(TEXT("Roughness"), 1.f);
		Backdrop->SetMaterial(0, MID);
	}

	// Luz de foto de documento: principal suave de frente/acima e um preenchimento fraco do outro lado.
	// (Valores aqui e nao no construtor para valerem tambem com Live Coding.)
	KeyLight->SetIntensity(KeyCandelas);
	KeyLight->SetAttenuationRadius(600.f);
	KeyLight->SetSourceRadius(20.f);
	KeyLight->SetSoftSourceRadius(30.f);
	FillLight->SetIntensity(FillCandelas);
	FillLight->SetAttenuationRadius(600.f);
	// Sem GI/reflexo dinamico: com tao pouca luz o Lumen so adiciona ruido na foto.
	FPostProcessSettings& PhotoPP = Capture->PostProcessSettings;
	PhotoPP.bOverride_DynamicGlobalIlluminationMethod = true;
	PhotoPP.DynamicGlobalIlluminationMethod = EDynamicGlobalIlluminationMethod::None;
	PhotoPP.bOverride_ReflectionMethod = true;
	PhotoPP.ReflectionMethod = EReflectionMethod::None;
	Capture->PostProcessSettings.ColorSaturation = FVector4(0.9f, 0.9f, 0.9f, 1.f);
	Capture->PostProcessSettings.bOverride_ColorContrast = true;
	Capture->PostProcessSettings.ColorContrast = FVector4(1.05f, 1.05f, 1.05f, 1.f);

	Target = NewObject<UTextureRenderTarget2D>(this, TEXT("PortraitTarget"));
	Target->RenderTargetFormat = RTF_RGBA8;
	Target->ClearColor = FLinearColor(0.62f, 0.58f, 0.46f, 1.f);
	Target->Filter = TF_Nearest; // pixels grandes, como foto PSX
	Target->InitAutoFormat(TargetWidth, TargetHeight);
	Target->UpdateResourceImmediate(true);
	Capture->TextureTarget = Target;
	Capture->ShowOnlyActors.Add(this);

	if (CharacterSlot >= 0)
	{
		const int32 Slot = CharacterSlot;
		CharacterSlot = -1;
		SetCharacter(Slot);
	}
}

void ANoclipPortraitStudio::SetCharacter(int32 InSlot)
{
	InSlot = FMath::Clamp(InSlot, 0, NoclipCharacters::NumSlots() - 1);
	if (InSlot == CharacterSlot && Mesh->GetSkeletalMeshAsset())
	{
		return;
	}
	CharacterSlot = InSlot;
	if (!HasActorBegunPlay())
	{
		return; // aplica no BeginPlay
	}

	const FNoclipCharacterDef& Def = NoclipCharacters::Get(InSlot);
	Mesh->SetRelativeRotation(FRotator(0.f, Def.MeshYaw, 0.f)); // olhando para +X (a camera)
	Mesh->SetSkeletalMesh(NoclipCharacters::LoadMesh(InSlot), true);
	Mesh->EmptyOverrideMaterials();
	if (Def.bLocked)
	{
		if (UMaterialInterface* Dark = NoclipCharacters::GetSilhouetteMaterial())
		{
			for (int32 m = 0; m < Mesh->GetNumMaterials(); ++m)
			{
				Mesh->SetMaterial(m, Dark);
			}
		}
	}
	if (UNoclipCharacterAnimInstance* Anim = Cast<UNoclipCharacterAnimInstance>(Mesh->GetAnimInstance()))
	{
		Anim->SetCharacter(InSlot);
	}
	// Foto unica: espera a pose assentar, tira a foto e para (ver Tick).
	Mesh->bPauseAnims = false;
	bHasHead = false;
	WarmupFrames = PhotoWarmupFrames;
	SetActorTickEnabled(true);
}

void ANoclipPortraitStudio::UpdateFraming(float DeltaSeconds, bool bSnap)
{
	FVector Head = GetActorLocation() + FVector(0.f, 0.f, 150.f);
	if (Mesh->GetSkeletalMeshAsset() && Mesh->GetBoneIndex(HeadBone) != INDEX_NONE)
	{
		Head = Mesh->GetBoneLocation(HeadBone, EBoneSpaces::WorldSpace);
	}
	// O idle mexe a cabeca: a camera segue devagar (sem tremer).
	SmoothedHead = (bSnap || !bHasHead) ? Head : FMath::VInterpTo(SmoothedHead, Head, DeltaSeconds, 3.f);
	bHasHead = true;

	const FVector Face = SmoothedHead + FVector(0.f, 0.f, FaceAboveHeadBone);
	const FVector Forward = GetActorForwardVector();
	const FVector Right = GetActorRightVector();
	const FVector CamLoc = Face + Forward * CameraDistance + FVector(0.f, 0.f, 2.f);
	// Mira um pouco abaixo do rosto: a cabeca fica no terco de cima (a faixa "Trocar" cobre o pe da foto).
	Capture->SetWorldLocationAndRotation(CamLoc, (Face - FVector(0.f, 0.f, 5.f) - CamLoc).Rotation());
	Backdrop->SetWorldLocation(Face - Forward * 45.f);
	KeyLight->SetWorldLocation(Face + Forward * 150.f + Right * 45.f + FVector(0.f, 0.f, 58.f));
	FillLight->SetWorldLocation(Face + Forward * 140.f - Right * 90.f - FVector(0.f, 0.f, 2.f));
}

void ANoclipPortraitStudio::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!Target || WarmupFrames <= 0)
	{
		SetActorTickEnabled(false);
		return;
	}
	if (!Mesh->bPauseAnims)
	{
		UpdateFraming(DeltaSeconds, true);
	}
	Capture->CaptureScene();
	--WarmupFrames;
	if (WarmupFrames == PhotoFreezeAt)
	{
		// Foto 3x4 estatica: congela a pose; os quadros que faltam so recapturam a mesma imagem.
		Mesh->bPauseAnims = true;
	}
	if (WarmupFrames <= 0)
	{
		SetActorTickEnabled(false);
	}
}

UMaterialInstanceDynamic* ANoclipPortraitStudio::MakeUIMaterial(UObject* InOuter) const
{
	UMaterialInterface* FeedMat = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/NOCLIP/UI/M_UI_CameraFeed.M_UI_CameraFeed"));
	if (!FeedMat || !Target)
	{
		return nullptr;
	}
	UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(FeedMat, InOuter);
	MID->SetTextureParameterValue(TEXT("Feed"), Target);
	return MID;
}
