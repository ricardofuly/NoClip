// Copyright Fuly Games. All Rights Reserved.

#include "World/NoclipCharacterStage.h"
#include "Character/NoclipCharacterAnimInstance.h"
#include "Character/NoclipCharacterCatalog.h"
#include "Components/PointLightComponent.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/SpotLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(NoclipCharacterStage)

namespace
{
	constexpr float RingRadius = 170.f;
	constexpr float CameraDistance = 480.f; // da frente do carrossel ate a camera
	constexpr float CameraHeight = 100.f;
	constexpr float CaptureFPS = 30.f;
	/** Leve 3/4 do escolhido (graus), como uma foto de ficha. */
	constexpr float FrontTurn = -14.f;
}

ANoclipCharacterStage::ANoclipCharacterStage()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = false;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;

	Ring = CreateDefaultSubobject<USceneComponent>(TEXT("Ring"));
	Ring->SetupAttachment(Root);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderFinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> CarpetFinder(TEXT("/Game/NOCLIP/Materials/MI_L0_Carpet.MI_L0_Carpet"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> WhiteFinder(TEXT("/Game/NOCLIP/Materials/MI_Loot_White.MI_Loot_White"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> PPFinder(TEXT("/Game/NOCLIP/Materials/MI_PP_PSX.MI_PP_PSX"));

	// Pedestal: disco de carpete (o cilindro basico tem 100 x 100 x 100, centro no meio).
	Turntable = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Turntable"));
	Turntable->SetupAttachment(Ring);
	Turntable->SetStaticMesh(CylinderFinder.Object);
	Turntable->SetRelativeScale3D(FVector(5.6f, 5.6f, 0.08f));
	Turntable->SetRelativeLocation(FVector(0.f, 0.f, -4.f));
	Turntable->SetMaterial(0, CarpetFinder.Object);
	Turntable->SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);

	TurntableRim = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("TurntableRim"));
	TurntableRim->SetupAttachment(Root);
	TurntableRim->SetStaticMesh(CylinderFinder.Object);
	TurntableRim->SetRelativeScale3D(FVector(5.9f, 5.9f, 0.06f));
	TurntableRim->SetRelativeLocation(FVector(0.f, 0.f, -6.f));
	TurntableRim->SetMaterial(0, WhiteFinder.Object);
	TurntableRim->SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);

	// Luz principal: fluorescente amarelada de cima, mirando quem esta na frente.
	KeyLight = CreateDefaultSubobject<USpotLightComponent>(TEXT("KeyLight"));
	KeyLight->SetupAttachment(Root);
	KeyLight->SetRelativeLocation(FVector(RingRadius + 160.f, 0.f, 420.f));
	KeyLight->SetRelativeRotation((FVector(RingRadius, 0.f, 80.f) - FVector(RingRadius + 160.f, 0.f, 420.f)).Rotation());
	KeyLight->SetIntensityUnits(ELightUnits::Candelas);
	KeyLight->SetIntensity(60.f);
	KeyLight->SetAttenuationRadius(1200.f);
	KeyLight->SetInnerConeAngle(14.f);
	KeyLight->SetOuterConeAngle(30.f);
	KeyLight->SetLightColor(FLinearColor(1.f, 0.9f, 0.68f));
	KeyLight->SetCastShadows(true);

	FillLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("FillLight"));
	FillLight->SetupAttachment(Root);
	FillLight->SetRelativeLocation(FVector(RingRadius + CameraDistance * 0.6f, -120.f, 160.f));
	FillLight->SetIntensityUnits(ELightUnits::Candelas);
	FillLight->SetIntensity(4.f);
	FillLight->SetAttenuationRadius(900.f);
	FillLight->SetLightColor(FLinearColor(0.75f, 0.8f, 1.f));
	FillLight->SetCastShadows(false);

	// Camera: na frente do carrossel, olhando para o escolhido.
	Capture = CreateDefaultSubobject<USceneCaptureComponent2D>(TEXT("Capture"));
	Capture->SetupAttachment(Root);
	const FVector CamLoc(RingRadius + CameraDistance, 0.f, CameraHeight);
	Capture->SetRelativeLocation(CamLoc);
	Capture->SetRelativeRotation((FVector(RingRadius, 0.f, 72.f) - CamLoc).Rotation());
	Capture->FOVAngle = 50.f;
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

	FPostProcessSettings& PP = Capture->PostProcessSettings;
	PP.bOverride_AutoExposureMethod = true;
	PP.AutoExposureMethod = EAutoExposureMethod::AEM_Manual;
	PP.bOverride_AutoExposureApplyPhysicalCameraExposure = true;
	PP.AutoExposureApplyPhysicalCameraExposure = false;
	PP.bOverride_AutoExposureBias = true;
	PP.AutoExposureBias = 0.f;
	PP.bOverride_VignetteIntensity = true;
	PP.VignetteIntensity = 0.7f;
	PP.bOverride_BloomIntensity = true;
	PP.BloomIntensity = 0.3f;
	PP.bOverride_ColorGain = true;
	PP.ColorGain = FVector4(1.04f, 1.0f, 0.92f, 1.f);
	if (PPFinder.Object)
	{
		PP.WeightedBlendables.Array.Add(FWeightedBlendable(1.f, PPFinder.Object));
	}
}

void ANoclipCharacterStage::BeginPlay()
{
	Super::BeginPlay();

	const int32 N = NoclipCharacters::NumSlots();
	const float Step = 360.f / FMath::Max(N, 1);
	for (int32 i = 0; i < N; ++i)
	{
		const FNoclipCharacterDef& Def = NoclipCharacters::Get(i);
		USkeletalMeshComponent* M = NewObject<USkeletalMeshComponent>(this, *FString::Printf(TEXT("Slot%d"), i));
		M->SetupAttachment(Ring);
		const float A = FMath::DegreesToRadians(i * Step);
		M->SetRelativeLocation(FVector(FMath::Cos(A) * RingRadius, FMath::Sin(A) * RingRadius, 0.f));
		M->SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
		M->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
		M->SetAnimationMode(EAnimationMode::AnimationBlueprint);
		M->AnimClass = UNoclipCharacterAnimInstance::StaticClass();
		M->SetSkeletalMesh(NoclipCharacters::LoadMesh(i), true);
		M->RegisterComponent();
		if (Def.bLocked)
		{
			if (UMaterialInterface* Dark = NoclipCharacters::GetSilhouetteMaterial())
			{
				for (int32 m = 0; m < M->GetNumMaterials(); ++m)
				{
					M->SetMaterial(m, Dark);
				}
			}
		}
		if (UNoclipCharacterAnimInstance* Anim = Cast<UNoclipCharacterAnimInstance>(M->GetAnimInstance()))
		{
			Anim->SetCharacter(i);
			Anim->SetPhaseOffset(i * 1.9f);
		}
		Slots.Add(M);
	}

	if (Capture)
	{
		Capture->ShowOnlyActors.Add(this);

		Target = NewObject<UTextureRenderTarget2D>(this, TEXT("StageTarget"));
		Target->RenderTargetFormat = RTF_RGBA8;
		Target->ClearColor = FLinearColor(0.007f, 0.006f, 0.004f, 1.f);
		Target->InitAutoFormat(static_cast<int32>(CaptureWidth * 0.75f), static_cast<int32>(CaptureHeight * 0.75f));
		Target->UpdateResourceImmediate(true);
		Capture->TextureTarget = Target;

		for (FWeightedBlendable& B : Capture->PostProcessSettings.WeightedBlendables.Array)
		{
			if (UMaterialInterface* Mat = Cast<UMaterialInterface>(B.Object))
			{
				UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(Mat, this);
				MID->SetScalarParameterValue(TEXT("VerticalResolution"), 300.f);
				MID->SetScalarParameterValue(TEXT("DitherAmount"), 0.5f);
				B.Object = MID;
			}
		}
	}
	RingYaw = -Position * Step;
}

void ANoclipCharacterStage::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	const int32 N = FMath::Max(Slots.Num(), 1);
	const float Step = 360.f / N;

	// Gira ate o escolhido (mola criticamente amortecida, sem passar do ponto).
	const float TargetYaw = -Position * Step;
	RingYaw = FMath::FInterpTo(RingYaw, TargetYaw, DeltaSeconds, 7.f);
	Ring->SetRelativeRotation(FRotator(0.f, RingYaw, 0.f));

	// Cada estagiario olha para a camera (como os cartoes do prototipo), o da frente em 3/4.
	const FVector CamWorld = Capture ? Capture->GetComponentLocation() : GetActorLocation() + GetActorForwardVector() * 1000.f;
	const int32 Selected = ((Position % N) + N) % N;
	for (int32 i = 0; i < Slots.Num(); ++i)
	{
		USkeletalMeshComponent* M = Slots[i];
		if (!M)
		{
			continue;
		}
		const FVector ToCam = (CamWorld - M->GetComponentLocation()).GetSafeNormal2D();
		const float Face = ToCam.Rotation().Yaw + (i == Selected ? FrontTurn : 0.f);
		M->SetWorldRotation(FRotator(0.f, Face + NoclipCharacters::Get(i).MeshYaw, 0.f));
	}

	if (Capture && Target)
	{
		CaptureAccumulator += DeltaSeconds;
		if (CaptureAccumulator >= 1.f / CaptureFPS)
		{
			CaptureAccumulator = 0.f;
			Capture->CaptureScene();
		}
	}
}
