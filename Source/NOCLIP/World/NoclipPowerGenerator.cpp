// Copyright Fuly Games. All Rights Reserved.

#include "World/NoclipPowerGenerator.h"
#include "Audio/NoclipAudioSubsystem.h"
#include "Character/NoclipCharacter.h"
#include "Enemy/NoclipNoiseSubsystem.h"
#include "Game/NoclipGameMode.h"
#include "Game/NoclipGameState.h"
#include "NOCLIP.h"
#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Net/UnrealNetwork.h"
#include "UObject/ConstructorHelpers.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(NoclipPowerGenerator)

ANoclipPowerGenerator::ANoclipPowerGenerator()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 0.05f;
	bReplicates = true;
	bAlwaysRelevant = true;
	SetReplicatingMovement(false);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> BodyFinder(TEXT("/Game/NOCLIP/Assets/PuzzleMachinery/generator/StaticMeshes/generator.generator"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> LampFinder(TEXT("/Game/NOCLIP/Assets/PuzzleMachinery/generator/StaticMeshes/generator_lamp.generator_lamp"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> LampIdleFinder(TEXT("/Game/NOCLIP/Assets/PuzzleMachinery/generator/StaticMeshes/generator_lamp_idle.generator_lamp_idle"));

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;

	auto MakePart = [this](const TCHAR* Name, UStaticMesh* Mesh, bool bCollide)
	{
		UStaticMeshComponent* C = CreateDefaultSubobject<UStaticMeshComponent>(Name);
		C->SetupAttachment(Root);
		C->SetStaticMesh(Mesh);
		C->SetCollisionProfileName(bCollide ? TEXT("BlockAll") : TEXT("NoCollision"));
		C->SetCanEverAffectNavigation(false);
		return C;
	};
	// O modelo ja vem com pivo no chao e as lampadas na posicao certa.
	Body = MakePart(TEXT("Body"), BodyFinder.Object, true);
	LampOn = MakePart(TEXT("LampOn"), LampFinder.Object, false);
	LampIdle = MakePart(TEXT("LampIdle"), LampIdleFinder.Object, false);
	LampIdle->SetVisibility(false);

	StatusLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("StatusLight"));
	StatusLight->SetupAttachment(Root);
	StatusLight->SetRelativeLocation(FVector(53.f, 18.f, 160.f)); // em cima da lampada do modelo
	StatusLight->SetIntensityUnits(ELightUnits::Candelas);
	StatusLight->SetIntensity(6.f);
	StatusLight->SetAttenuationRadius(420.f);
	StatusLight->SetLightColor(FLinearColor(0.3f, 1.f, 0.4f));
	StatusLight->SetCastShadows(false);

	Label = CreateDefaultSubobject<UTextRenderComponent>(TEXT("Label"));
	Label->SetupAttachment(Root);
	Label->SetRelativeLocation(FVector(0.f, 43.f, 125.f));
	Label->SetRelativeRotation(FRotator(0.f, 90.f, 0.f));
	Label->SetHorizontalAlignment(EHTA_Center);
	Label->SetVerticalAlignment(EVRTA_TextCenter);
	Label->SetWorldSize(9.f);
	Label->SetTextRenderColor(FColor(30, 30, 30));
	Label->SetText(NSLOCTEXT("NoclipPower", "Label", "GERADOR · BASELINE"));
	Label->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

void ANoclipPowerGenerator::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ANoclipPowerGenerator, Progress);
	DOREPLIFETIME(ANoclipPowerGenerator, Workers);
}

FVector ANoclipPowerGenerator::GetPanelLocation() const
{
	return GetActorTransform().TransformPosition(FVector(0.f, 0.f, 100.f));
}

bool ANoclipPowerGenerator::IsInReach(const FVector& Location) const
{
	// Distancia ate a caixa do modelo (230 x 82 x 164): da para mexer de qualquer lado.
	const FVector Local = GetActorTransform().InverseTransformPosition(Location);
	const FVector Closest(FMath::Clamp(Local.X, -115.f, 115.f), FMath::Clamp(Local.Y, -41.f, 41.f), FMath::Clamp(Local.Z, 0.f, 164.f));
	return FVector::Dist(Local, Closest) <= ReachDistance - 80.f;
}

void ANoclipPowerGenerator::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	const ANoclipGameState* GS = GetWorld()->GetGameState<ANoclipGameState>();
	const bool bOut = GS && GS->IsPowerOut();
	const double Now = GetWorld()->GetTimeSeconds();

	// Lampada: verde ligado; no apagao, vermelha piscando (mais rapido conforme carrega).
	if (GetNetMode() != NM_DedicatedServer && StatusLight)
	{
		if (bOut)
		{
			const float Rate = 1.5f + Progress * 6.f;
			const bool bBlink = FMath::Frac(Now * Rate) < 0.5f;
			StatusLight->SetLightColor(FLinearColor(1.f, 0.15f, 0.08f));
			StatusLight->SetIntensity(bBlink ? 9.f : 1.f);
			LampOn->SetVisibility(bBlink);
			LampIdle->SetVisibility(!bBlink);
		}
		else
		{
			StatusLight->SetLightColor(FLinearColor(0.3f, 1.f, 0.4f));
			StatusLight->SetIntensity(4.f);
			LampOn->SetVisibility(true);
			LampIdle->SetVisibility(false);
		}
	}

	// Servidor: quem esta mexendo agora (para a HUD dizer "2 no gerador").
	if (HasAuthority())
	{
		int32 Count = 0;
		for (auto It = LastAttempt.CreateIterator(); It; ++It)
		{
			if (!It.Key().IsValid() || Now - It.Value() > 3.0)
			{
				It.RemoveCurrent();
				continue;
			}
			++Count;
		}
		Workers = static_cast<uint8>(bOut ? Count : 0);
	}
}

void ANoclipPowerGenerator::BeginOutage()
{
	if (HasAuthority())
	{
		Progress = 0.f;
		LastAttempt.Reset();
		ForceNetUpdate();
	}
}

void ANoclipPowerGenerator::ServerAttempt(ANoclipCharacter* By, bool bHit)
{
	const ANoclipGameState* GS = GetWorld()->GetGameState<ANoclipGameState>();
	if (!HasAuthority() || !By || !GS || !GS->IsPowerOut() || !IsInReach(By->GetActorLocation()))
	{
		return;
	}
	const double Now = GetWorld()->GetTimeSeconds();
	// Ritmo maximo por pessoa (o QTE do dono ja limita; isto evita spam).
	if (const double* Last = LastAttempt.Find(By))
	{
		if (Now - *Last < 0.2)
		{
			return;
		}
	}
	LastAttempt.Add(By, Now);

	const float Old = Progress;
	if (bHit)
	{
		Progress = FMath::Min(1.f, Progress + HitGain);
	}
	else
	{
		Progress = FMath::Max(0.f, Progress - MissLoss);
		// Faisca e barulho: as entidades vem ver.
		UNoclipNoiseSubsystem::Report(this, GetActorLocation(), 1500.f, By, true);
	}
	OnRep_Progress(Old);
	ForceNetUpdate();

	if (Progress >= 1.f)
	{
		if (ANoclipGameMode* GM = GetWorld()->GetAuthGameMode<ANoclipGameMode>())
		{
			GM->RestorePower(this);
		}
	}
}

void ANoclipPowerGenerator::OnRep_Progress(float OldProgress)
{
	if (GetNetMode() == NM_DedicatedServer || FMath::IsNearlyEqual(OldProgress, Progress))
	{
		return;
	}
	FeedbackSound(Progress > OldProgress);
}

void ANoclipPowerGenerator::FeedbackSound(bool bHit) const
{
	if (UNoclipAudioSubsystem* Audio = UNoclipAudioSubsystem::Get(this))
	{
		Audio->PlayGeneratorFeedback(GetPanelLocation(), bHit, Progress);
	}
}
