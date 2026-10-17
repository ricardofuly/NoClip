// Copyright Fuly Games. All Rights Reserved.

#include "Character/NoclipToolComponent.h"
#include "Character/NoclipCharacter.h"
#include "Character/NoclipCharacterCatalog.h"
#include "Character/NoclipGrabComponent.h"
#include "Game/NoclipPlayerState.h"
#include "World/NoclipToolPickup.h"
#include "NOCLIP.h"
#include "Camera/CameraComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/SpotLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/Controller.h"
#include "GameFramework/GameStateBase.h"
#include "Net/UnrealNetwork.h"
#include "HAL/IConsoleManager.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(NoclipToolComponent)

static TAutoConsoleVariable<int32> CVarNoclipDebugTools(TEXT("noclip.DebugTools"), 0,
	TEXT("Debug (servidor): 1 = da lanterna, celular e 3 kits e machuca (vida 40); 2 = comeca o curativo com o kit na mao; 3 = nivela a mira e recarrega a lanterna. Volta a 0 sozinho."));

namespace
{
	constexpr float BeamCandelas = 42.f;
	constexpr float PickupReach = 320.f;
	/** Duracao do "nao" com a cabeca (s). Mesma curva na camera do dono e no Control Rig. */
	constexpr float HeadShakeSeconds = 0.6f;
}

UNoclipToolComponent::UNoclipToolComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickGroup = TG_PostPhysics;
	SetIsReplicatedByDefault(true);
}

void UNoclipToolComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UNoclipToolComponent, Equipped);
	DOREPLIFETIME(UNoclipToolComponent, Action);
	DOREPLIFETIME(UNoclipToolComponent, bFlashlightOn);
	DOREPLIFETIME(UNoclipToolComponent, BatteryPct);
	DOREPLIFETIME(UNoclipToolComponent, UseStartServerTime);
}

ANoclipCharacter* UNoclipToolComponent::GetCharacter() const
{
	return Cast<ANoclipCharacter>(GetOwner());
}

ANoclipPlayerState* UNoclipToolComponent::GetNoclipPlayerState() const
{
	const ANoclipCharacter* C = GetCharacter();
	return C ? C->GetPlayerState<ANoclipPlayerState>() : nullptr;
}

float UNoclipToolComponent::GetServerTime() const
{
	const UWorld* World = GetWorld();
	const AGameStateBase* GS = World ? World->GetGameState() : nullptr;
	return GS ? static_cast<float>(GS->GetServerWorldTimeSeconds()) : (World ? World->GetTimeSeconds() : 0.f);
}

bool UNoclipToolComponent::Owns(ENoclipTool Tool) const
{
	const ANoclipPlayerState* PS = GetNoclipPlayerState();
	return PS && PS->HasTool(Tool);
}

int32 UNoclipToolComponent::GetMedkitCount() const
{
	const ANoclipPlayerState* PS = GetNoclipPlayerState();
	return PS ? PS->GetMedkitCount() : 0;
}

bool UNoclipToolComponent::IsHoldingHeavyLoot() const
{
	const ANoclipCharacter* C = GetCharacter();
	const UNoclipGrabComponent* Grab = C ? C->GetGrabComponent() : nullptr;
	return Grab && Grab->IsHoldingTwoHanded();
}

float UNoclipToolComponent::GetUseProgress() const
{
	if (Equipped != ENoclipTool::Medkit || Action != ENoclipToolAction::Using)
	{
		return 0.f;
	}
	return FMath::Clamp((GetServerTime() - UseStartServerTime) / NoclipTools::MedkitUseSeconds, 0.f, 1.f);
}

float UNoclipToolComponent::GetHeadShakeAge() const
{
	const UWorld* World = GetWorld();
	return World ? static_cast<float>(World->GetTimeSeconds() - HeadShakeTime) : 100.f;
}

// ---------------------------------------------------------------------------------------------
// Entrada do dono
// ---------------------------------------------------------------------------------------------

void UNoclipToolComponent::RequestSlot(int32 Slot)
{
	const ENoclipTool Tool = NoclipTools::FromSlot(Slot);
	if (Tool == ENoclipTool::None)
	{
		return;
	}
	if (Equipped == Tool)
	{
		Server_Equip(ENoclipTool::None); // guarda no cinto
		return;
	}
	if (!Owns(Tool))
	{
		Deny(ENoclipHandsDenyReason::NotOwned);
		return;
	}
	if (IsHoldingHeavyLoot())
	{
		Deny(ENoclipHandsDenyReason::HandsFullHeavy);
		return;
	}
	Server_Equip(Tool);
}

void UNoclipToolComponent::RequestUse(bool bPressed)
{
	if (!HasToolInHand())
	{
		return;
	}
	if (bPressed)
	{
		if (Equipped == ENoclipTool::Medkit)
		{
			const ANoclipCharacter* C = GetCharacter();
			if (C && C->GetHealth() >= C->GetMaxHealth() - 0.5f)
			{
				Deny(ENoclipHandsDenyReason::AlreadyHealthy);
				return;
			}
		}
		if (Equipped == ENoclipTool::Flashlight && !bFlashlightOn && BatteryPct == 0)
		{
			Deny(ENoclipHandsDenyReason::NoBattery);
			return;
		}
	}
	else if (Equipped != ENoclipTool::Medkit)
	{
		return; // so o kit medico e "segurar para usar"
	}
	Server_Use(bPressed);
}

void UNoclipToolComponent::RequestDrop()
{
	if (HasToolInHand())
	{
		Server_Drop();
	}
}

void UNoclipToolComponent::RequestPickup(ANoclipToolPickup* Pickup)
{
	if (!Pickup)
	{
		return;
	}
	const ENoclipTool Tool = Pickup->GetTool();
	const bool bRoom = Tool == ENoclipTool::Medkit ? GetMedkitCount() < NoclipTools::MaxMedkits : !Owns(Tool);
	if (!bRoom)
	{
		Deny(ENoclipHandsDenyReason::NotOwned);
		return;
	}
	Server_Pickup(Pickup);
}

void UNoclipToolComponent::Deny(ENoclipHandsDenyReason Reason)
{
	LastDenyReason = Reason;
	LastDenyTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
	LocalHeadShake();
	Server_Deny();
	if (ANoclipCharacter* C = GetCharacter())
	{
		C->PlayRumble(0.25f, 0.12f);
	}
}

void UNoclipToolComponent::LocalHeadShake()
{
	HeadShakeTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
}

void UNoclipToolComponent::Server_Deny_Implementation()
{
	Multicast_Deny();
}

void UNoclipToolComponent::Multicast_Deny_Implementation()
{
	const ANoclipCharacter* C = GetCharacter();
	if (C && C->IsLocallyControlled())
	{
		return; // o dono ja balancou a cabeca na hora
	}
	LocalHeadShake();
}

// ---------------------------------------------------------------------------------------------
// Servidor
// ---------------------------------------------------------------------------------------------

void UNoclipToolComponent::Server_Equip_Implementation(ENoclipTool Tool)
{
	if (Tool != ENoclipTool::None && (!Owns(Tool) || IsHoldingHeavyLoot()))
	{
		return;
	}
	Equipped = Tool;
	Action = ENoclipToolAction::Hold;
	// A lanterna acende ao tirar do cinto (se tiver bateria); guardar apaga.
	bFlashlightOn = Tool == ENoclipTool::Flashlight && BatteryPct > 0;
	OnRep_Equipped();
	OnRep_Action();
}

void UNoclipToolComponent::ServerHolster()
{
	if (GetOwner() && GetOwner()->HasAuthority() && Equipped != ENoclipTool::None)
	{
		Server_Equip_Implementation(ENoclipTool::None);
	}
}

void UNoclipToolComponent::Server_Use_Implementation(bool bPressed)
{
	switch (Equipped)
	{
	case ENoclipTool::Flashlight:
		if (bPressed)
		{
			bFlashlightOn = !bFlashlightOn && BatteryPct > 0;
		}
		break;

	case ENoclipTool::Medkit:
		if (bPressed && Action == ENoclipToolAction::Hold)
		{
			const ANoclipCharacter* C = GetCharacter();
			if (C && C->GetHealth() < C->GetMaxHealth() - 0.5f && Owns(ENoclipTool::Medkit))
			{
				Action = ENoclipToolAction::Using;
				UseStartServerTime = GetServerTime();
			}
		}
		else if (!bPressed && Action == ENoclipToolAction::Using)
		{
			Action = ENoclipToolAction::Hold; // soltou antes de terminar: cancela
		}
		break;

	case ENoclipTool::Phone:
		if (bPressed)
		{
			Action = Action == ENoclipToolAction::Looking ? ENoclipToolAction::Hold : ENoclipToolAction::Looking;
		}
		break;

	default:
		break;
	}
	OnRep_Action();
}

void UNoclipToolComponent::FinishMedkit()
{
	ANoclipCharacter* C = GetCharacter();
	ANoclipPlayerState* PS = GetNoclipPlayerState();
	if (C && PS && PS->HasTool(ENoclipTool::Medkit))
	{
		C->Heal(NoclipTools::MedkitHeal);
		PS->TakeTool(ENoclipTool::Medkit);
		UE_LOG(LogNoclip, Log, TEXT("%s usou um kit medico (vida %.0f, restam %d)"), *GetNameSafe(PS), C->GetHealth(), PS->GetMedkitCount());
	}
	Action = ENoclipToolAction::Hold;
	if (!PS || !PS->HasTool(ENoclipTool::Medkit))
	{
		Equipped = ENoclipTool::None;
		OnRep_Equipped();
	}
	OnRep_Action();
}

void UNoclipToolComponent::Server_Drop_Implementation()
{
	ANoclipCharacter* C = GetCharacter();
	ANoclipPlayerState* PS = GetNoclipPlayerState();
	if (!C || !PS || Equipped == ENoclipTool::None)
	{
		return;
	}
	const ENoclipTool Tool = Equipped;
	const FRotator Aim = C->GetAimRotation();
	const FVector Start = C->GetEyeLocation() + Aim.Vector() * 45.f - FVector(0.f, 0.f, 20.f);

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
	const FTransform T(Aim, Start);
	if (ANoclipToolPickup* Pickup = GetWorld()->SpawnActorDeferred<ANoclipToolPickup>(ANoclipToolPickup::StaticClass(), T, nullptr, nullptr, Params.SpawnCollisionHandlingOverride))
	{
		Pickup->Setup(Tool, BatteryPct);
		Pickup->FinishSpawning(T);
		if (UPrimitiveComponent* Prim = Cast<UPrimitiveComponent>(Pickup->GetRootComponent()))
		{
			Prim->SetPhysicsLinearVelocity(Aim.Vector() * 160.f + FVector(0.f, 0.f, 60.f));
		}
	}
	PS->TakeTool(Tool);
	Equipped = ENoclipTool::None;
	Action = ENoclipToolAction::Hold;
	bFlashlightOn = false;
	if (Tool == ENoclipTool::Flashlight)
	{
		BatteryPct = 255; // a proxima lanterna (comprada) vem carregada
		BatteryDrain = 0.f;
	}
	OnRep_Equipped();
	OnRep_Action();
}

void UNoclipToolComponent::Server_Pickup_Implementation(ANoclipToolPickup* Pickup)
{
	ANoclipCharacter* C = GetCharacter();
	ANoclipPlayerState* PS = GetNoclipPlayerState();
	if (!C || !PS || !IsValid(Pickup) || FVector::Dist(C->GetEyeLocation(), Pickup->GetActorLocation()) > PickupReach)
	{
		return;
	}
	const ENoclipTool Tool = Pickup->GetTool();
	const bool bRoom = Tool == ENoclipTool::Medkit ? PS->GetMedkitCount() < NoclipTools::MaxMedkits : !PS->HasTool(Tool);
	if (!bRoom)
	{
		return;
	}
	PS->GiveTool(Tool);
	if (Tool == ENoclipTool::Flashlight)
	{
		BatteryPct = Pickup->GetBattery();
		BatteryDrain = 1.f - BatteryPct / 255.f;
	}
	Pickup->Destroy();
}

// ---------------------------------------------------------------------------------------------
// Tick e visual
// ---------------------------------------------------------------------------------------------

void UNoclipToolComponent::BeginPlay()
{
	Super::BeginPlay();

	ANoclipCharacter* C = GetCharacter();
	if (!C || GetNetMode() == NM_DedicatedServer)
	{
		return;
	}

	// No corpo (os outros veem; o dono so a sombra). Posicao/rotacao absolutas: o osso da mao tem escala 100.
	BodyItem = NewObject<UStaticMeshComponent>(C, TEXT("ToolBodyItem"));
	BodyItem->SetupAttachment(C->GetRootComponent());
	BodyItem->SetUsingAbsoluteLocation(true);
	BodyItem->SetUsingAbsoluteRotation(true);
	BodyItem->SetUsingAbsoluteScale(true);
	BodyItem->SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
	BodyItem->SetOwnerNoSee(true);
	BodyItem->bCastHiddenShadow = true;
	BodyItem->SetVisibility(false);
	BodyItem->RegisterComponent();

	// Primeira pessoa (so o dono ve), preso a camera.
	ViewItem = NewObject<UStaticMeshComponent>(C, TEXT("ToolViewItem"));
	ViewItem->SetupAttachment(C->GetFirstPersonCamera());
	ViewItem->SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
	ViewItem->SetOnlyOwnerSee(true);
	ViewItem->SetCastShadow(false);
	ViewItem->SetVisibility(false);
	ViewItem->RegisterComponent();

	Beam = NewObject<USpotLightComponent>(C, TEXT("ToolBeam"));
	Beam->SetupAttachment(C->IsLocallyControlled() ? ViewItem.Get() : BodyItem.Get());
	Beam->SetIntensityUnits(ELightUnits::Candelas);
	Beam->SetIntensity(BeamCandelas);
	Beam->SetAttenuationRadius(1800.f);
	Beam->SetInnerConeAngle(12.f);
	Beam->SetOuterConeAngle(27.f);
	Beam->SetLightColor(FLinearColor(1.f, 0.93f, 0.8f));
	Beam->SetCastShadows(true);
	Beam->SetVisibility(false);
	Beam->RegisterComponent();

	ApplyVisuals();
}

void UNoclipToolComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	for (UPrimitiveComponent* Comp : TArray<UPrimitiveComponent*>{ BodyItem.Get(), ViewItem.Get() })
	{
		if (Comp)
		{
			Comp->DestroyComponent();
		}
	}
	if (Beam)
	{
		Beam->DestroyComponent();
	}
	Super::EndPlay(EndPlayReason);
}

void UNoclipToolComponent::SetBodyVisibleToOwner(bool bVisible)
{
	if (BodyItem)
	{
		BodyItem->SetOwnerNoSee(!bVisible);
	}
	if (ViewItem)
	{
		ViewItem->SetHiddenInGame(bVisible);
	}
}

void UNoclipToolComponent::OnRep_Equipped()
{
	ViewBlend = 0.f; // sobe da cintura
	ApplyVisuals();
}

void UNoclipToolComponent::OnRep_Action()
{
}

void UNoclipToolComponent::ApplyVisuals()
{
	VisualTool = Equipped;
	if (!BodyItem || !ViewItem)
	{
		return;
	}
	const bool bHas = Equipped != ENoclipTool::None;
	UStaticMesh* Mesh = bHas ? NoclipTools::LoadMesh(Equipped) : nullptr;
	const float Scale = NoclipTools::Get(Equipped).MeshScale;
	BodyItem->SetStaticMesh(Mesh);
	BodyItem->SetWorldScale3D(FVector(Scale));
	BodyItem->SetVisibility(bHas && Mesh);
	ViewItem->SetStaticMesh(Mesh);
	ViewItem->SetRelativeScale3D(FVector(Scale));
	ViewItem->SetVisibility(bHas && Mesh);

	// Luz sai da lente: +Z do modelo da lanterna (a ponta fica a ~10 cm do centro).
	if (Beam)
	{
		Beam->SetRelativeLocation(FVector(0.f, 0.f, 11.f / FMath::Max(Scale, 0.01f)));
		Beam->SetRelativeRotation(FRotator(90.f, 0.f, 0.f));
	}
}

void UNoclipToolComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (GetOwner() && GetOwner()->HasAuthority())
	{
		if (CVarNoclipDebugTools.GetValueOnGameThread() == 2 && Equipped == ENoclipTool::Medkit)
		{
			// Simula "segurar para curar" (o teste automatico so aperta e solta).
			Action = ENoclipToolAction::Using;
			UseStartServerTime = GetServerTime();
			CVarNoclipDebugTools->Set(0, ECVF_SetByConsole);
		}
		else if (CVarNoclipDebugTools.GetValueOnGameThread() == 3)
		{
			// Nivela a mira (teste automatico nao mexe o mouse) e recarrega a lanterna.
			if (ANoclipCharacter* C = GetCharacter())
			{
				if (AController* PC = C->GetController())
				{
					FRotator R = PC->GetControlRotation();
					R.Pitch = 0.f;
					PC->SetControlRotation(R);
				}
			}
			BatteryPct = 255;
			BatteryDrain = 0.f;
			CVarNoclipDebugTools->Set(0, ECVF_SetByConsole);
		}
		else if (CVarNoclipDebugTools.GetValueOnGameThread() == 1)
		{
			if (ANoclipPlayerState* PS = GetNoclipPlayerState())
			{
				PS->GiveTool(ENoclipTool::Flashlight);
				PS->GiveTool(ENoclipTool::Phone);
				PS->GiveTool(ENoclipTool::Medkit, NoclipTools::MaxMedkits);
			}
			if (ANoclipCharacter* C = GetCharacter())
			{
				C->ApplyHurt(C->GetHealth() - 40.f);
			}
			BatteryPct = 255;
			BatteryDrain = 0.f;
			CVarNoclipDebugTools->Set(0, ECVF_SetByConsole);
		}
		// Posse mudou por fora (colapso tirou a lanterna, kit acabou): guarda.
		if (Equipped != ENoclipTool::None && !Owns(Equipped))
		{
			ServerHolster();
		}
		if (bFlashlightOn)
		{
			BatteryDrain = FMath::Min(1.f, BatteryDrain + DeltaTime / NoclipTools::FlashlightBatterySeconds);
			const uint8 NewPct = static_cast<uint8>(FMath::RoundToInt(255.f * (1.f - BatteryDrain)));
			if (NewPct != BatteryPct)
			{
				BatteryPct = NewPct;
			}
			if (BatteryPct == 0)
			{
				bFlashlightOn = false;
			}
		}
		if (Equipped == ENoclipTool::Medkit && Action == ENoclipToolAction::Using && GetUseProgress() >= 1.f)
		{
			FinishMedkit();
		}
	}

	UpdateVisuals(DeltaTime);
}

void UNoclipToolComponent::UpdateVisuals(float DeltaTime)
{
	if (!BodyItem || !ViewItem)
	{
		return;
	}
	if (VisualTool != Equipped)
	{
		ApplyVisuals();
	}
	ANoclipCharacter* C = GetCharacter();
	if (!C || Equipped == ENoclipTool::None)
	{
		if (Beam)
		{
			Beam->SetVisibility(false);
		}
		return;
	}

	const NoclipTools::FToolDef& Def = NoclipTools::Get(Equipped);
	const FRotator Aim = C->GetAimRotation();
	const FQuat AimQ = Aim.Quaternion();

	// ---------- Corpo: na mao direita (kit medico entre as duas maos) ----------
	if (USkeletalMeshComponent* Body = C->GetCharacterMesh())
	{
		const FNoclipCharacterDef& CharDef = NoclipCharacters::Get(C->GetAppliedCharacter());
		FVector Hand = Body->GetSocketLocation(CharDef.HandR);
		if (Equipped == ENoclipTool::Medkit)
		{
			Hand = (Hand + Body->GetSocketLocation(CharDef.HandL)) * 0.5f;
		}
		const FQuat ItemQ = AimQ * Def.AimRotation.Quaternion();
		BodyItem->SetWorldLocationAndRotation(Hand + AimQ.RotateVector(Def.GripOffset), ItemQ);
	}

	// ---------- Primeira pessoa ----------
	const bool bLooking = IsPhoneRaised();
	ViewBlend = FMath::FInterpTo(ViewBlend, 1.f, DeltaTime, 9.f);
	const float Speed = C->GetVelocity().Size2D();
	ViewBob += DeltaTime * (2.f + Speed / 70.f);
	FVector Offset = Def.ViewOffset;
	Offset.Z += FMath::Sin(ViewBob * 2.f) * FMath::Clamp(Speed / 420.f, 0.f, 1.f) * 0.8f; // passo
	Offset.Z -= (1.f - ViewBlend) * 22.f;                                                  // subindo da cintura
	FRotator Rot = Def.ViewRotation;
	if (Equipped == ENoclipTool::Medkit && Action == ENoclipToolAction::Using)
	{
		// Curativo: o kit desce para o peito e "bate" no ritmo do curativo.
		Offset = FVector(30.f, 2.f, -27.f + FMath::Abs(FMath::Sin(GetUseProgress() * PI * 5.f)) * 3.f);
		Rot.Pitch += 25.f;
	}
	ViewItem->SetRelativeLocationAndRotation(Offset, Rot);
	ViewItem->SetVisibility(!bLooking); // celular levantado vira a tela 2D da HUD

	// ---------- Lanterna ----------
	if (Beam)
	{
		// O dono acende a partir da camera (o facho vai para a mira); os outros, da lente da lanterna na mao.
		// (No BeginPlay do cliente o pawn ainda pode nao estar possuido, por isso decide aqui.)
		const bool bOwnerView = C->IsLocallyControlled();
		USceneComponent* WantParent = bOwnerView ? static_cast<USceneComponent*>(C->GetFirstPersonCamera()) : static_cast<USceneComponent*>(BodyItem);
		if (Beam->GetAttachParent() != WantParent)
		{
			Beam->AttachToComponent(WantParent, FAttachmentTransformRules::KeepRelativeTransform);
		}
		if (bOwnerView)
		{
			Beam->SetRelativeLocationAndRotation(FVector(20.f, 12.f, -10.f), FRotator::ZeroRotator);
		}
		else
		{
			Beam->SetRelativeLocationAndRotation(FVector(0.f, 0.f, 11.f / FMath::Max(NoclipTools::Get(Equipped).MeshScale, 0.01f)), FRotator(90.f, 0.f, 0.f));
		}
		bool bOn = Equipped == ENoclipTool::Flashlight && bFlashlightOn;
		const float Battery = GetBattery();
		float Scale = FMath::Lerp(0.55f, 1.f, FMath::Clamp(Battery / 0.5f, 0.f, 1.f));
		if (bOn && Battery < NoclipTools::FlashlightLowBattery)
		{
			// "Pisca quando mais precisa" (GDD): falhas curtas e aleatorias com a bateria no fim.
			FlickerTime -= DeltaTime;
			if (FlickerTime <= -0.12f)
			{
				FlickerTime = FMath::FRandRange(0.4f, 2.2f) * (0.3f + Battery / NoclipTools::FlashlightLowBattery);
			}
			bOn = FlickerTime > 0.f;
		}
		Beam->SetVisibility(bOn);
		Beam->SetIntensity(BeamCandelas * Scale);
	}
}
