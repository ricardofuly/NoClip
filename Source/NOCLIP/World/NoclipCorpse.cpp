// Copyright Fuly Games. All Rights Reserved.

#include "World/NoclipCorpse.h"
#include "Character/NoclipCharacterCatalog.h"
#include "Animation/AnimSequence.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/World.h"
#include "GameFramework/PlayerState.h"
#include "Net/UnrealNetwork.h"
#include "PhysicsEngine/PhysicsAsset.h"
#include "TimerManager.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(NoclipCorpse)

ANoclipCorpse::ANoclipCorpse()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
	bAlwaysRelevant = true;
	SetReplicatingMovement(false);

	Mesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("Mesh"));
	RootComponent = Mesh;
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Mesh->SetGenerateOverlapEvents(false);
	Mesh->SetCanEverAffectNavigation(false);
	Mesh->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
}

void ANoclipCorpse::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME_CONDITION(ANoclipCorpse, OwnerPlayer, COND_InitialOnly);
	DOREPLIFETIME_CONDITION(ANoclipCorpse, CharacterIndex, COND_InitialOnly);
	DOREPLIFETIME_CONDITION(ANoclipCorpse, Impulse, COND_InitialOnly);
}

void ANoclipCorpse::Setup(APlayerState* InOwner, int32 InCharacterIndex, const FVector& InImpulse)
{
	OwnerPlayer = InOwner;
	CharacterIndex = NoclipCharacters::ClampPlayable(InCharacterIndex);
	Impulse = InImpulse;
}

FVector ANoclipCorpse::GetBodyLocation() const
{
	if (Mesh && Mesh->IsSimulatingPhysics())
	{
		return Mesh->Bounds.Origin;
	}
	return GetActorLocation() + FVector(0.f, 0.f, 40.f);
}

void ANoclipCorpse::BeginPlay()
{
	Super::BeginPlay();

	if (GetNetMode() == NM_DedicatedServer)
	{
		return;
	}
	USkeletalMesh* NewMesh = NoclipCharacters::LoadMesh(CharacterIndex);
	if (!NewMesh)
	{
		return;
	}
	Mesh->SetSkeletalMesh(NewMesh, true);

	// Comeca na pose parada (sem o T da pose de referencia) e cai no quadro seguinte.
	if (UAnimSequence* Idle = NoclipCharacters::LoadAnim(CharacterIndex, ENoclipCharacterAnim::Idle))
	{
		Mesh->SetAnimationMode(EAnimationMode::AnimationSingleNode);
		Mesh->PlayAnimation(Idle, false);
		Mesh->SetPosition(0.f, false);
		Mesh->Stop();
	}
	GetWorldTimerManager().SetTimerForNextTick(this, &ANoclipCorpse::StartRagdoll);
}

void ANoclipCorpse::StartRagdoll()
{
	if (!Mesh || !Mesh->GetPhysicsAsset())
	{
		// Sem physics asset: so deita o modelo no chao.
		SetActorRotation(GetActorRotation() + FRotator(0.f, 0.f, 90.f));
		SetActorLocation(GetActorLocation() + FVector(0.f, 0.f, 20.f));
		return;
	}
	Mesh->SetCollisionProfileName(TEXT("Ragdoll"));
	Mesh->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
	Mesh->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
	Mesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	Mesh->SetAllBodiesSimulatePhysics(true);
	Mesh->SetSimulatePhysics(true);
	Mesh->WakeAllRigidBodies();
	Mesh->bBlendPhysics = true;
	const FVector V = Impulse;
	if (!V.IsNearlyZero())
	{
		Mesh->SetAllPhysicsLinearVelocity(V);
	}
}
