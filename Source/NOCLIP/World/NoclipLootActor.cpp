// Copyright Fuly Games. All Rights Reserved.

#include "World/NoclipLootActor.h"
#include "Enemy/NoclipEnemy.h"
#include "Enemy/NoclipNoiseSubsystem.h"
#include "Components/StaticMeshComponent.h"
#include "Net/UnrealNetwork.h"
#include "Engine/World.h"
#include "Game/NoclipPlayerState.h"
#include "Character/NoclipCharacter.h"
#include "Character/NoclipGrabComponent.h"
#include "World/NoclipLootCatalog.h"
#include "Engine/StaticMesh.h"
#include "Math/RandomStream.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(NoclipLootActor)

ANoclipLootActor::ANoclipLootActor()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
	SetReplicatingMovement(true);

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	Mesh->SetCollisionProfileName(TEXT("PhysicsActor"));
	Mesh->SetSimulatePhysics(true);
	Mesh->SetNotifyRigidBodyCollision(true);
	Mesh->SetLinearDamping(0.05f);
	Mesh->SetAngularDamping(0.2f);
	RootComponent = Mesh;
}

void ANoclipLootActor::SetupFromCatalog(int32 Index, FRandomStream& Stream)
{
	const FNoclipLootDef* Def = NoclipLoot::Get(Index);
	if (!Def)
	{
		return;
	}
	CatalogIndex = static_cast<int16>(Index);
	ApplyCatalog();
	BaseValue = Stream.RandRange(Def->MinValue, Def->MaxValue);
}

void ANoclipLootActor::OnRep_CatalogIndex()
{
	ApplyCatalog();
}

void ANoclipLootActor::ApplyCatalog()
{
	const FNoclipLootDef* Def = NoclipLoot::Get(CatalogIndex);
	if (!Def)
	{
		return;
	}
	DisplayName = Def->Name;
	Description = Def->Description;
	MassKg = Def->MassKg;
	RequiredCarriers = FMath::Clamp(Def->Carriers, 1, 4);
	Fragility = Def->Fragility;
	if (UStaticMesh* LootMesh = NoclipLoot::LoadMesh(CatalogIndex))
	{
		if (Mesh->GetStaticMesh() != LootMesh)
		{
			Mesh->SetStaticMesh(LootMesh);
		}
	}
	Mesh->SetWorldScale3D(FVector(Def->Scale));
	if (Mesh->IsRegistered())
	{
		Mesh->SetMassOverrideInKg(NAME_None, MassKg, true);
	}
}

void ANoclipLootActor::BeginPlay()
{
	Super::BeginPlay();
	ApplyCatalog();

	Mesh->SetMassOverrideInKg(NAME_None, MassKg, true);

	if (HasAuthority())
	{
		CurrentValue = BaseValue;
		Mesh->OnComponentHit.AddDynamic(this, &ANoclipLootActor::HandleHit);
	}
}

void ANoclipLootActor::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ANoclipLootActor, CurrentValue);
	DOREPLIFETIME(ANoclipLootActor, BaseValue);
	DOREPLIFETIME(ANoclipLootActor, CatalogIndex);
	DOREPLIFETIME(ANoclipLootActor, NumCarriers);
}

void ANoclipLootActor::AddCarrier(ANoclipCharacter* Carrier)
{
	if (HasAuthority() && Carrier && !IsCarriedBy(Carrier))
	{
		Carriers.Add(Carrier);
		RefreshCarriers();
	}
}

void ANoclipLootActor::RemoveCarrier(ANoclipCharacter* Carrier)
{
	if (HasAuthority())
	{
		Carriers.RemoveAll([Carrier](const TWeakObjectPtr<ANoclipCharacter>& C) { return !C.IsValid() || C.Get() == Carrier; });
		RefreshCarriers();
	}
}

bool ANoclipLootActor::IsCarriedBy(const ANoclipCharacter* Carrier) const
{
	return Carriers.ContainsByPredicate([Carrier](const TWeakObjectPtr<ANoclipCharacter>& C) { return C.Get() == Carrier; });
}

ANoclipCharacter* ANoclipLootActor::GetLeadCarrier() const
{
	for (const TWeakObjectPtr<ANoclipCharacter>& C : Carriers)
	{
		if (C.IsValid())
		{
			return C.Get();
		}
	}
	return nullptr;
}

void ANoclipLootActor::RefreshCarriers()
{
	Carriers.RemoveAll([](const TWeakObjectPtr<ANoclipCharacter>& C) { return !C.IsValid() || C->IsActorBeingDestroyed(); });
	const int32 Count = Carriers.Num();
	if (NumCarriers != Count)
	{
		NumCarriers = static_cast<uint8>(Count);
		ForceNetUpdate();
	}
	// Peso dividido: cada um sente so a sua parte (e todo mundo arrasta devagar enquanto falta gente).
	for (const TWeakObjectPtr<ANoclipCharacter>& C : Carriers)
	{
		if (UNoclipGrabComponent* Grab = C->GetGrabComponent())
		{
			Grab->ApplyCarryShare(Count, RequiredCarriers, MassKg);
		}
	}
}

FVector ANoclipLootActor::ComputeTeamTarget() const
{
	FVector Sum = FVector::ZeroVector;
	float FeetZ = 0.f;
	int32 N = 0;
	for (const TWeakObjectPtr<ANoclipCharacter>& C : Carriers)
	{
		if (const UNoclipGrabComponent* Grab = C.IsValid() ? C->GetGrabComponent() : nullptr)
		{
			Sum += Grab->GetHoldTarget();
			FeetZ += C->GetActorLocation().Z - 88.f;
			++N;
		}
	}
	if (N == 0)
	{
		return GetActorLocation();
	}
	FVector Target = Sum / N;
	// Na altura da cintura: o achado inteiro fica um pouco acima do chao (sem levantar ate os olhos).
	const float HalfHeight = Mesh ? Mesh->Bounds.BoxExtent.Z : 40.f;
	Target.Z = FeetZ / N + FMath::Max(HalfHeight + 18.f, 70.f);
	return Target;
}

void ANoclipLootActor::NotifyGrabbed(ANoclipPlayerState* Holder)
{
	LastHolder = Holder;
	PendingThrower.Reset();
	ThrowTime = -1.0;
}

void ANoclipLootActor::NotifyThrown(ANoclipPlayerState* Thrower)
{
	PendingThrower = Thrower;
	ThrowOrigin = GetActorLocation();
	ThrowTime = GetWorld()->GetTimeSeconds();
}

void ANoclipLootActor::HandleHit(UPrimitiveComponent* HitComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit)
{
	const double Now = GetWorld()->GetTimeSeconds();

	// Primeiro impacto depois de um arremesso: mede a distancia no plano ("Maior arremesso").
	if (ThrowTime >= 0.0 && Now - ThrowTime > 0.05)
	{
		if (ANoclipPlayerState* Thrower = PendingThrower.Get())
		{
			const float Dist = FVector::Dist2D(ThrowOrigin, GetActorLocation());
			if (Dist > Thrower->LongestThrowCm)
			{
				Thrower->LongestThrowCm = Dist;
				Thrower->LongestThrowItem = DisplayName;
			}
		}
		PendingThrower.Reset();
		ThrowTime = -1.0;
	}
	// Variacao de velocidade causada pelo impacto.
	const float DeltaV = NormalImpulse.Size() / FMath::Max(Mesh->GetMass(), 0.1f);

	// Achado arremessado na cara de uma entidade: atordoa.
	if (ANoclipEnemy* Enemy = Cast<ANoclipEnemy>(OtherActor))
	{
		Enemy->NotifyStruck(this, FMath::Max(DeltaV, Mesh->GetPhysicsLinearVelocity().Size()));
	}

	// Batida faz barulho (os caes escutam): achado pesado e rapido se ouve de longe. Vira isca.
	if (DeltaV > 220.f && Now - LastNoiseTime > 0.2)
	{
		LastNoiseTime = Now;
		const float MassFactor = FMath::Clamp(Mesh->GetMass() / 10.f, 0.6f, 2.f);
		UNoclipNoiseSubsystem::Report(this, Hit.ImpactPoint, FMath::Clamp(DeltaV * 1.4f * MassFactor, 400.f, 2600.f), this, false);
	}

	if (Now - LastDamageTime < 0.15 || CurrentValue <= 0)
	{
		return;
	}
	if (DeltaV <= DamageSpeedThreshold)
	{
		return;
	}

	const float Loss = ((DeltaV - DamageSpeedThreshold) / 1000.f) * Fragility * BaseValue;
	const int32 LossInt = FMath::Clamp(FMath::CeilToInt(Loss), 1, CurrentValue);
	const int32 Old = CurrentValue;
	CurrentValue -= LossInt;
	LastDamageTime = Now;

	if (ANoclipPlayerState* Holder = LastHolder.Get())
	{
		Holder->DamageCaused += LossInt;
	}

	OnRep_CurrentValue(Old);
	Multicast_ImpactFeedback(LossInt, Hit.ImpactPoint);
}

void ANoclipLootActor::OnRep_CurrentValue(int32 OldValue)
{
	// Hook para UI; o feedback visual vem do multicast.
}

void ANoclipLootActor::Multicast_ImpactFeedback_Implementation(int32 Lost, FVector_NetQuantize ImpactLocation)
{
	OnValueLost(Lost, ImpactLocation);
}
