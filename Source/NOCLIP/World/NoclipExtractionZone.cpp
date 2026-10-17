// Copyright Fuly Games. All Rights Reserved.

#include "World/NoclipExtractionZone.h"
#include "World/NoclipLootActor.h"
#include "Character/NoclipCharacter.h"
#include "Game/NoclipGameMode.h"
#include "Game/NoclipGameState.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Net/UnrealNetwork.h"
#include "UObject/ConstructorHelpers.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(NoclipExtractionZone)

ANoclipExtractionZone::ANoclipExtractionZone()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 0.2f;
	bReplicates = true;
	bAlwaysRelevant = true;

	Volume = CreateDefaultSubobject<UBoxComponent>(TEXT("Volume"));
	Volume->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Volume->SetHiddenInGame(true);
	RootComponent = Volume;

	FloorMarker = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("FloorMarker"));
	FloorMarker->SetupAttachment(Volume);
	FloorMarker->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	FloorMarker->SetCastShadow(false);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (Cube.Succeeded())
	{
		FloorMarker->SetStaticMesh(Cube.Object);
	}

	Sign = CreateDefaultSubobject<UTextRenderComponent>(TEXT("Sign"));
	Sign->SetupAttachment(Volume);
	Sign->SetText(FText::FromString(TEXT("SALA MANILA")));
	Sign->SetHorizontalAlignment(EHTA_Center);
	Sign->SetVerticalAlignment(EVRTA_TextCenter);
	Sign->SetWorldSize(60.f);
	Sign->SetTextRenderColor(FColor(230, 200, 120));
}

void ANoclipExtractionZone::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ANoclipExtractionZone, ReplicatedHalfExtent);
	DOREPLIFETIME(ANoclipExtractionZone, ReplicatedHeight);
	DOREPLIFETIME(ANoclipExtractionZone, ReplicatedMarkerMaterial);
}

void ANoclipExtractionZone::Setup(const FVector2D& HalfExtent, float Height, UMaterialInterface* MarkerMaterial)
{
	ReplicatedHalfExtent = HalfExtent;
	ReplicatedHeight = Height;
	ReplicatedMarkerMaterial = MarkerMaterial;
	ApplyLayout();
}

void ANoclipExtractionZone::OnRep_Layout()
{
	ApplyLayout();
}

void ANoclipExtractionZone::ApplyLayout()
{
	// O ator fica no nivel do chao; o volume sobe ate o teto.
	// O Volume e a raiz: nao mexer na posicao dele aqui (SetRelativeLocation na raiz levava a sala para a origem do mundo).
	Volume->SetBoxExtent(FVector(ReplicatedHalfExtent.X, ReplicatedHalfExtent.Y, ReplicatedHeight * 0.5f));

	// Faixa de piso iluminada marcando a sala (2 cm acima do carpete).
	FloorMarker->SetRelativeLocation(FVector(0.f, 0.f, -ReplicatedHeight * 0.5f + 1.f));
	FloorMarker->SetRelativeScale3D(FVector(ReplicatedHalfExtent.X * 2.f / 100.f, ReplicatedHalfExtent.Y * 2.f / 100.f, 0.02f));
	if (ReplicatedMarkerMaterial)
	{
		FloorMarker->SetMaterial(0, ReplicatedMarkerMaterial);
	}

	Sign->SetRelativeLocation(FVector(0.f, 0.f, ReplicatedHeight * 0.5f - 60.f));
}

bool ANoclipExtractionZone::IsInside(const FVector& WorldLocation) const
{
	const FVector Local = GetActorTransform().InverseTransformPosition(WorldLocation);
	return FMath::Abs(Local.X) <= ReplicatedHalfExtent.X && FMath::Abs(Local.Y) <= ReplicatedHalfExtent.Y && FMath::Abs(Local.Z) <= ReplicatedHeight * 0.5f + 50.f;
}

void ANoclipExtractionZone::ComputeContents(int32& OutValue, int32& OutItems, int32& OutPlayers) const
{
	OutValue = 0;
	OutItems = 0;
	OutPlayers = 0;

	for (TActorIterator<ANoclipLootActor> It(GetWorld()); It; ++It)
	{
		if (IsValid(*It) && IsInside(It->GetActorLocation()))
		{
			OutValue += It->GetCurrentValue();
			++OutItems;
		}
	}
	for (TActorIterator<ANoclipCharacter> It(GetWorld()); It; ++It)
	{
		if (IsValid(*It) && IsInside(It->GetActorLocation()))
		{
			++OutPlayers;
		}
	}
}

void ANoclipExtractionZone::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	// O texto sempre de frente para a camera local (barato e legivel).
	if (const APlayerController* PC = GetWorld()->GetFirstPlayerController())
	{
		if (PC->PlayerCameraManager)
		{
			const FVector ToCam = PC->PlayerCameraManager->GetCameraLocation() - Sign->GetComponentLocation();
			Sign->SetWorldRotation(FRotator(0.f, ToCam.Rotation().Yaw, 0.f));
		}
	}

	if (!HasAuthority())
	{
		return;
	}

	ANoclipGameState* GS = GetWorld()->GetGameState<ANoclipGameState>();
	if (!GS)
	{
		return;
	}

	int32 Value = 0, Items = 0, PlayersInside = 0;
	ComputeContents(Value, Items, PlayersInside);

	int32 PlayersAlive = 0;
	for (TActorIterator<ANoclipCharacter> It(GetWorld()); It; ++It)
	{
		if (IsValid(*It)) { ++PlayersAlive; }
	}

	const ENoclipShiftPhase Phase = GS->GetPhase();
	const bool bShiftRunning = Phase == ENoclipShiftPhase::Active || Phase == ENoclipShiftPhase::Collapse;
	const bool bEveryoneInside = PlayersAlive > 0 && PlayersInside >= PlayersAlive;

	if (bShiftRunning && !bElevatorLocked && bEveryoneInside)
	{
		HoldProgress += DeltaSeconds / ExtractHoldSeconds;
	}
	else
	{
		HoldProgress = FMath::Max(0.f, HoldProgress - DeltaSeconds / 1.5f);
	}
	HoldProgress = FMath::Clamp(HoldProgress, 0.f, 1.f);

	GS->SetExtractionStatus(Value, HoldProgress, PlayersInside);

	if (HoldProgress >= 1.f && bShiftRunning)
	{
		bElevatorLocked = true;
		if (ANoclipGameMode* GM = GetWorld()->GetAuthGameMode<ANoclipGameMode>())
		{
			GM->NotifyExtractionComplete();
		}
	}
}
