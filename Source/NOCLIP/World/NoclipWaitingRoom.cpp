// Copyright Fuly Games. All Rights Reserved.

#include "World/NoclipWaitingRoom.h"
#include "World/NoclipElevator.h"
#include "World/NoclipWaitingAvatar.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Components/StaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/StaticMesh.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "DrawDebugHelpers.h"
#include "HAL/IConsoleManager.h"
#include "NOCLIP.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerState.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Net/UnrealNetwork.h"
#include "UObject/ConstructorHelpers.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(NoclipWaitingRoom)

namespace
{
	constexpr float HalfX = 350.f;   // profundidade: camera em -X, elevador em +X
	constexpr float HalfY = 500.f;
	constexpr float RoomHeight = 300.f;
	constexpr float WallT = 20.f;
	constexpr float ElevatorHalfFront = 190.f; // largura da frente do elevador / 2
	constexpr int32 MaxSlots = 6;
}

ANoclipWaitingRoom::ANoclipWaitingRoom()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;
	bAlwaysRelevant = true;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeFinder(TEXT("/Engine/BasicShapes/Cube.Cube"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderFinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> WallFinder(TEXT("/Game/NOCLIP/Materials/MI_L0_Wallpaper.MI_L0_Wallpaper"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> FloorFinder(TEXT("/Game/NOCLIP/Materials/MI_L0_Carpet.MI_L0_Carpet"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> CeilingFinder(TEXT("/Game/NOCLIP/Materials/MI_L0_Ceiling.MI_L0_Ceiling"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> LightFinder(TEXT("/Game/NOCLIP/Materials/MI_L0_LightPanel.MI_L0_LightPanel"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> WhiteFinder(TEXT("/Game/NOCLIP/Materials/MI_Loot_White.MI_Loot_White"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> BlueFinder(TEXT("/Game/NOCLIP/Materials/MI_Loot_Blue.MI_Loot_Blue"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> RedFinder(TEXT("/Game/NOCLIP/Materials/MI_Loot_Red.MI_Loot_Red"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> PPFinder(TEXT("/Game/NOCLIP/Materials/MI_PP_PSX.MI_PP_PSX"));

	CubeMesh = CubeFinder.Object;
	CylinderMesh = CylinderFinder.Object;
	UMaterialInterface* WallMat = WallFinder.Object;
	UMaterialInterface* White = WhiteFinder.Object;
	UMaterialInterface* Blue = BlueFinder.Object;
	UMaterialInterface* Red = RedFinder.Object;

	// ---------- Casca da sala ----------
	MakeBox(TEXT("Floor"), FVector(0.f, 0.f, -5.f), FVector(HalfX * 2.f + WallT * 2.f, HalfY * 2.f + WallT * 2.f, 10.f), FloorFinder.Object, true);
	MakeBox(TEXT("Ceiling"), FVector(0.f, 0.f, RoomHeight + 5.f), FVector(HalfX * 2.f + WallT * 2.f, HalfY * 2.f + WallT * 2.f, 10.f), CeilingFinder.Object);
	MakeBox(TEXT("WallBack"), FVector(-HalfX - WallT * 0.5f, 0.f, RoomHeight * 0.5f), FVector(WallT, HalfY * 2.f, RoomHeight), WallMat);
	MakeBox(TEXT("WallLeft"), FVector(0.f, -HalfY - WallT * 0.5f, RoomHeight * 0.5f), FVector(HalfX * 2.f + WallT * 2.f, WallT, RoomHeight), WallMat);
	MakeBox(TEXT("WallRight"), FVector(0.f, HalfY + WallT * 0.5f, RoomHeight * 0.5f), FVector(HalfX * 2.f + WallT * 2.f, WallT, RoomHeight), WallMat);
	const float SideW = HalfY - ElevatorHalfFront;
	MakeBox(TEXT("WallFrontL"), FVector(HalfX + WallT * 0.5f, -(ElevatorHalfFront + SideW * 0.5f), RoomHeight * 0.5f), FVector(WallT, SideW, RoomHeight), WallMat);
	MakeBox(TEXT("WallFrontR"), FVector(HalfX + WallT * 0.5f, ElevatorHalfFront + SideW * 0.5f, RoomHeight * 0.5f), FVector(WallT, SideW, RoomHeight), WallMat);
	MakeBox(TEXT("WallFrontTop"), FVector(HalfX + WallT * 0.5f, 0.f, (280.f + RoomHeight) * 0.5f), FVector(WallT, ElevatorHalfFront * 2.f, RoomHeight - 280.f), WallMat);

	// Rodape escuro e a faixa do teto (o "friso" do prototipo).
	MakeBox(TEXT("TrimFrontL"), FVector(HalfX - 1.f, -(ElevatorHalfFront + SideW * 0.5f), 6.f), FVector(2.f, SideW, 12.f), Blue);
	MakeBox(TEXT("TrimFrontR"), FVector(HalfX - 1.f, ElevatorHalfFront + SideW * 0.5f, 6.f), FVector(2.f, SideW, 12.f), Blue);
	MakeBox(TEXT("TrimLeft"), FVector(0.f, -HalfY + 1.f, 6.f), FVector(HalfX * 2.f, 2.f, 12.f), Blue);
	MakeBox(TEXT("TrimRight"), FVector(0.f, HalfY - 1.f, 6.f), FVector(HalfX * 2.f, 2.f, 12.f), Blue);

	// Paineis de luz no forro.
	MakeBox(TEXT("PanelA"), FVector(-20.f, 0.f, RoomHeight - 1.f), FVector(60.f, 160.f, 2.f), LightFinder.Object);
	MakeBox(TEXT("PanelB"), FVector(170.f, -300.f, RoomHeight - 1.f), FVector(60.f, 120.f, 2.f), LightFinder.Object);
	MakeBox(TEXT("PanelC"), FVector(170.f, 300.f, RoomHeight - 1.f), FVector(60.f, 120.f, 2.f), LightFinder.Object);

	// ---------- Cadeiras (fileira na parede da esquerda) ----------
	for (int32 i = 0; i < 4; ++i)
	{
		const float X = 20.f + i * 72.f;
		const FString Seat = FString::Printf(TEXT("ChairSeat%d"), i);
		const FString Back = FString::Printf(TEXT("ChairBack%d"), i);
		const FString Base = FString::Printf(TEXT("ChairBase%d"), i);
		MakeBox(*Seat, FVector(X, -HalfY + 34.f, 44.f), FVector(58.f, 52.f, 10.f), Blue);
		MakeBox(*Back, FVector(X, -HalfY + 8.f, 78.f), FVector(58.f, 8.f, 60.f), Blue);
		MakeBox(*Base, FVector(X, -HalfY + 34.f, 20.f), FVector(46.f, 40.f, 38.f), White);
	}
	MakeBox(TEXT("SideTable"), FVector(-80.f, -HalfY + 34.f, 26.f), FVector(52.f, 52.f, 52.f), White);

	// ---------- Planta (canto direito, perto do elevador) ----------
	{
		UStaticMeshComponent* Pot = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PlantPot"));
		Pot->SetupAttachment(Root);
		Pot->SetStaticMesh(CylinderMesh);
		Pot->SetRelativeLocation(FVector(260.f, HalfY - 60.f, 22.f));
		Pot->SetRelativeScale3D(FVector(0.42f, 0.42f, 0.44f));
		Pot->SetMaterial(0, Red);
		Pot->SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
		UStaticMeshComponent* Leaves = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PlantLeaves"));
		Leaves->SetupAttachment(Root);
		Leaves->SetStaticMesh(CylinderMesh);
		Leaves->SetRelativeLocation(FVector(260.f, HalfY - 60.f, 95.f));
		Leaves->SetRelativeScale3D(FVector(0.62f, 0.62f, 1.05f));
		Leaves->SetMaterial(0, Blue);
		Leaves->SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
	}

	// ---------- Relogio (parede do elevador, a esquerda) ----------
	{
		UStaticMeshComponent* Face = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ClockFace"));
		Face->SetupAttachment(Root);
		Face->SetStaticMesh(CylinderMesh);
		Face->SetRelativeLocation(FVector(HalfX - 3.f, -330.f, 215.f));
		Face->SetRelativeRotation(FRotator(90.f, 0.f, 0.f));
		Face->SetRelativeScale3D(FVector(0.44f, 0.44f, 0.05f));
		Face->SetMaterial(0, White);
		Face->SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
		ClockHand = MakeBox(TEXT("ClockHand"), FVector(HalfX - 6.f, -330.f, 215.f), FVector(1.f, 3.f, 18.f), Blue);
	}

	// ---------- Luzes (fluorescente amarelada) ----------
	auto MakeLight = [this](const TCHAR* Name, const FVector& Loc) -> UPointLightComponent*
	{
		UPointLightComponent* L = CreateDefaultSubobject<UPointLightComponent>(Name);
		L->SetupAttachment(Root);
		L->SetRelativeLocation(Loc);
		L->SetIntensityUnits(ELightUnits::Candelas);
		L->SetIntensity(14.f);
		L->SetAttenuationRadius(1100.f);
		L->SetLightColor(FLinearColor(1.f, 0.93f, 0.72f));
		L->SetCastShadows(false);
		return L;
	};
	LightA = MakeLight(TEXT("LightA"), FVector(-120.f, 0.f, RoomHeight - 30.f));
	LightB = MakeLight(TEXT("LightB"), FVector(180.f, 0.f, RoomHeight - 30.f));

	// ---------- Camera de seguranca (canto do teto, parede da camera) ----------
	SecurityCamera = CreateDefaultSubobject<USceneCaptureComponent2D>(TEXT("SecurityCamera"));
	SecurityCamera->SetupAttachment(Root);
	SecurityCamera->SetRelativeLocation(FVector(-HalfX + 24.f, 0.f, RoomHeight - 34.f));
	SecurityCamera->SetRelativeRotation(FRotator(-16.f, 0.f, 0.f));
	SecurityCamera->FOVAngle = CameraFOV;
	SecurityCamera->bCaptureEveryFrame = false;
	SecurityCamera->bCaptureOnMovement = false;
	SecurityCamera->bAlwaysPersistRenderingState = true;
	SecurityCamera->CaptureSource = ESceneCaptureSource::SCS_FinalColorLDR;
	SecurityCamera->ShowFlags.SetTemporalAA(false);
	SecurityCamera->ShowFlags.SetAntiAliasing(false);
	SecurityCamera->ShowFlags.SetMotionBlur(false);

	// CFTV antigo: dessaturado, puxado para o sepia, contraste alto (filtro do prototipo) e exposicao fixa.
	FPostProcessSettings& PP = SecurityCamera->PostProcessSettings;
	PP.bOverride_ColorSaturation = true;
	PP.ColorSaturation = FVector4(0.3f, 0.3f, 0.3f, 1.f);
	PP.bOverride_ColorContrast = true;
	PP.ColorContrast = FVector4(1.15f, 1.15f, 1.15f, 1.f);
	PP.bOverride_ColorGain = true;
	PP.ColorGain = FVector4(1.1f, 1.0f, 0.82f, 1.f);
	PP.bOverride_VignetteIntensity = true;
	PP.VignetteIntensity = 0.9f;
	PP.bOverride_AutoExposureMethod = true;
	PP.AutoExposureMethod = EAutoExposureMethod::AEM_Manual;
	PP.bOverride_AutoExposureApplyPhysicalCameraExposure = true;
	PP.AutoExposureApplyPhysicalCameraExposure = false;
	PP.bOverride_AutoExposureBias = true;
	PP.AutoExposureBias = 0.f;
	PP.bOverride_BloomIntensity = true;
	PP.BloomIntensity = 0.4f;
	PP.bOverride_FilmGrainIntensity = true;
	PP.FilmGrainIntensity = 0.35f;
	if (PPFinder.Object)
	{
		PP.WeightedBlendables.Array.Add(FWeightedBlendable(1.f, PPFinder.Object));
	}
}

UStaticMeshComponent* ANoclipWaitingRoom::MakeBox(const TCHAR* Name, const FVector& Center, const FVector& Size, UMaterialInterface* Material, bool bCollide)
{
	UStaticMeshComponent* C = CreateDefaultSubobject<UStaticMeshComponent>(Name);
	C->SetupAttachment(Root);
	C->SetStaticMesh(CubeMesh);
	C->SetRelativeLocation(Center);
	C->SetRelativeScale3D(Size / 100.f);
	if (Material)
	{
		C->SetMaterial(0, Material);
	}
	C->SetCollisionProfileName(bCollide ? UCollisionProfile::BlockAll_ProfileName : UCollisionProfile::NoCollision_ProfileName);
	C->SetCanEverAffectNavigation(false);
	return C;
}

void ANoclipWaitingRoom::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ANoclipWaitingRoom, Elevator);
	DOREPLIFETIME(ANoclipWaitingRoom, Phase);
	DOREPLIFETIME(ANoclipWaitingRoom, PhaseServerTime);
}

void ANoclipWaitingRoom::BeginPlay()
{
	Super::BeginPlay();

	if (HasAuthority() && !Elevator)
	{
		FActorSpawnParameters Params;
		Params.Owner = this;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		const FTransform T(FRotator(0.f, 180.f, 0.f), FVector(HalfX, 0.f, 0.f));
		Elevator = GetWorld()->SpawnActor<ANoclipElevator>(ANoclipElevator::StaticClass(), T * GetActorTransform(), Params);
		if (Elevator)
		{
			Elevator->SetFloorLabel(TEXT("T"));
			Elevator->SetDoorsOpen(false, true);
		}
		PhaseServerTime = GetServerNow();
	}

	// Imagem da camera: so quem tem tela precisa.
	if (GetNetMode() != NM_DedicatedServer && SecurityCamera)
	{
		CameraTarget = NewObject<UTextureRenderTarget2D>(this, TEXT("SecurityCameraTarget"));
		CameraTarget->RenderTargetFormat = RTF_RGBA8;
		CameraTarget->ClearColor = FLinearColor(0.04f, 0.035f, 0.03f, 1.f);
		CameraTarget->InitAutoFormat(480, 270);
		CameraTarget->UpdateResourceImmediate(true);
		SecurityCamera->TextureTarget = CameraTarget;
		SecurityCamera->FOVAngle = CameraFOV;

		// PSX com menos linhas (a imagem ja e pequena).
		for (FWeightedBlendable& B : SecurityCamera->PostProcessSettings.WeightedBlendables.Array)
		{
			if (UMaterialInterface* Mat = Cast<UMaterialInterface>(B.Object))
			{
				UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(Mat, this);
				MID->SetScalarParameterValue(TEXT("VerticalResolution"), 200.f);
				MID->SetScalarParameterValue(TEXT("DitherAmount"), 0.6f);
				B.Object = MID;
			}
		}
	}
}

void ANoclipWaitingRoom::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (HasAuthority())
	{
		for (ANoclipWaitingAvatar* A : Avatars)
		{
			if (IsValid(A))
			{
				A->Destroy();
			}
		}
		Avatars.Reset();
		if (IsValid(Elevator))
		{
			Elevator->Destroy();
		}
	}
	Super::EndPlay(EndPlayReason);
}

double ANoclipWaitingRoom::GetServerNow() const
{
	const UWorld* World = GetWorld();
	const AGameStateBase* GS = World ? World->GetGameState() : nullptr;
	return GS ? GS->GetServerWorldTimeSeconds() : (World ? World->GetTimeSeconds() : 0.0);
}

float ANoclipWaitingRoom::GetPhaseElapsed() const
{
	return FMath::Max(0.f, static_cast<float>(GetServerNow() - PhaseServerTime));
}

void ANoclipWaitingRoom::SetPhase(ENoclipWaitingPhase NewPhase)
{
	if (HasAuthority())
	{
		Phase = NewPhase;
		PhaseServerTime = GetServerNow();
	}
}

ANoclipWaitingAvatar* ANoclipWaitingRoom::AddAvatar(APlayerState* ForPlayer)
{
	if (!HasAuthority() || !ForPlayer)
	{
		return nullptr;
	}
	Avatars.RemoveAll([](const TObjectPtr<ANoclipWaitingAvatar>& A) { return !IsValid(A); });
	for (ANoclipWaitingAvatar* A : Avatars)
	{
		if (A->GetPlayerStateRef() == ForPlayer)
		{
			return A;
		}
	}
	int32 FreeSlot = INDEX_NONE;
	for (int32 Slot = 0; Slot < MaxSlots && FreeSlot == INDEX_NONE; ++Slot)
	{
		const bool bUsed = Avatars.ContainsByPredicate([Slot](const TObjectPtr<ANoclipWaitingAvatar>& A) { return A->GetSlot() == Slot; });
		if (!bUsed)
		{
			FreeSlot = Slot;
		}
	}
	if (FreeSlot == INDEX_NONE)
	{
		return nullptr;
	}

	FVector Loc;
	FRotator Rot;
	bool bWalking = false, bInCabin = false;
	EvaluateAvatar(FreeSlot, Loc, Rot, bWalking, bInCabin);

	FActorSpawnParameters Params;
	Params.Owner = this;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ANoclipWaitingAvatar* Avatar = GetWorld()->SpawnActorDeferred<ANoclipWaitingAvatar>(ANoclipWaitingAvatar::StaticClass(), FTransform(Rot, Loc), this, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (Avatar)
	{
		Avatar->Setup(this, ForPlayer, FreeSlot);
		Avatar->FinishSpawning(FTransform(Rot, Loc));
		Avatars.Add(Avatar);
	}
	return Avatar;
}

void ANoclipWaitingRoom::RemoveAvatar(APlayerState* ForPlayer)
{
	if (!HasAuthority())
	{
		return;
	}
	for (int32 i = Avatars.Num() - 1; i >= 0; --i)
	{
		if (!IsValid(Avatars[i]) || Avatars[i]->GetPlayerStateRef() == ForPlayer)
		{
			if (IsValid(Avatars[i]))
			{
				Avatars[i]->Destroy();
			}
			Avatars.RemoveAt(i);
		}
	}
}

namespace NoclipWaitingWalk
{
	constexpr float Ramp = 0.4f;     // segundos para arrancar / parar
	constexpr float DoorGap = 0.6f;  // segundos entre um passar a porta e o proximo

	/**
	 * Rotas no espaco da sala (camera em -X, porta do elevador em X = 350, cabine de 350 a 650).
	 * Cada lugar tem a sua faixa na porta (+55 / 0 / -55) e o seu lugar na cabine, na mesma ordem
	 * lateral dos lugares de espera: as rotas nao se cruzam. O fundo da cabine entra primeiro.
	 */
	struct FRouteDef
	{
		FVector2D Idle;
		TArray<FVector2D> Via; // pontos de passagem (o ultimo e o lugar na cabine)
		int32 Row;
		int32 DoorIndex;       // qual ponto de Via e a porta
	};

	const FRouteDef& Def(int32 Slot)
	{
		static const FRouteDef Defs[MaxSlots] = {
			// 0: meio, frente
			{ FVector2D(170.f, -30.f),  { FVector2D(255.f, -8.f), FVector2D(350.f, 0.f), FVector2D(405.f, 0.f), FVector2D(460.f, 0.f) }, 1, 1 },
			// 1: direita da camera, frente
			{ FVector2D(215.f, 120.f),  { FVector2D(280.f, 72.f), FVector2D(350.f, 55.f), FVector2D(405.f, 62.f), FVector2D(460.f, 72.f) }, 1, 1 },
			// 2: esquerda, frente
			{ FVector2D(70.f, -200.f),  { FVector2D(185.f, -145.f), FVector2D(280.f, -72.f), FVector2D(350.f, -55.f), FVector2D(405.f, -62.f), FVector2D(460.f, -72.f) }, 1, 2 },
			// 3: bem a direita, fundo (contorna o lugar 1 por fora)
			{ FVector2D(110.f, 260.f),  { FVector2D(250.f, 195.f), FVector2D(305.f, 95.f), FVector2D(350.f, 55.f), FVector2D(430.f, 78.f), FVector2D(510.f, 106.f), FVector2D(570.f, 110.f) }, 0, 2 },
			// 4: bem a esquerda, fundo
			{ FVector2D(-10.f, -340.f), { FVector2D(150.f, -265.f), FVector2D(290.f, -95.f), FVector2D(350.f, -55.f), FVector2D(430.f, -78.f), FVector2D(510.f, -106.f), FVector2D(570.f, -110.f) }, 0, 2 },
			// 5: meio de tras, fundo (passa entre os lugares 0 e 1)
			{ FVector2D(10.f, 70.f),    { FVector2D(130.f, 58.f), FVector2D(262.f, 14.f), FVector2D(350.f, 0.f), FVector2D(450.f, 0.f), FVector2D(570.f, 0.f) }, 0, 2 },
		};
		return Defs[FMath::Abs(Slot) % MaxSlots];
	}

	float PathLength(const TArray<FVector>& P)
	{
		float L = 0.f;
		for (int32 i = 0; i + 1 < P.Num(); ++i)
		{
			L += FVector::Dist2D(P[i], P[i + 1]);
		}
		return L;
	}

	void SamplePath(const TArray<FVector>& P, float Distance, FVector& OutLoc, FVector& OutDir)
	{
		OutLoc = P.Num() > 0 ? P[0] : FVector::ZeroVector;
		OutDir = FVector::ZeroVector;
		for (int32 i = 0; i + 1 < P.Num(); ++i)
		{
			const float Seg = FVector::Dist2D(P[i], P[i + 1]);
			if (Seg < KINDA_SMALL_NUMBER)
			{
				continue;
			}
			OutDir = (P[i + 1] - P[i]).GetSafeNormal2D();
			if (Distance <= Seg || i + 2 == P.Num())
			{
				OutLoc = FMath::Lerp(P[i], P[i + 1], FMath::Clamp(Distance / Seg, 0.f, 1.f));
				return;
			}
			Distance -= Seg;
		}
		if (P.Num() > 0)
		{
			OutLoc = P.Last();
		}
	}
}

static TAutoConsoleVariable<int32> CVarNoclipDebugWaitingWalk(TEXT("noclip.DebugWaitingWalk"), 0,
	TEXT("Sala de espera: desenha as rotas do elevador (1) e avisa no log quando um boneco pula (2)."));

FVector ANoclipWaitingRoom::GetIdleSpot(int32 Slot) const
{
	// Espalhados na frente do elevador, como no quadro da camera do prototipo.
	const FVector2D S = NoclipWaitingWalk::Def(Slot).Idle;
	return GetActorTransform().TransformPosition(FVector(S.X, S.Y, 0.f));
}

const ANoclipWaitingRoom::FBoardRoute& ANoclipWaitingRoom::GetRoute(int32 Slot) const
{
	if (RouteCache.Num() != MaxSlots)
	{
		RouteCache.SetNum(MaxSlots);
		const FTransform Room = GetActorTransform();
		for (int32 Index = 0; Index < MaxSlots; ++Index)
		{
			const NoclipWaitingWalk::FRouteDef& D = NoclipWaitingWalk::Def(Index);
			FBoardRoute& R = RouteCache[Index];
			R.Row = D.Row;

			// Spline (Catmull-Rom automatica) pelos pontos, com a chave = distancia em linha reta.
			FInterpCurveVector Curve;
			float Key = 0.f;
			FVector2D Prev = D.Idle;
			Curve.AddPoint(0.f, FVector(D.Idle, 0.f));
			TArray<float> Keys;
			for (const FVector2D& V : D.Via)
			{
				Key += FVector2D::Distance(Prev, V);
				Keys.Add(Key);
				Curve.AddPoint(Key, FVector(V, 0.f));
				Prev = V;
			}
			for (FInterpCurvePoint<FVector>& Pt : Curve.Points)
			{
				Pt.InterpMode = CIM_CurveAuto;
			}
			Curve.AutoSetTangents(0.f, false);

			const int32 Steps = FMath::Max(8, FMath::CeilToInt(Key / 8.f));
			R.Points.Reset(Steps + 1);
			const float DoorKey = Keys.IsValidIndex(D.DoorIndex) ? Keys[D.DoorIndex] : Key;
			float Length = 0.f;
			R.DoorDistance = -1.f;
			for (int32 i = 0; i <= Steps; ++i)
			{
				const float K = Key * i / Steps;
				const FVector W = Room.TransformPosition(Curve.Eval(K, FVector::ZeroVector));
				if (R.Points.Num() > 0)
				{
					Length += FVector::Dist2D(R.Points.Last(), W);
				}
				if (R.DoorDistance < 0.f && K >= DoorKey)
				{
					R.DoorDistance = Length;
				}
				R.Points.Add(W);
			}
			R.Length = Length;
			if (R.DoorDistance < 0.f)
			{
				R.DoorDistance = Length;
			}
		}
	}
	return RouteCache[FMath::Abs(Slot) % MaxSlots];
}

float ANoclipWaitingRoom::GetWalkDuration(float PathLength) const
{
	const float V = FMath::Max(WalkSpeed, 1.f);
	const float A = FMath::Min(NoclipWaitingWalk::Ramp, PathLength / V);
	return PathLength / V + A;
}

float ANoclipWaitingRoom::GetTimeAtDistance(float Distance, float PathLength) const
{
	const float V = FMath::Max(WalkSpeed, 1.f);
	const float A = FMath::Min(NoclipWaitingWalk::Ramp, PathLength / V);
	const float T = GetWalkDuration(PathLength);
	Distance = FMath::Clamp(Distance, 0.f, PathLength);
	const float RampDist = 0.5f * V * A;
	if (A <= KINDA_SMALL_NUMBER)
	{
		return Distance / V;
	}
	if (Distance < RampDist)
	{
		return FMath::Sqrt(2.f * Distance * A / V);
	}
	if (Distance <= PathLength - RampDist)
	{
		return A + (Distance - RampDist) / V;
	}
	return T - FMath::Sqrt(FMath::Max(0.f, 2.f * (PathLength - Distance) * A / V));
}

uint8 ANoclipWaitingRoom::GetLiveSlotMask() const
{
	uint8 Mask = 0;
	for (TActorIterator<ANoclipWaitingAvatar> It(GetWorld()); It; ++It)
	{
		if (IsValid(*It) && !It->IsActorBeingDestroyed() && It->GetSlot() >= 0 && It->GetSlot() < MaxSlots)
		{
			Mask |= 1 << It->GetSlot();
		}
	}
	return Mask;
}

void ANoclipWaitingRoom::ComputeSchedule(uint8 Mask, float OutStart[6]) const
{
	TArray<int32, TInlineAllocator<MaxSlots>> Order;
	for (int32 Slot = 0; Slot < MaxSlots; ++Slot)
	{
		OutStart[Slot] = WalkStartDelay + Slot * WalkStagger;
		if (Mask & (1 << Slot))
		{
			Order.Add(Slot);
		}
	}
	// Fundo da cabine primeiro (senao quem fica na frente tampa a passagem); empate: quem esta mais perto da porta.
	Order.Sort([this](int32 A, int32 B)
	{
		const FBoardRoute& RA = GetRoute(A);
		const FBoardRoute& RB = GetRoute(B);
		return RA.Row != RB.Row ? RA.Row < RB.Row : RA.DoorDistance < RB.DoorDistance;
	});
	// Um de cada vez na porta, com folga: ninguem atropela ninguem.
	float LastDoor = -BIG_NUMBER;
	for (int32 i = 0; i < Order.Num(); ++i)
	{
		const int32 Slot = Order[i];
		const FBoardRoute& R = GetRoute(Slot);
		const float ToDoor = GetTimeAtDistance(R.DoorDistance, R.Length);
		const float Start = FMath::Max(WalkStartDelay + i * WalkStagger, LastDoor + NoclipWaitingWalk::DoorGap - ToDoor);
		OutStart[Slot] = Start;
		LastDoor = Start + ToDoor;
	}
}

float ANoclipWaitingRoom::GetBoardClock() const
{
	if (BoardClockFrame == GFrameCounter)
	{
		return BoardClock;
	}
	const bool bFirst = BoardClockFrame == MAX_uint64;
	BoardClockFrame = GFrameCounter;
	const UWorld* World = GetWorld();
	const float Dt = World ? World->GetDeltaSeconds() : 0.f;

	// Ordem de entrada: acompanha quem esta na sala enquanto espera; congela quando as portas abrem.
	if (Phase == ENoclipWaitingPhase::Gathering || !bScheduleValid || BoardClockPhase == ENoclipWaitingPhase::Gathering)
	{
		ComputeSchedule(GetLiveSlotMask(), SlotStart);
		bScheduleValid = true;
	}

	switch (Phase)
	{
	case ENoclipWaitingPhase::Gathering:
		BoardClock = 0.f;
		break;
	case ENoclipWaitingPhase::Boarding:
	{
		// O relogio do servidor no cliente da saltos a cada correcao: aqui so serve de referencia.
		const float Target = GetPhaseElapsed();
		if (BoardClockPhase != ENoclipWaitingPhase::Boarding)
		{
			BoardClock = Target;
		}
		else
		{
			BoardClock += Dt;
			const float Err = Target - BoardClock;
			BoardClock = FMath::Abs(Err) > 1.f ? Target : BoardClock + Err * FMath::Min(1.f, Dt * 0.5f);
		}
		break;
	}
	case ENoclipWaitingPhase::Departing:
		// Continua de onde o embarque parou (sem pular para a cabine); quem chegou ja descendo comeca la dentro.
		BoardClock = (bFirst || BoardClockPhase == ENoclipWaitingPhase::Gathering) ? 1.0e5f : BoardClock + Dt;
		break;
	}
	BoardClockPhase = Phase;
	return BoardClock;
}

void ANoclipWaitingRoom::EvaluateAvatar(int32 Slot, FVector& OutLocation, FRotator& OutRotation, bool& bOutWalking, bool& bOutInCabin, float* OutSpeed) const
{
	const FVector Idle = GetIdleSpot(Slot);
	// Olhando para o elevador, cada um com um angulo um pouco diferente.
	const FRotator IdleRot(0.f, GetActorRotation().Yaw + FMath::Sin(Slot * 1.7f) * 25.f, 0.f);

	bOutWalking = false;
	bOutInCabin = false;
	OutLocation = Idle;
	OutRotation = IdleRot;
	if (OutSpeed)
	{
		*OutSpeed = 0.f;
	}

	if (Phase == ENoclipWaitingPhase::Gathering || !Elevator)
	{
		return;
	}

	const FBoardRoute& Route = GetRoute(Slot);
	const float Clock = GetBoardClock();
	const float Total = Route.Length;
	const float V = FMath::Max(WalkSpeed, 1.f);
	const float A = FMath::Min(NoclipWaitingWalk::Ramp, Total / V);
	const float Duration = GetWalkDuration(Total);
	const float T = Clock - SlotStart[FMath::Clamp(Slot, 0, MaxSlots - 1)];
	const FRotator CabinRot(0.f, Elevator->GetActorRotation().Yaw, 0.f); // de frente para a porta (para a camera)

	if (T <= 0.f)
	{
		return;
	}
	if (T >= Duration || Total < 1.f)
	{
		OutLocation = Route.Points.Num() > 0 ? Route.Points.Last() : Idle;
		OutRotation = CabinRot;
		bOutInCabin = true;
		return;
	}

	// Perfil trapezoidal: acelera, anda, freia (sem largar ou parar de uma vez).
	float S, Speed;
	if (T < A)                 { S = 0.5f * V * T * T / A;                                       Speed = V * T / A; }
	else if (T < Duration - A) { S = 0.5f * V * A + V * (T - A);                                 Speed = V; }
	else                       { const float R = Duration - T; S = Total - 0.5f * V * R * R / A; Speed = V * R / A; }

	FVector Dir;
	NoclipWaitingWalk::SamplePath(Route.Points, S, OutLocation, Dir);
	OutRotation = Dir.IsNearlyZero() ? CabinRot : Dir.Rotation();
	bOutWalking = true;
	// Passou da porta: ja conta como dentro (a camera para de rastrear quando a porta fecha).
	bOutInCabin = S >= Route.DoorDistance;
	if (OutSpeed)
	{
		*OutSpeed = Speed;
	}
}

float ANoclipWaitingRoom::GetBoardingFinishSeconds() const
{
	float Start[MaxSlots];
	const uint8 Mask = GetLiveSlotMask();
	ComputeSchedule(Mask, Start);
	float Finish = WalkStartDelay;
	for (int32 Slot = 0; Slot < MaxSlots; ++Slot)
	{
		if (Mask & (1 << Slot))
		{
			Finish = FMath::Max(Finish, Start[Slot] + GetWalkDuration(GetRoute(Slot).Length));
		}
	}
	return Finish;
}

bool ANoclipWaitingRoom::ProjectToCamera(const FVector& World, FVector2D& OutUV) const
{
	if (!SecurityCamera)
	{
		return false;
	}
	const FVector L = SecurityCamera->GetComponentTransform().InverseTransformPosition(World);
	if (L.X < 1.f)
	{
		return false;
	}
	const float TanHalf = FMath::Tan(FMath::DegreesToRadians(SecurityCamera->FOVAngle * 0.5f));
	const float Aspect = 16.f / 9.f;
	OutUV.X = 0.5f + (L.Y / L.X) / (2.f * TanHalf);
	OutUV.Y = 0.5f - (L.Z / L.X) * Aspect / (2.f * TanHalf);
	return true;
}

void ANoclipWaitingRoom::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	// Ponteiro do relogio: um giro por minuto (cada maquina no seu proprio relogio).
	if (ClockHand)
	{
		const float Angle = FMath::Fmod(static_cast<float>(GetServerNow()) * 6.f, 360.f);
		const FVector Center(HalfX - 6.f, -330.f, 215.f);
		const FQuat Q(FVector::ForwardVector, FMath::DegreesToRadians(Angle));
		ClockHand->SetRelativeLocation(Center + Q.RotateVector(FVector(0.f, 0.f, 8.f)));
		ClockHand->SetRelativeRotation(Q);
	}

	if (CVarNoclipDebugWaitingWalk.GetValueOnGameThread() & 1)
	{
		static const FColor Colors[MaxSlots] = { FColor::Yellow, FColor::Cyan, FColor::Green, FColor::Magenta, FColor::Orange, FColor::White };
		for (int32 Slot = 0; Slot < MaxSlots; ++Slot)
		{
			const FBoardRoute& R = GetRoute(Slot);
			for (int32 i = 0; i + 1 < R.Points.Num(); ++i)
			{
				DrawDebugLine(GetWorld(), R.Points[i] + FVector(0, 0, 3), R.Points[i + 1] + FVector(0, 0, 3), Colors[Slot], false, -1.f, 0, 2.f);
			}
			if (R.Points.Num() > 0)
			{
				DrawDebugCircle(GetWorld(), R.Points.Last() + FVector(0, 0, 3), 25.f, 16, Colors[Slot], false, -1.f, 0, 1.5f, FVector::YAxisVector, FVector::XAxisVector);
			}
		}
	}

	if (CameraTarget && SecurityCamera && CameraFPS > 0.f)
	{
		// Cadencia fixa (zerar o acumulador deixava 4 e 5 quadros alternados: o boneco "pulava").
		const float Interval = 1.f / CameraFPS;
		CaptureAccumulator = FMath::Min(CaptureAccumulator + DeltaSeconds, Interval * 2.f);
		if (CaptureAccumulator >= Interval)
		{
			CaptureAccumulator -= Interval;
			SecurityCamera->CaptureScene();
			++CaptureCount;
			// As caixas de rastreio usam a posicao desta imagem (senao andam lisas enquanto a imagem anda aos saltos).
			for (TActorIterator<ANoclipWaitingAvatar> It(GetWorld()); It; ++It)
			{
				It->NoteCaptured();
			}
		}
	}
}
