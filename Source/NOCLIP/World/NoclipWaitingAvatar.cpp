// Copyright Fuly Games. All Rights Reserved.

#include "World/NoclipWaitingAvatar.h"
#include "World/NoclipWaitingRoom.h"
#include "Character/NoclipCharacterAnimInstance.h"
#include "Character/NoclipCharacterCatalog.h"
#include "Game/NoclipPlayerState.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/SkeletalMesh.h"
#include "GameFramework/PlayerState.h"
#include "Net/UnrealNetwork.h"
#include "HAL/IConsoleManager.h"
#include "NOCLIP.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(NoclipWaitingAvatar)

ANoclipWaitingAvatar::ANoclipWaitingAvatar()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;
	bAlwaysRelevant = true;
	SetReplicatingMovement(false);

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;

	Mesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("Mesh"));
	Mesh->SetupAttachment(Root);
	Mesh->SetRelativeRotation(FRotator(0.f, -90.f, 0.f));
	Mesh->SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
	Mesh->SetGenerateOverlapEvents(false);
	Mesh->SetCanEverAffectNavigation(false);
	// A tela principal nao desenha o mundo na sala de espera (so a camera de seguranca): anima mesmo assim.
	Mesh->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
	Mesh->SetAnimationMode(EAnimationMode::AnimationBlueprint);
	Mesh->AnimClass = UNoclipCharacterAnimInstance::StaticClass();
}

void ANoclipWaitingAvatar::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ANoclipWaitingAvatar, Room);
	DOREPLIFETIME(ANoclipWaitingAvatar, OwnerPlayer);
	DOREPLIFETIME(ANoclipWaitingAvatar, Slot);
}

void ANoclipWaitingAvatar::Setup(ANoclipWaitingRoom* InRoom, APlayerState* InPlayer, int32 InSlot)
{
	Room = InRoom;
	OwnerPlayer = InPlayer;
	Slot = InSlot;
	OnRep_Slot();
}

void ANoclipWaitingAvatar::OnRep_Slot()
{
	SyncCharacter();
}

void ANoclipWaitingAvatar::SyncCharacter()
{
	if (!Mesh)
	{
		return;
	}
	const ANoclipPlayerState* PS = Cast<ANoclipPlayerState>(OwnerPlayer);
	const int32 Wanted = NoclipCharacters::ClampPlayable(PS ? PS->GetCharacterIndex() : 0);
	if (Wanted != AppliedCharacter)
	{
		if (USkeletalMesh* NewMesh = NoclipCharacters::LoadMesh(Wanted))
		{
			AppliedCharacter = Wanted;
			Mesh->SetRelativeRotation(FRotator(0.f, NoclipCharacters::Get(Wanted).MeshYaw, 0.f));
			Mesh->SetSkeletalMesh(NewMesh, true);
		}
	}
	// A instancia de animacao so existe com o componente registrado (pode nascer depois do Setup).
	UNoclipCharacterAnimInstance* Anim = Cast<UNoclipCharacterAnimInstance>(Mesh->GetAnimInstance());
	if (Anim && AppliedCharacter >= 0 && Anim->GetCharacterIndex() != AppliedCharacter)
	{
		Anim->SetCharacter(AppliedCharacter);
		Anim->SetPhaseOffset(Slot * 1.37f);
	}
}

void ANoclipWaitingAvatar::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	SyncCharacter();
	if (!Room)
	{
		return;
	}

	// A sala tira a foto da camera depois que os bonecos se moveram neste quadro.
	if (!bTickLinked)
	{
		Room->AddTickPrerequisiteActor(this);
		bTickLinked = true;
	}

	FVector Loc;
	FRotator Rot;
	bool bWalking = false;
	float Speed = 0.f;
	Room->EvaluateAvatar(Slot, Loc, Rot, bWalking, bInCabin, &Speed);

	// Diagnostico (noclip.DebugWaitingWalk 2): avisa se o boneco andou mais do que devia neste quadro.
	static IConsoleVariable* DebugVar = IConsoleManager::Get().FindConsoleVariable(TEXT("noclip.DebugWaitingWalk"));
	if (DebugVar && (DebugVar->GetInt() & 2) && DeltaSeconds > KINDA_SMALL_NUMBER && bHadLocation)
	{
		const float Moved = FVector::Dist2D(Loc, GetActorLocation());
		const float Allowed = FMath::Max(Room->WalkSpeed * 1.5f * DeltaSeconds, 4.f);
		if (Moved > Allowed)
		{
			UE_LOG(LogNoclip, Warning, TEXT("[SalaEspera] lugar %d pulou %.1f cm em %.3f s (fase %d, %s)"),
				Slot, Moved, DeltaSeconds, static_cast<int32>(Room->GetPhase()), HasAuthority() ? TEXT("servidor") : TEXT("cliente"));
		}
	}
	bHadLocation = true;

	// O caminho ja vem com curvas: a rotacao acompanha de perto (parado, vira mais devagar para a porta).
	const FRotator Smoothed = FMath::RInterpTo(GetActorRotation(), Rot, DeltaSeconds, bWalking ? 12.f : 6.f);
	SetActorLocationAndRotation(Loc, Smoothed);

	// A animacao recebe a velocidade do caminho, sempre para a frente (medir pelo deslocamento fazia o boneco
	// "patinar" de lado nas curvas e trocar de animacao aos trancos).
	if (UNoclipCharacterAnimInstance* Anim = Mesh ? Cast<UNoclipCharacterAnimInstance>(Mesh->GetAnimInstance()) : nullptr)
	{
		Anim->SetVelocityOverride(true, FVector(Speed, 0.f, 0.f));
	}
}
