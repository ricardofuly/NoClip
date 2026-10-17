// Copyright Fuly Games. All Rights Reserved.

#include "Character/NoclipGrabComponent.h"
#include "Character/NoclipCharacter.h"
#include "Character/NoclipToolComponent.h"
#include "World/NoclipToolPickup.h"
#include "World/NoclipLootActor.h"
#include "Enemy/NoclipNoiseSubsystem.h"
#include "Game/NoclipPlayerState.h"
#include "NOCLIP.h"
#include "PhysicsEngine/PhysicsHandleComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Engine/World.h"
#include "Net/UnrealNetwork.h"
#include "CollisionQueryParams.h"
#include "DrawDebugHelpers.h"
#include "HAL/IConsoleManager.h"

static TAutoConsoleVariable<int32> CVarNoclipDebugGrab(TEXT("noclip.DebugGrab"), 0, TEXT("Desenha e loga as tentativas de agarrar."));

#include UE_INLINE_GENERATED_CPP_BY_NAME(NoclipGrabComponent)

UNoclipGrabComponent::UNoclipGrabComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickGroup = TG_PrePhysics;
	SetIsReplicatedByDefault(true);
}

void UNoclipGrabComponent::BeginPlay()
{
	Super::BeginPlay();

	if (GetOwner() && GetOwner()->HasAuthority())
	{
		PhysicsHandle = NewObject<UPhysicsHandleComponent>(GetOwner(), TEXT("GrabPhysicsHandle"));
		PhysicsHandle->bSoftAngularConstraint = true;
		PhysicsHandle->bSoftLinearConstraint = true;
		PhysicsHandle->bInterpolateTarget = true;
		PhysicsHandle->SetLinearStiffness(900.f);
		PhysicsHandle->SetLinearDamping(140.f);
		PhysicsHandle->SetAngularStiffness(600.f);
		PhysicsHandle->SetAngularDamping(300.f);
		PhysicsHandle->RegisterComponent();
	}
}

void UNoclipGrabComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UNoclipGrabComponent, HeldActor);
	DOREPLIFETIME_CONDITION(UNoclipGrabComponent, CarrySpeedScale, COND_OwnerOnly);
}

ANoclipCharacter* UNoclipGrabComponent::GetNoclipOwner() const
{
	return Cast<ANoclipCharacter>(GetOwner());
}

void UNoclipGrabComponent::ToggleGrab()
{
	UE_LOG(LogNoclip, Verbose, TEXT("[Grab] ToggleGrab no cliente (holding=%d, role=%d)"), IsHolding() ? 1 : 0, GetOwner() ? (int32)GetOwner()->GetLocalRole() : -1);
	if (IsHolding())
	{
		Server_Release();
		return;
	}

	ANoclipCharacter* OwnerChar = GetNoclipOwner();
	UNoclipToolComponent* Tools = OwnerChar ? OwnerChar->GetToolComponent() : nullptr;
	bool bTooHeavy = false;
	AActor* Candidate = FindGrabCandidate(bTooHeavy);

	// Ferramenta solta no chao: vai para o cinto em vez de ficar na mao.
	if (ANoclipToolPickup* Pickup = Cast<ANoclipToolPickup>(Candidate))
	{
		if (Tools)
		{
			Tools->RequestPickup(Pickup);
		}
		return;
	}

	// Achado pesado precisa das duas maos: com ferramenta na mao, nega (aviso + "nao" com a cabeca).
	if (Candidate && Tools && Tools->HasToolInHand() && IsTwoHandedActor(Candidate))
	{
		Tools->Deny(ENoclipHandsDenyReason::HeavyNeedsBothHands);
		return;
	}
	Server_TryGrab();
}

bool UNoclipGrabComponent::IsTwoHandedActor(const AActor* Actor)
{
	if (const ANoclipLootActor* Loot = Cast<ANoclipLootActor>(Actor))
	{
		return Loot->GetMassKg() >= NoclipTools::TwoHandMassKg;
	}
	const UPrimitiveComponent* Prim = Actor ? Cast<UPrimitiveComponent>(Actor->GetRootComponent()) : nullptr;
	return Prim && Prim->IsSimulatingPhysics() && Prim->GetMass() >= NoclipTools::TwoHandMassKg;
}

bool UNoclipGrabComponent::IsHoldingTwoHanded() const
{
	return HeldActor && IsTwoHandedActor(HeldActor);
}

void UNoclipGrabComponent::BeginThrowCharge()
{
	if (IsHolding())
	{
		ThrowChargeStartTime = GetWorld()->GetTimeSeconds();
	}
}

float UNoclipGrabComponent::GetThrowCharge() const
{
	if (ThrowChargeStartTime < 0.0 || !GetWorld())
	{
		return 0.f;
	}
	return FMath::Clamp(static_cast<float>((GetWorld()->GetTimeSeconds() - ThrowChargeStartTime) / FMath::Max(ThrowMaxChargeTime, 0.01f)), 0.f, 1.f);
}

AActor* UNoclipGrabComponent::FindGrabCandidate(bool& bOutTooHeavy) const
{
	bOutTooHeavy = false;
	const ANoclipCharacter* OwnerChar = GetNoclipOwner();
	const UWorld* World = GetWorld();
	if (!OwnerChar || !World)
	{
		return nullptr;
	}

	// Mesma consulta do servidor (Server_TryGrab), mas com a rotacao local da camera.
	const FVector Start = OwnerChar->GetEyeLocation();
	const FVector End = Start + OwnerChar->GetViewRotation().Vector() * GrabReach;

	FCollisionQueryParams Params(SCENE_QUERY_STAT(NoclipGrabPrompt), false, OwnerChar);
	FCollisionObjectQueryParams ObjectParams;
	ObjectParams.AddObjectTypesToQuery(ECC_PhysicsBody);
	ObjectParams.AddObjectTypesToQuery(ECC_WorldDynamic);

	FHitResult Hit;
	if (!World->SweepSingleByObjectType(Hit, Start, End, FQuat::Identity, ObjectParams, FCollisionShape::MakeSphere(GrabSweepRadius), Params))
	{
		return nullptr;
	}
	const UPrimitiveComponent* Comp = Hit.GetComponent();
	const bool bSimulating = Comp && Comp->IsSimulatingPhysics();
	if (!Comp || (!bSimulating && !Cast<ANoclipLootActor>(Hit.GetActor()) && !Cast<ANoclipToolPickup>(Hit.GetActor())))
	{
		return nullptr;
	}
	// Achado de equipe: o limite vale para a parte de cada um.
	const ANoclipLootActor* Loot = Cast<ANoclipLootActor>(Hit.GetActor());
	const int32 Share = Loot ? FMath::Max(1, Loot->GetRequiredCarriers()) : 1;
	bOutTooHeavy = bSimulating && Comp->GetMass() / Share > MaxGrabMassKg;
	return Hit.GetActor();
}

void UNoclipGrabComponent::ReleaseThrow()
{
	if (ThrowChargeStartTime < 0.0)
	{
		return;
	}
	const float Charge = GetThrowCharge();
	ThrowChargeStartTime = -1.0;

	if (IsHolding())
	{
		OnThrowFeel(Charge);
		Server_Throw(Charge);
	}
}

void UNoclipGrabComponent::Server_TryGrab_Implementation()
{
	ANoclipCharacter* OwnerChar = GetNoclipOwner();
	if (!OwnerChar || IsHolding() || !PhysicsHandle)
	{
		UE_LOG(LogNoclip, Verbose, TEXT("[Grab] Servidor recusou: owner=%d holding=%d handle=%d"), OwnerChar ? 1 : 0, IsHolding() ? 1 : 0, PhysicsHandle ? 1 : 0);
		return;
	}

	const FVector Start = OwnerChar->GetEyeLocation();
	const FVector Dir = OwnerChar->GetAuthoritativeViewRotation().Vector();
	UE_LOG(LogNoclip, Verbose, TEXT("[Grab] Servidor: olho=%s dir=%s rot=%s"), *Start.ToCompactString(), *Dir.ToCompactString(), *OwnerChar->GetAuthoritativeViewRotation().ToCompactString());
	const FVector End = Start + Dir * GrabReach;

	FCollisionQueryParams Params(SCENE_QUERY_STAT(NoclipGrab), false, OwnerChar);
	FCollisionObjectQueryParams ObjectParams;
	ObjectParams.AddObjectTypesToQuery(ECC_PhysicsBody);
	ObjectParams.AddObjectTypesToQuery(ECC_WorldDynamic);

	FHitResult Hit;
	const bool bHit = GetWorld()->SweepSingleByObjectType(Hit, Start, End, FQuat::Identity, ObjectParams, FCollisionShape::MakeSphere(GrabSweepRadius), Params);
	if (CVarNoclipDebugGrab.GetValueOnGameThread() > 0)
	{
		DrawDebugLine(GetWorld(), Start, bHit ? Hit.Location : End, bHit ? FColor::Green : FColor::Red, false, 3.f, 0, 1.5f);
	}
	if (!bHit || !Hit.GetComponent() || !Hit.GetComponent()->IsSimulatingPhysics())
	{
		UE_LOG(LogNoclip, Verbose, TEXT("[Grab] Nada agarravel: hit=%d comp=%s simula=%d"), bHit ? 1 : 0, *GetNameSafe(Hit.GetComponent()), (Hit.GetComponent() && Hit.GetComponent()->IsSimulatingPhysics()) ? 1 : 0);
		return;
	}

	ServerGrabFromHit(Hit);
}

void UNoclipGrabComponent::ServerGrabFromHit(const FHitResult& Hit)
{
	ANoclipCharacter* OwnerChar = GetNoclipOwner();
	if (!OwnerChar || IsHolding() || !PhysicsHandle || !Hit.GetComponent() || !Hit.GetComponent()->IsSimulatingPhysics())
	{
		return;
	}

	UPrimitiveComponent* Comp = Hit.GetComponent();
	const float Mass = Comp->GetMass();
	ANoclipLootActor* HitLoot = Cast<ANoclipLootActor>(Comp->GetOwner());
	const int32 Required = HitLoot ? FMath::Max(1, HitLoot->GetRequiredCarriers()) : 1;
	if (Mass / Required > MaxGrabMassKg)
	{
		UE_LOG(LogNoclip, Verbose, TEXT("%s pesado demais (%.0f kg)"), *GetNameSafe(Comp->GetOwner()), Mass);
		return;
	}
	if (Cast<ANoclipToolPickup>(Comp->GetOwner()))
	{
		return; // ferramentas vao para o cinto (UNoclipToolComponent::RequestPickup)
	}
	const bool bTwoHanded = IsTwoHandedActor(Comp->GetOwner());
	if (bTwoHanded && OwnerChar->GetToolComponent() && OwnerChar->GetToolComponent()->HasToolInHand())
	{
		return; // o dono ja mostrou o aviso; aqui so garante a regra
	}

	// Achado de equipe: entra na equipe (o primeiro guia a fisica; os outros dividem o peso).
	if (HitLoot && Required > 1)
	{
		if (HitLoot->IsCarriedBy(OwnerChar) || HitLoot->GetNumCarriers() >= FMath::Min(Required + 1, 4))
		{
			return;
		}
		HoldDistance = FMath::Clamp(Hit.Distance + GrabSweepRadius, MinHoldDistance, MaxHoldDistance);
		const bool bFirst = HitLoot->GetNumCarriers() == 0;
		HeldComponent = Comp;
		HeldActor = HitLoot;
		if (bFirst)
		{
			BecomeLead();
		}
		HitLoot->AddCarrier(OwnerChar); // chama ApplyCarryShare de todos
		HitLoot->NotifyGrabbed(OwnerChar->GetPlayerState<ANoclipPlayerState>());
		Comp->WakeAllRigidBodies();
		UE_LOG(LogNoclip, Verbose, TEXT("[Grab] %s entrou na equipe de %s (%d/%d)"), *GetNameSafe(OwnerChar), *GetNameSafe(HitLoot), HitLoot->GetNumCarriers(), Required);
		OnRep_HeldActor();
		return;
	}

	// Um so por vez nos achados comuns: quem chegou primeiro fica.
	if (HitLoot && HitLoot->GetNumCarriers() > 0)
	{
		return;
	}
	HoldDistance = FMath::Clamp(Hit.Distance + GrabSweepRadius, MinHoldDistance, MaxHoldDistance);

	const float MassAlpha = FMath::Clamp(Mass / MaxGrabMassKg, 0.f, 1.f);
	PhysicsHandle->SetInterpolationSpeed(FMath::Lerp(LightInterpSpeed, HeavyInterpSpeed, MassAlpha));
	CarrySpeedScale = 1.f - MassAlpha * MaxCarryPenalty;
	if (bTwoHanded)
	{
		// Duas maos: anda mais devagar e nao corre (abaixo do limite de sprint do UNoclipWalkingMode).
		CarrySpeedScale = FMath::Min(CarrySpeedScale * TwoHandedSpeedFactor, 0.72f);
	}

	PhysicsHandle->GrabComponentAtLocationWithRotation(Comp, NAME_None, Hit.ImpactPoint, Comp->GetComponentRotation());
	UE_LOG(LogNoclip, Verbose, TEXT("[Grab] Agarrou %s (%.0f kg)"), *GetNameSafe(Comp->GetOwner()), Mass);
	Comp->WakeAllRigidBodies();

	HeldComponent = Comp;
	HeldActor = Comp->GetOwner();
	if (ANoclipLootActor* Loot = Cast<ANoclipLootActor>(HeldActor))
	{
		Loot->NotifyGrabbed(OwnerChar->GetPlayerState<ANoclipPlayerState>());
		Loot->AddCarrier(OwnerChar);
	}
	OnRep_HeldActor();
}

// ===================================================================== Carregar em equipe

bool UNoclipGrabComponent::IsLead() const
{
	return PhysicsHandle && HeldComponent && PhysicsHandle->GetGrabbedComponent() == HeldComponent;
}

void UNoclipGrabComponent::BecomeLead()
{
	if (!PhysicsHandle || !IsValid(HeldComponent) || IsLead())
	{
		return;
	}
	// Pega pelo centro de massa: a equipe leva o achado pelo meio, sem girar estranho.
	PhysicsHandle->SetInterpolationSpeed(HeavyInterpSpeed);
	PhysicsHandle->GrabComponentAtLocationWithRotation(HeldComponent, NAME_None, HeldComponent->GetCenterOfMass(), HeldComponent->GetComponentRotation());
	HeldComponent->WakeAllRigidBodies();
}

void UNoclipGrabComponent::ApplyCarryShare(int32 Carriers, int32 Required, float MassKg)
{
	const int32 Count = FMath::Max(Carriers, 1);
	if (Required > 1 && Count < Required)
	{
		CarrySpeedScale = DragSpeedScale; // arrastando: devagar e sem correr
		return;
	}
	const float Share = MassKg / Count;
	const float MassAlpha = FMath::Clamp(Share / FMath::Max(MaxGrabMassKg, 1.f), 0.f, 1.f);
	CarrySpeedScale = 1.f - MassAlpha * MaxCarryPenalty;
	if (Share >= NoclipTools::TwoHandMassKg || Required > 1)
	{
		CarrySpeedScale = FMath::Min(CarrySpeedScale * TwoHandedSpeedFactor, 0.72f);
	}
}

FVector UNoclipGrabComponent::GetHoldTarget() const
{
	const ANoclipCharacter* OwnerChar = GetNoclipOwner();
	if (!OwnerChar)
	{
		return FVector::ZeroVector;
	}
	return OwnerChar->GetEyeLocation() + OwnerChar->GetAuthoritativeViewRotation().Vector() * HoldDistance;
}

bool UNoclipGrabComponent::IsDragging() const
{
	const ANoclipLootActor* Loot = Cast<ANoclipLootActor>(HeldActor);
	return Loot && Loot->IsTeamCarry() && !Loot->IsLifted();
}

void UNoclipGrabComponent::TickTeamCarry(float DeltaTime)
{
	ANoclipLootActor* Loot = Cast<ANoclipLootActor>(HeldActor);
	const ANoclipCharacter* OwnerChar = GetNoclipOwner();
	if (!Loot || !OwnerChar)
	{
		return;
	}
	// O lider saiu sem avisar (desconectou): alguem assume.
	if (!Loot->GetLeadCarrier() || (Loot->GetLeadCarrier() == OwnerChar && !IsLead()))
	{
		BecomeLead();
	}

	if (IsLead())
	{
		FVector Target = Loot->ComputeTeamTarget();
		const FRotator Flat(0.f, HeldComponent->GetComponentRotation().Yaw, 0.f);
		if (!Loot->IsLifted())
		{
			// Falta gente: so desliza pelo chao (a altura fica onde esta) e faz barulho de arrasto.
			Target.Z = HeldComponent->GetCenterOfMass().Z;
			PhysicsHandle->SetInterpolationSpeed(2.f);
			const double Now = GetWorld()->GetTimeSeconds();
			if (Now >= NextDragNoiseTime && HeldComponent->GetPhysicsLinearVelocity().Size2D() > 40.f)
			{
				NextDragNoiseTime = Now + 0.7;
				UNoclipNoiseSubsystem::Report(this, HeldComponent->GetComponentLocation(), 1000.f, GetOwner(), true);
			}
		}
		else
		{
			PhysicsHandle->SetInterpolationSpeed(HeavyInterpSpeed + 1.f);
		}
		PhysicsHandle->SetTargetLocationAndRotation(Target, Flat);
	}

	// Cada um solta se ficar longe demais do achado (a equipe se separou).
	const float Reach = BreakDistance + HeldComponent->Bounds.SphereRadius;
	if (FVector::DistSquared(HeldComponent->GetComponentLocation(), GetHoldTarget()) > FMath::Square(Reach))
	{
		ReleaseInternal();
	}
}

void UNoclipGrabComponent::Server_Release_Implementation()
{
	ReleaseInternal();
}

void UNoclipGrabComponent::Server_Throw_Implementation(float Charge)
{
	ANoclipCharacter* OwnerChar = GetNoclipOwner();
	UPrimitiveComponent* Comp = HeldComponent;
	if (!OwnerChar || !Comp)
	{
		return;
	}
	// Achado de equipe nao se arremessa: so solta.
	if (const ANoclipLootActor* TeamLoot = Cast<ANoclipLootActor>(HeldActor); TeamLoot && TeamLoot->IsTeamCarry())
	{
		ReleaseInternal();
		return;
	}

	ReleaseInternal();

	const float ClampedCharge = FMath::Clamp(Charge, 0.f, 1.f);
	const FVector Dir = OwnerChar->GetAuthoritativeViewRotation().Vector();
	const float Speed = FMath::Lerp(ThrowMinSpeed, ThrowMaxSpeed, ClampedCharge * ClampedCharge);
	// Objetos pesados saem mais devagar.
	const float MassAlpha = FMath::Clamp(Comp->GetMass() / MaxGrabMassKg, 0.f, 1.f);
	const float FinalSpeed = Speed * FMath::Lerp(1.f, 0.35f, MassAlpha);

	if (ANoclipLootActor* Loot = Cast<ANoclipLootActor>(Comp->GetOwner()))
	{
		Loot->NotifyThrown(OwnerChar->GetPlayerState<ANoclipPlayerState>());
	}
	Comp->AddImpulse(Dir * FinalSpeed, NAME_None, true);
	Comp->AddAngularImpulseInDegrees(FVector(FMath::FRandRange(-1.f, 1.f), FMath::FRandRange(-1.f, 1.f), FMath::FRandRange(-1.f, 1.f)) * 180.f * ClampedCharge, NAME_None, true);
}

void UNoclipGrabComponent::ForceDrop()
{
	if (GetOwner() && GetOwner()->HasAuthority() && HeldActor)
	{
		ReleaseInternal();
	}
}

void UNoclipGrabComponent::ReleaseInternal()
{
	const bool bWasLead = IsLead();
	if (PhysicsHandle && PhysicsHandle->GetGrabbedComponent())
	{
		PhysicsHandle->ReleaseComponent();
	}
	ANoclipLootActor* Loot = Cast<ANoclipLootActor>(HeldActor);

	HeldComponent = nullptr;
	CarrySpeedScale = 1.f;

	HeldActor = nullptr;
	OnRep_HeldActor();

	// Saiu da equipe: o proximo assume a fisica e o peso se redistribui.
	if (IsValid(Loot) && GetOwner() && GetOwner()->HasAuthority())
	{
		Loot->RemoveCarrier(GetNoclipOwner());
		if (bWasLead)
		{
			if (ANoclipCharacter* Next = Loot->GetLeadCarrier())
			{
				if (UNoclipGrabComponent* NextGrab = Next->GetGrabComponent())
				{
					NextGrab->BecomeLead();
				}
			}
		}
	}
}

void UNoclipGrabComponent::OnRep_HeldActor()
{
	const ANoclipCharacter* OwnerChar = GetNoclipOwner();
	if (HeldActor && OwnerChar && OwnerChar->IsLocallyControlled())
	{
		OnGrabFeel(HeldActor);
		const UPrimitiveComponent* Prim = Cast<UPrimitiveComponent>(HeldActor->GetRootComponent());
		const float MassAlpha = Prim ? FMath::Clamp(Prim->GetMass() / FMath::Max(MaxGrabMassKg, 1.f), 0.f, 1.f) : 0.3f;
		OwnerChar->PlayRumble(0.25f + 0.5f * MassAlpha, 0.12f);
	}
	if (!HeldActor)
	{
		ThrowChargeStartTime = -1.0;
	}
}

void UNoclipGrabComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!GetOwner() || !GetOwner()->HasAuthority() || !PhysicsHandle)
	{
		return;
	}

	if (!IsValid(HeldComponent))
	{
		HeldComponent = nullptr;
		if (HeldActor)
		{
			ReleaseInternal(); // objeto destruido
		}
		return;
	}

	const ANoclipCharacter* OwnerChar = GetNoclipOwner();
	if (!OwnerChar)
	{
		return;
	}
	if (const ANoclipLootActor* TeamLoot = Cast<ANoclipLootActor>(HeldActor); TeamLoot && TeamLoot->IsTeamCarry())
	{
		TickTeamCarry(DeltaTime);
		return;
	}

	const FRotator View = OwnerChar->GetAuthoritativeViewRotation();
	const FVector Target = OwnerChar->GetEyeLocation() + View.Vector() * HoldDistance;
	const FRotator TargetRot(0.f, View.Yaw, 0.f);
	PhysicsHandle->SetTargetLocationAndRotation(Target, TargetRot);

	if (FVector::DistSquared(HeldComponent->GetComponentLocation(), Target) > FMath::Square(BreakDistance))
	{
		ReleaseInternal();
	}
}
