// Copyright Fuly Games. All Rights Reserved.

#include "World/NoclipToolPickup.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/StaticMesh.h"
#include "Net/UnrealNetwork.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(NoclipToolPickup)

ANoclipToolPickup::ANoclipToolPickup()
{
	bReplicates = true;
	SetReplicatingMovement(true);

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	Mesh->SetCollisionProfileName(UCollisionProfile::PhysicsActor_ProfileName);
	Mesh->SetSimulatePhysics(false); // liga no servidor depois de escolher o modelo
	Mesh->SetCanEverAffectNavigation(false);
	Mesh->BodyInstance.bOverrideMass = true;
	Mesh->BodyInstance.SetMassOverride(0.6f);
	RootComponent = Mesh;
}

void ANoclipToolPickup::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ANoclipToolPickup, Tool);
	DOREPLIFETIME(ANoclipToolPickup, Battery);
}

void ANoclipToolPickup::Setup(ENoclipTool InTool, uint8 InBattery)
{
	Tool = InTool;
	Battery = InBattery;
	OnRep_Tool();
	if (HasAuthority())
	{
		Mesh->SetSimulatePhysics(true);
	}
}

void ANoclipToolPickup::OnRep_Tool()
{
	const NoclipTools::FToolDef& Def = NoclipTools::Get(Tool);
	Mesh->SetStaticMesh(NoclipTools::LoadMesh(Tool));
	Mesh->SetWorldScale3D(FVector(Def.MeshScale));
}
