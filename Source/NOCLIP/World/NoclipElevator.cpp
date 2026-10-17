// Copyright Fuly Games. All Rights Reserved.

#include "World/NoclipElevator.h"
#include "Components/StaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"
#include "Materials/MaterialInterface.h"
#include "Net/UnrealNetwork.h"
#include "UObject/ConstructorHelpers.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(NoclipElevator)

namespace
{
	// Medidas internas da cabine (cm).
	constexpr float CabinDepth = 300.f;
	constexpr float CabinHalfWidth = 180.f;
	constexpr float CabinHeight = 280.f;
	constexpr float Wall = 10.f;
	constexpr float DoorHalfOpening = 100.f;
	constexpr float DoorHeight = 230.f;
	constexpr float DoorThickness = 6.f;
}

ANoclipElevator::ANoclipElevator()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;
	bAlwaysRelevant = true;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeFinder(TEXT("/Engine/BasicShapes/Cube.Cube"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> WallFinder(TEXT("/Game/NOCLIP/Materials/MI_L0_Wallpaper.MI_L0_Wallpaper"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> FloorFinder(TEXT("/Game/NOCLIP/Materials/MI_L0_Carpet.MI_L0_Carpet"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> CeilingFinder(TEXT("/Game/NOCLIP/Materials/MI_L0_Ceiling.MI_L0_Ceiling"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> MetalFinder(TEXT("/Game/NOCLIP/Materials/MI_Loot_White.MI_Loot_White"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> LightFinder(TEXT("/Game/NOCLIP/Materials/MI_L0_LightPanel.MI_L0_LightPanel"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> DarkFinder(TEXT("/Game/NOCLIP/Materials/MI_Loot_Blue.MI_Loot_Blue"));

	UStaticMesh* Cube = CubeFinder.Object;
	UMaterialInterface* WallMat = WallFinder.Object;
	MetalMaterial = MetalFinder.Object;
	CallOnMaterial = LightFinder.Object;
	CallOffMaterial = DarkFinder.Object;

	auto Make = [this, Cube](const TCHAR* Name, const FVector& Center, const FVector& Size, UMaterialInterface* Mat, bool bCollide) -> UStaticMeshComponent*
	{
		UStaticMeshComponent* C = CreateDefaultSubobject<UStaticMeshComponent>(Name);
		C->SetupAttachment(Root);
		C->SetStaticMesh(Cube);
		C->SetRelativeLocation(Center);
		C->SetRelativeScale3D(Size / 100.f);
		if (Mat)
		{
			C->SetMaterial(0, Mat);
		}
		C->SetCollisionProfileName(bCollide ? UCollisionProfile::BlockAll_ProfileName : UCollisionProfile::NoCollision_ProfileName);
		C->SetCanEverAffectNavigation(false);
		return C;
	};

	// Cabine (a origem e o centro da porta, no chao).
	Make(TEXT("Floor"), FVector(-CabinDepth * 0.5f, 0.f, 0.5f), FVector(CabinDepth, CabinHalfWidth * 2.f, 3.f), FloorFinder.Object, true);
	Make(TEXT("Ceiling"), FVector(-CabinDepth * 0.5f, 0.f, CabinHeight + Wall * 0.5f), FVector(CabinDepth + Wall * 2.f, CabinHalfWidth * 2.f + Wall * 2.f, Wall), CeilingFinder.Object, true);
	Make(TEXT("BackWall"), FVector(-CabinDepth - Wall * 0.5f, 0.f, CabinHeight * 0.5f), FVector(Wall, CabinHalfWidth * 2.f + Wall * 2.f, CabinHeight), MetalMaterial, true);
	Make(TEXT("LeftWall"), FVector(-CabinDepth * 0.5f, -CabinHalfWidth - Wall * 0.5f, CabinHeight * 0.5f), FVector(CabinDepth, Wall, CabinHeight), MetalMaterial, true);
	Make(TEXT("RightWall"), FVector(-CabinDepth * 0.5f, CabinHalfWidth + Wall * 0.5f, CabinHeight * 0.5f), FVector(CabinDepth, Wall, CabinHeight), MetalMaterial, true);

	// Parede da frente com o vao da porta, e a moldura.
	const float SideW = CabinHalfWidth + Wall - DoorHalfOpening;
	Make(TEXT("FrontLeft"), FVector(0.f, -(DoorHalfOpening + SideW * 0.5f), CabinHeight * 0.5f), FVector(Wall, SideW, CabinHeight), WallMat, true);
	Make(TEXT("FrontRight"), FVector(0.f, DoorHalfOpening + SideW * 0.5f, CabinHeight * 0.5f), FVector(Wall, SideW, CabinHeight), WallMat, true);
	Make(TEXT("FrontTop"), FVector(0.f, 0.f, DoorHeight + (CabinHeight - DoorHeight) * 0.5f), FVector(Wall, DoorHalfOpening * 2.f, CabinHeight - DoorHeight), WallMat, true);
	Make(TEXT("FrameTop"), FVector(Wall * 0.5f + 2.f, 0.f, DoorHeight + 6.f), FVector(4.f, DoorHalfOpening * 2.f + 24.f, 12.f), CallOffMaterial, false);
	Make(TEXT("FrameLeft"), FVector(Wall * 0.5f + 2.f, -DoorHalfOpening - 6.f, DoorHeight * 0.5f), FVector(4.f, 12.f, DoorHeight), CallOffMaterial, false);
	Make(TEXT("FrameRight"), FVector(Wall * 0.5f + 2.f, DoorHalfOpening + 6.f, DoorHeight * 0.5f), FVector(4.f, 12.f, DoorHeight), CallOffMaterial, false);

	// Portas de correr (na frente da parede).
	DoorLeft = Make(TEXT("DoorLeft"), FVector(Wall * 0.5f + DoorThickness * 0.5f + 1.f, -DoorHalfOpening * 0.5f, DoorHeight * 0.5f), FVector(DoorThickness, DoorHalfOpening, DoorHeight), MetalMaterial, true);
	DoorRight = Make(TEXT("DoorRight"), FVector(Wall * 0.5f + DoorThickness * 0.5f + 1.f, DoorHalfOpening * 0.5f, DoorHeight * 0.5f), FVector(DoorThickness, DoorHalfOpening, DoorHeight), MetalMaterial, true);

	// Painel do andar e botao de chamada.
	Make(TEXT("FloorPanel"), FVector(Wall * 0.5f + 3.f, 0.f, DoorHeight + 30.f), FVector(4.f, 60.f, 24.f), CallOffMaterial, false);
	FloorText = CreateDefaultSubobject<UTextRenderComponent>(TEXT("FloorText"));
	FloorText->SetupAttachment(Root);
	FloorText->SetRelativeLocation(FVector(Wall * 0.5f + 6.f, 0.f, DoorHeight + 30.f));
	FloorText->SetHorizontalAlignment(EHTA_Center);
	FloorText->SetVerticalAlignment(EVRTA_TextCenter);
	FloorText->SetWorldSize(18.f);
	FloorText->SetTextRenderColor(FColor(245, 176, 58));
	FloorText->SetText(FText::FromString(TEXT("▼ 0")));

	CallLight = Make(TEXT("CallLight"), FVector(Wall * 0.5f + 2.f, DoorHalfOpening + 40.f, 120.f), FVector(3.f, 8.f, 8.f), CallOffMaterial, false);

	// Luz da cabine (quente, fluorescente) e o painel no teto.
	Make(TEXT("CabinPanel"), FVector(-CabinDepth * 0.5f, 0.f, CabinHeight - 1.f), FVector(160.f, 120.f, 2.f), CallOnMaterial, false);
	CabinLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("CabinLight"));
	CabinLight->SetupAttachment(Root);
	CabinLight->SetRelativeLocation(FVector(-CabinDepth * 0.5f, 0.f, CabinHeight - 30.f));
	CabinLight->SetIntensityUnits(ELightUnits::Candelas);
	CabinLight->SetIntensity(8.f);
	CabinLight->SetAttenuationRadius(520.f);
	CabinLight->SetLightColor(FLinearColor(1.f, 0.93f, 0.78f));
	CabinLight->SetCastShadows(false);
}

void ANoclipElevator::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ANoclipElevator, bDoorsOpen);
	DOREPLIFETIME(ANoclipElevator, DoorChangeServerTime);
	DOREPLIFETIME(ANoclipElevator, FloorLabel);
}

void ANoclipElevator::BeginPlay()
{
	Super::BeginPlay();
	OnRep_Floor();
}

double ANoclipElevator::GetServerNow() const
{
	const UWorld* World = GetWorld();
	const AGameStateBase* GS = World ? World->GetGameState() : nullptr;
	return GS ? GS->GetServerWorldTimeSeconds() : (World ? World->GetTimeSeconds() : 0.0);
}

void ANoclipElevator::SetDoorsOpen(bool bOpen, bool bInstant)
{
	if (!HasAuthority())
	{
		return;
	}
	if (bDoorsOpen == bOpen && !bInstant)
	{
		return;
	}
	bDoorsOpen = bOpen;
	DoorChangeServerTime = bInstant ? -100.0 : GetServerNow();
	OnRep_Doors();
}

void ANoclipElevator::SetFloorLabel(const FString& InLabel)
{
	if (HasAuthority())
	{
		FloorLabel = InLabel;
		OnRep_Floor();
	}
}

void ANoclipElevator::OnRep_Floor()
{
	if (FloorText)
	{
		FloorText->SetText(FText::FromString(FString::Printf(TEXT("▼ %s"), *FloorLabel)));
	}
}

void ANoclipElevator::OnRep_Doors()
{
	if (CallLight)
	{
		CallLight->SetMaterial(0, bDoorsOpen ? CallOnMaterial.Get() : CallOffMaterial.Get());
	}
	if (!bFiredOnce || bLastFiredOpen != bDoorsOpen)
	{
		bFiredOnce = true;
		bLastFiredOpen = bDoorsOpen;
		if (DoorChangeServerTime > 0.0)
		{
			OnDoorsStartMoving(bDoorsOpen);
		}
	}
}

float ANoclipElevator::GetDoorAlpha() const
{
	const float Linear = DoorChangeServerTime < 0.0 ? 1.f
		: FMath::Clamp(static_cast<float>((GetServerNow() - DoorChangeServerTime) / FMath::Max(DoorTravelSeconds, 0.05f)), 0.f, 1.f);
	// Portas de elevador: arrancam devagar e freiam no fim.
	const float Eased = FMath::InterpEaseInOut(0.f, 1.f, Linear, 2.f);
	return bDoorsOpen ? Eased : 1.f - Eased;
}

void ANoclipElevator::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	const float Alpha = GetDoorAlpha();
	const float Offset = DoorHalfOpening * 0.5f + Alpha * (DoorHalfOpening - 4.f);
	const float X = Wall * 0.5f + DoorThickness * 0.5f + 1.f;
	DoorLeft->SetRelativeLocation(FVector(X, -Offset, DoorHeight * 0.5f));
	DoorRight->SetRelativeLocation(FVector(X, Offset, DoorHeight * 0.5f));
}

FTransform ANoclipElevator::GetCabinSlot(int32 Index, float CapsuleHalfHeight) const
{
	static const FVector2D Slots[6] = {
		FVector2D(-110.f, -70.f), FVector2D(-110.f, 70.f),
		FVector2D(-220.f, -110.f), FVector2D(-220.f, 0.f), FVector2D(-220.f, 110.f),
		FVector2D(-110.f, 0.f)
	};
	const FVector2D S = Slots[FMath::Abs(Index) % 6];
	const FVector Local(S.X, S.Y, CapsuleHalfHeight + 4.f);
	return FTransform(GetActorRotation(), GetActorTransform().TransformPosition(Local));
}

FVector ANoclipElevator::GetDoorFront(float Distance) const
{
	return GetActorTransform().TransformPosition(FVector(Distance, 0.f, 0.f));
}
