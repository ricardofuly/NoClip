// Copyright Fuly Games. All Rights Reserved.

#include "Character/NoclipCharacter.h"
#include "Character/NoclipGrabComponent.h"
#include "Character/NoclipToolComponent.h"
#include "Enemy/NoclipNoiseSubsystem.h"
#include "Net/UnrealNetwork.h"
#include "HAL/IConsoleManager.h"
#include "Character/NoclipCharacterAnimInstance.h"
#include "Character/NoclipCharacterCatalog.h"
#include "Game/NoclipPlayerState.h"
#include "Game/NoclipGameState.h"
#include "Game/NoclipGameMode.h"
#include "Audio/NoclipAudioSubsystem.h"
#include "World/NoclipPowerGenerator.h"
#include "DefaultMovementSet/InstantMovementEffects/BasicInstantMovementEffects.h"
#include "EngineUtils.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Game/NoclipPlayerController.h"
#include "Game/NoclipUserSettings.h"
#include "Movement/NoclipMoverComponent.h"
#include "Movement/NoclipMoverInputs.h"
#include "MoverDataModelTypes.h"
#include "DefaultMovementSet/Settings/CommonLegacyMovementSettings.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Camera/CameraComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "GameFramework/PlayerController.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "Engine/LocalPlayer.h"
#include "UObject/ConstructorHelpers.h"
#include "Engine/CollisionProfile.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(NoclipCharacter)

static TAutoConsoleVariable<int32> CVarNoclipThirdPerson(TEXT("noclip.ThirdPerson"), 0,
	TEXT("Debug: camera atras do estagiario e corpo visivel para o dono (ver poses do Control Rig)."));

ANoclipCharacter::ANoclipCharacter(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;
	SetReplicatingMovement(false); // o Mover cuida da replicacao de movimento

	bUseControllerRotationYaw = false;
	bUseControllerRotationPitch = false;
	bUseControllerRotationRoll = false;

	CapsuleComponent = CreateDefaultSubobject<UCapsuleComponent>(TEXT("Capsule"));
	CapsuleComponent->InitCapsuleSize(34.f, 88.f);
	CapsuleComponent->SetCollisionProfileName(UCollisionProfile::Pawn_ProfileName);
	CapsuleComponent->SetShouldUpdatePhysicsVolume(true);
	CapsuleComponent->SetCanEverAffectNavigation(false);
	RootComponent = CapsuleComponent;

	BodyMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BodyMesh"));
	BodyMesh->SetupAttachment(CapsuleComponent);
	BodyMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	BodyMesh->SetOwnerNoSee(true);
	BodyMesh->SetRelativeScale3D(FVector(0.6f, 0.6f, 1.7f));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderMesh(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	if (CylinderMesh.Succeeded())
	{
		BodyMesh->SetStaticMesh(CylinderMesh.Object);
	}

	CharacterMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("CharacterMesh"));
	CharacterMesh->SetupAttachment(CapsuleComponent);
	CharacterMesh->SetRelativeLocation(FVector(0.f, 0.f, -88.f)); // pes na base da capsula
	CharacterMesh->SetRelativeRotation(FRotator(0.f, -90.f, 0.f));
	CharacterMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	CharacterMesh->SetGenerateOverlapEvents(false);
	CharacterMesh->SetCanEverAffectNavigation(false);
	CharacterMesh->SetOwnerNoSee(true);    // primeira pessoa: o dono nao ve o proprio corpo...
	CharacterMesh->bCastHiddenShadow = true; // ...mas ve a sombra
	CharacterMesh->SetAnimationMode(EAnimationMode::AnimationBlueprint);
	CharacterMesh->AnimClass = UNoclipCharacterAnimInstance::StaticClass();

	FirstPersonCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FirstPersonCamera"));
	FirstPersonCamera->SetupAttachment(CapsuleComponent);
	FirstPersonCamera->SetRelativeLocation(FVector(0.f, 0.f, EyeHeight));
	FirstPersonCamera->bUsePawnControlRotation = false; // rotacao aplicada em UpdateCameraFeel (com roll)
	FirstPersonCamera->SetFieldOfView(BaseFOV);

	MoverComponent = CreateDefaultSubobject<UNoclipMoverComponent>(TEXT("MoverComponent"));

	GrabComponent = CreateDefaultSubobject<UNoclipGrabComponent>(TEXT("GrabComponent"));

	ToolComponent = CreateDefaultSubobject<UNoclipToolComponent>(TEXT("ToolComponent"));

	// Acao nova (gamepad): padrao nativo para o BP ja existente pegar sem precisar editar.
	static ConstructorHelpers::FObjectFinder<UInputAction> LookStickFinder(TEXT("/Game/NOCLIP/Input/IA_LookStick.IA_LookStick"));
	if (LookStickFinder.Succeeded())
	{
		LookStickAction = LookStickFinder.Object;
	}
}

void ANoclipCharacter::BeginPlay()
{
	Super::BeginPlay();

	ApplyMovementSettings();
	SyncCharacterFromPlayerState();

	CurrentFOV = BaseFOV;
	FirstPersonCamera->SetFieldOfView(BaseFOV);

	if (APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		if (PC->PlayerCameraManager)
		{
			PC->PlayerCameraManager->ViewPitchMax = 85.f;
			PC->PlayerCameraManager->ViewPitchMin = -85.f;
		}
	}
}

void ANoclipCharacter::ApplyMovementSettings()
{
	// Todas as maquinas aplicam os mesmos valores, entao a simulacao continua deterministica.
	if (UCommonLegacyMovementSettings* Settings = MoverComponent->FindSharedSettings_Mutable<UCommonLegacyMovementSettings>())
	{
		Settings->MaxSpeed = WalkSpeed;
		Settings->Acceleration = GroundAcceleration;
		Settings->Deceleration = GroundDeceleration;
		Settings->JumpUpwardsSpeed = JumpUpwardsSpeed;
		Settings->bShouldRemainVertical = true;
		Settings->TurningRate = -1.f; // primeira pessoa: vira instantaneamente para a camera
	}
}

void ANoclipCharacter::PawnClientRestart()
{
	Super::PawnClientRestart();

	if (const APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer()))
		{
			if (DefaultMappingContext)
			{
				Subsystem->RemoveMappingContext(DefaultMappingContext);
				Subsystem->AddMappingContext(DefaultMappingContext, 0);
			}
		}
	}
	AddToolMappingContext();
}

void ANoclipCharacter::AddToolMappingContext()
{
	const APlayerController* PC = Cast<APlayerController>(GetController());
	UEnhancedInputLocalPlayerSubsystem* Subsystem = PC ? ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer()) : nullptr;
	if (!Subsystem || !ToolUseAction)
	{
		return;
	}
	if (!ToolMappingContext)
	{
		// Cinto de ferramentas: 1 lanterna / 2 kit medico / 3 celular, botao direito usa, G solta.
		ToolMappingContext = NewObject<UInputMappingContext>(this, TEXT("IMC_Tools"));
		const FKey SlotKeys[3] = { EKeys::One, EKeys::Two, EKeys::Three };
		const FKey SlotPad[3] = { EKeys::Gamepad_DPad_Left, EKeys::Gamepad_DPad_Up, EKeys::Gamepad_DPad_Right };
		for (int32 i = 0; i < 3; ++i)
		{
			ToolMappingContext->MapKey(ToolSlotActions[i], SlotKeys[i]);
			ToolMappingContext->MapKey(ToolSlotActions[i], SlotPad[i]);
		}
		ToolMappingContext->MapKey(ToolUseAction, EKeys::RightMouseButton);
		ToolMappingContext->MapKey(ToolUseAction, EKeys::F);
		ToolMappingContext->MapKey(ToolUseAction, EKeys::Gamepad_LeftShoulder);
		ToolMappingContext->MapKey(ToolDropAction, EKeys::G);
		ToolMappingContext->MapKey(ToolDropAction, EKeys::Gamepad_DPad_Down);
	}
	Subsystem->RemoveMappingContext(ToolMappingContext);
	Subsystem->AddMappingContext(ToolMappingContext, 1);
}

void ANoclipCharacter::SetupToolInput(UEnhancedInputComponent* Input)
{
	if (!ToolUseAction)
	{
		for (int32 i = 0; i < 3; ++i)
		{
			ToolSlotActions[i] = NewObject<UInputAction>(this, *FString::Printf(TEXT("IA_ToolSlot%d"), i + 1));
			ToolSlotActions[i]->ValueType = EInputActionValueType::Boolean;
		}
		ToolUseAction = NewObject<UInputAction>(this, TEXT("IA_ToolUse"));
		ToolUseAction->ValueType = EInputActionValueType::Boolean;
		ToolDropAction = NewObject<UInputAction>(this, TEXT("IA_ToolDrop"));
		ToolDropAction->ValueType = EInputActionValueType::Boolean;
	}
	Input->BindAction(ToolSlotActions[0], ETriggerEvent::Started, this, &ANoclipCharacter::OnToolSlot1);
	Input->BindAction(ToolSlotActions[1], ETriggerEvent::Started, this, &ANoclipCharacter::OnToolSlot2);
	Input->BindAction(ToolSlotActions[2], ETriggerEvent::Started, this, &ANoclipCharacter::OnToolSlot3);
	Input->BindAction(ToolUseAction, ETriggerEvent::Started, this, &ANoclipCharacter::OnToolUseStarted);
	Input->BindAction(ToolUseAction, ETriggerEvent::Completed, this, &ANoclipCharacter::OnToolUseCompleted);
	Input->BindAction(ToolDropAction, ETriggerEvent::Started, this, &ANoclipCharacter::OnToolDrop);
	AddToolMappingContext();
}

void ANoclipCharacter::OnToolSlot1(const FInputActionValue& Value) { ToolComponent->RequestSlot(1); }
void ANoclipCharacter::OnToolSlot2(const FInputActionValue& Value) { ToolComponent->RequestSlot(2); }
void ANoclipCharacter::OnToolSlot3(const FInputActionValue& Value) { ToolComponent->RequestSlot(3); }
void ANoclipCharacter::OnToolUseStarted(const FInputActionValue& Value) { ToolComponent->RequestUse(true); }
void ANoclipCharacter::OnToolUseCompleted(const FInputActionValue& Value) { ToolComponent->RequestUse(false); }
void ANoclipCharacter::OnToolDrop(const FInputActionValue& Value) { ToolComponent->RequestDrop(); }

void ANoclipCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ANoclipCharacter, Health);
	DOREPLIFETIME_CONDITION(ANoclipCharacter, ReplicatedAimPitch, COND_SkipOwner);
}

FRotator ANoclipCharacter::GetAimRotation() const
{
	if (IsLocallyControlled())
	{
		return GetViewRotation();
	}
	if (HasAuthority())
	{
		return GetAuthoritativeViewRotation();
	}
	return FRotator(ReplicatedAimPitch / 100.f, GetActorRotation().Yaw, 0.f);
}

void ANoclipCharacter::Heal(float Amount)
{
	if (!HasAuthority())
	{
		return;
	}
	Health = FMath::Clamp(Health + Amount, 0.f, MaxHealth);
	Client_RefillStamina();
}

void ANoclipCharacter::ApplyHurt(float Amount, FVector FromLocation, ENoclipDeathCause Cause)
{
	if (!HasAuthority() || Amount <= 0.f || bPendingDeath)
	{
		return;
	}
	// So se morre durante o turno (no saguao / elevador fica por um fio).
	const ANoclipGameState* GS = GetWorld() ? GetWorld()->GetGameState<ANoclipGameState>() : nullptr;
	const bool bLethal = GS && (GS->GetPhase() == ENoclipShiftPhase::Active || GS->GetPhase() == ENoclipShiftPhase::Collapse);
	Health = FMath::Clamp(Health - Amount, bLethal ? 0.f : 1.f, MaxHealth);
	if (Health <= 0.f)
	{
		// Processa no proximo Tick: quem bateu ainda mexe neste pawn depois do ApplyHurt.
		bPendingDeath = true;
		PendingDeathCause = Cause;
		LastHitFrom = FromLocation;
	}
	if (Amount >= 3.f)
	{
		Client_HurtFeedback(Amount, FromLocation);
	}
}

void ANoclipCharacter::Client_HurtFeedback_Implementation(float Amount, FVector_NetQuantize FromLocation)
{
	LastHurtTime = GetWorld()->GetTimeSeconds();
	HurtStrength = FMath::Clamp(Amount / 30.f, 0.35f, 1.f);
	HurtSide = 0.f;
	if (!FVector(FromLocation).IsNearlyZero())
	{
		const FVector Right = FRotationMatrix(FRotator(0.f, GetViewRotation().Yaw, 0.f)).GetUnitAxis(EAxis::Y);
		HurtSide = FMath::Sign(FVector::DotProduct((FVector(FromLocation) - GetActorLocation()).GetSafeNormal2D(), Right));
	}
	PlayRumble(0.4f + 0.6f * HurtStrength, 0.25f);
}

float ANoclipCharacter::GetHurtAge() const
{
	return GetWorld() ? static_cast<float>(GetWorld()->GetTimeSeconds() - LastHurtTime) : 100.f;
}

void ANoclipCharacter::Client_RefillStamina_Implementation()
{
	Stamina = 1.f;
	bExhausted = false;
	StaminaRegenDelay = 0.f;
}

void ANoclipCharacter::PlayFootstepSound(bool bSprinting, bool bLocal) const
{
	if (UNoclipAudioSubsystem* Audio = UNoclipAudioSubsystem::Get(this))
	{
		Audio->PlayFootstep(this, bSprinting, bLocal);
	}
}

void ANoclipCharacter::UpdateRemoteFootsteps(float DeltaSeconds)
{
	if (!MoverComponent || DeltaSeconds <= 0.f)
	{
		return;
	}
	const FVector Velocity = MoverComponent->GetVelocity();
	const float Speed = FVector(Velocity.X, Velocity.Y, 0.f).Size();
	const bool bOnGround = MoverComponent->IsOnGround();

	// Pouso de um colega.
	if (!bOnGround)
	{
		RemoteMinZSpeed = FMath::Min(RemoteMinZSpeed, Velocity.Z);
	}
	else if (!bRemoteWasOnGround)
	{
		const float FallSpeed = -RemoteMinZSpeed;
		if (FallSpeed > 250.f)
		{
			if (UNoclipAudioSubsystem* Audio = UNoclipAudioSubsystem::Get(this))
			{
				Audio->PlayLanding(this, FMath::Clamp(FallSpeed / 1200.f, 0.f, 1.f), false);
			}
		}
		RemoteMinZSpeed = 0.f;
	}
	bRemoteWasOnGround = bOnGround;

	if (!bOnGround || Speed < 40.f)
	{
		return;
	}
	const float PrevPhase = RemoteStepPhase;
	RemoteStepPhase += DeltaSeconds * HeadBobFrequency * (Speed / FMath::Max(WalkSpeed, 1.f)) * UE_TWO_PI;
	if (FMath::FloorToInt(PrevPhase / UE_PI) != FMath::FloorToInt(RemoteStepPhase / UE_PI))
	{
		PlayFootstepSound(Speed > WalkSpeed * 1.15f, false);
	}
	RemoteStepPhase = FMath::Fmod(RemoteStepPhase, UE_TWO_PI);
}

void ANoclipCharacter::UpdateServerHealth(float DeltaSeconds)
{
	// Mira para os outros verem (lanterna, celular).
	const float Pitch = FRotator::NormalizeAxis(GetAuthoritativeViewRotation().Pitch);
	ReplicatedAimPitch = static_cast<int16>(FMath::Clamp(FMath::RoundToInt(Pitch * 100.f), -9000, 9000));

	// Queda forte machuca.
	const bool bOnGround = MoverComponent->IsOnGround();
	if (!bOnGround)
	{
		ServerMinZSpeed = FMath::Min(ServerMinZSpeed, MoverComponent->GetVelocity().Z);
	}
	else if (!bServerWasOnGround)
	{
		const float FallSpeed = -ServerMinZSpeed;
		// Pouso faz barulho (os caes escutam).
		if (FallSpeed > 250.f)
		{
			UNoclipNoiseSubsystem::Report(this, GetActorLocation(), FMath::Clamp(FallSpeed * 1.3f, 400.f, 1800.f), this, true);
		}
		if (FallSpeed > FallDamageSpeed)
		{
			ApplyHurt((FallSpeed - FallDamageSpeed) / 100.f * FallDamagePer100, FVector::ZeroVector, ENoclipDeathCause::Fall);
		}
		ServerMinZSpeed = 0.f;
	}
	bServerWasOnGround = bOnGround;
}

void ANoclipCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	UEnhancedInputComponent* Input = Cast<UEnhancedInputComponent>(PlayerInputComponent);
	if (!Input)
	{
		return;
	}

	if (MoveAction)
	{
		Input->BindAction(MoveAction, ETriggerEvent::Triggered, this, &ANoclipCharacter::OnMoveTriggered);
		Input->BindAction(MoveAction, ETriggerEvent::Completed, this, &ANoclipCharacter::OnMoveCompleted);
	}
	if (LookAction)
	{
		Input->BindAction(LookAction, ETriggerEvent::Triggered, this, &ANoclipCharacter::OnLookTriggered);
	}
	if (LookStickAction)
	{
		Input->BindAction(LookStickAction, ETriggerEvent::Triggered, this, &ANoclipCharacter::OnLookStickTriggered);
		Input->BindAction(LookStickAction, ETriggerEvent::Completed, this, &ANoclipCharacter::OnLookStickCompleted);
	}
	if (JumpAction)
	{
		Input->BindAction(JumpAction, ETriggerEvent::Started, this, &ANoclipCharacter::OnJumpStarted);
		Input->BindAction(JumpAction, ETriggerEvent::Completed, this, &ANoclipCharacter::OnJumpCompleted);
	}
	if (SprintAction)
	{
		Input->BindAction(SprintAction, ETriggerEvent::Started, this, &ANoclipCharacter::OnSprintStarted);
		Input->BindAction(SprintAction, ETriggerEvent::Completed, this, &ANoclipCharacter::OnSprintCompleted);
	}
	if (GrabAction)
	{
		Input->BindAction(GrabAction, ETriggerEvent::Started, this, &ANoclipCharacter::OnGrabStarted);
	}
	if (ThrowAction)
	{
		Input->BindAction(ThrowAction, ETriggerEvent::Started, this, &ANoclipCharacter::OnThrowStarted);
		Input->BindAction(ThrowAction, ETriggerEvent::Completed, this, &ANoclipCharacter::OnThrowCompleted);
	}
	SetupToolInput(Input);
}

namespace
{
	/** Zona morta radial com reescala (o movimento comeca em 0 logo apos a zona morta). */
	FVector2D ApplyRadialDeadZone(const FVector2D& In, float DeadZone)
	{
		const float Size = In.Size();
		if (Size <= DeadZone)
		{
			return FVector2D::ZeroVector;
		}
		const float Scaled = FMath::Clamp((Size - DeadZone) / FMath::Max(1.f - DeadZone, KINDA_SMALL_NUMBER), 0.f, 1.f);
		return In / Size * Scaled;
	}
}

void ANoclipCharacter::OnMoveTriggered(const FInputActionValue& Value)
{
	// Teclado da sempre 0 ou 1, entao a zona morta so afeta o analogico.
	const FVector2D Axis = ApplyRadialDeadZone(Value.Get<FVector2D>(), StickDeadZone);
	// X = frente, Y = direita (convencao do IMC: W/S no Y do eixo 2D, A/D no X)
	CachedMoveInput = FVector(FMath::Clamp(Axis.Y, -1.f, 1.f), FMath::Clamp(Axis.X, -1.f, 1.f), 0.f);
}

void ANoclipCharacter::OnMoveCompleted(const FInputActionValue& Value)
{
	CachedMoveInput = FVector::ZeroVector;
}

void ANoclipCharacter::OnLookTriggered(const FInputActionValue& Value)
{
	const FNoclipUserSettings& Settings = FNoclipUserSettings::Get();
	const FVector2D Look = Value.Get<FVector2D>();
	const float Sens = LookSensitivity * Settings.MouseSensitivity;
	AddControllerYawInput(Look.X * Sens);
	AddControllerPitchInput((Settings.bInvertY ? Look.Y : -Look.Y) * Sens);
}

void ANoclipCharacter::OnLookStickTriggered(const FInputActionValue& Value)
{
	LookStickValue = Value.Get<FVector2D>();
}

void ANoclipCharacter::OnLookStickCompleted(const FInputActionValue& Value)
{
	LookStickValue = FVector2D::ZeroVector;
}

void ANoclipCharacter::ApplyStickLook(float DeltaSeconds)
{
	const FVector2D Stick = ApplyRadialDeadZone(LookStickValue, StickDeadZone);
	const float Size = Stick.Size();
	if (Size <= 0.f)
	{
		return;
	}
	const FNoclipUserSettings& Settings = FNoclipUserSettings::Get();
	const FVector2D Curved = Stick / Size * FMath::Pow(Size, LookStickExponent);
	const float Sens = Settings.StickSensitivity;
	const bool bInvert = bInvertGamepadY != Settings.bInvertY;
	AddControllerYawInput(Curved.X * GamepadYawRate * Sens * DeltaSeconds);
	AddControllerPitchInput((bInvert ? Curved.Y : -Curved.Y) * GamepadPitchRate * Sens * DeltaSeconds);
}

void ANoclipCharacter::UpdateStamina(float DeltaSeconds)
{
	const FVector Velocity = MoverComponent->GetVelocity();
	const bool bSprinting = bSprintHeld && !bExhausted && MoverComponent->IsOnGround() && FVector(Velocity.X, Velocity.Y, 0.f).Size() > WalkSpeed * 1.1f;
	if (bSprinting)
	{
		Stamina = FMath::Max(0.f, Stamina - DeltaSeconds / SprintStaminaSeconds);
		StaminaRegenDelay = 0.8f;
		if (Stamina <= 0.f)
		{
			bExhausted = true;
		}
	}
	else if (StaminaRegenDelay > 0.f)
	{
		StaminaRegenDelay -= DeltaSeconds;
	}
	else
	{
		Stamina = FMath::Min(1.f, Stamina + DeltaSeconds / StaminaRecoverSeconds);
	}
	// Sem folego: so volta a correr depois de recuperar 30% (evita "gaguejar" no limite).
	if (bExhausted && Stamina >= 0.3f)
	{
		bExhausted = false;
	}
}

void ANoclipCharacter::PlayRumble(float Intensity, float Duration) const
{
	if (!IsLocallyControlled() || RumbleScale <= 0.f || !FNoclipUserSettings::Get().bRumble)
	{
		return;
	}
	if (APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		const float Scaled = FMath::Clamp(Intensity * RumbleScale, 0.f, 1.f);
		PC->PlayDynamicForceFeedback(Scaled, Duration, true, true, true, true);
	}
}

void ANoclipCharacter::OnJumpStarted(const FInputActionValue& Value)
{
	bJumpHeld = true;
	JumpBufferRemaining = JumpBufferTime;
}

void ANoclipCharacter::OnJumpCompleted(const FInputActionValue& Value)
{
	bJumpHeld = false;
}

void ANoclipCharacter::OnSprintStarted(const FInputActionValue& Value) { bSprintHeld = true; }
void ANoclipCharacter::OnSprintCompleted(const FInputActionValue& Value) { bSprintHeld = false; }

void ANoclipCharacter::OnGrabStarted(const FInputActionValue& Value)
{
	// Apagao, perto do gerador e de maos vazias: o botao de agarrar opera o painel.
	if (TryGeneratorQTE())
	{
		return;
	}
	GrabComponent->ToggleGrab();
}

// ===================================================================== Noclip

void ANoclipCharacter::ServerNoclipTo(const FVector& Destination, uint8 GlitchKind)
{
	if (!HasAuthority() || !MoverComponent)
	{
		return;
	}
	const FVector From = GetActorLocation();
	TSharedPtr<FTeleportEffect> Teleport = MakeShared<FTeleportEffect>();
	Teleport->TargetLocation = Destination;
	Teleport->bUseActorRotation = true;
	MoverComponent->QueueInstantMovementEffect(Teleport);

	// O achado da mao vai junto (senao ficava para tras, do outro lado da parede).
	if (AActor* Held = GrabComponent ? GrabComponent->GetHeldActor() : nullptr)
	{
		Held->SetActorLocation(Destination + (Held->GetActorLocation() - From), false, nullptr, ETeleportType::TeleportPhysics);
	}
	if (ToolComponent)
	{
		ToolComponent->ServerHolster();
	}
	Client_NoclipFeedback(GlitchKind);
}

void ANoclipCharacter::Client_NoclipFeedback_Implementation(uint8 GlitchKind)
{
	LastNoclipTime = GetWorld()->GetTimeSeconds();
	LastNoclipKind = GlitchKind;
	bQTEActive = false;
	PlayRumble(0.8f, 0.35f);
	if (UNoclipAudioSubsystem* Audio = UNoclipAudioSubsystem::Get(this))
	{
		Audio->PlayNoclipSting();
	}
}

float ANoclipCharacter::GetNoclipAge() const
{
	return GetWorld() ? static_cast<float>(GetWorld()->GetTimeSeconds() - LastNoclipTime) : 100.f;
}

// ===================================================================== Gerador (QTE)

ANoclipPowerGenerator* ANoclipCharacter::GetGeneratorInReach() const
{
	const ANoclipGameState* GS = GetWorld() ? GetWorld()->GetGameState<ANoclipGameState>() : nullptr;
	ANoclipPowerGenerator* Gen = CachedGenerator.Get();
	if (!GS || !GS->IsPowerOut() || !Gen || bPendingDeath)
	{
		return nullptr;
	}
	return Gen->IsInReach(GetActorLocation()) ? Gen : nullptr;
}

float ANoclipCharacter::GetQTEResultAge() const
{
	return GetWorld() ? static_cast<float>(GetWorld()->GetTimeSeconds() - QTELastTime) : 100.f;
}

void ANoclipCharacter::NewQTEZone()
{
	// Cada acerto em sequencia aperta a faixa (e acelera o ponteiro, no Tick).
	QTEZoneHalf = FMath::Max(0.05f, 0.11f - QTEStreak * 0.008f);
	float Center = FMath::FRandRange(0.12f + QTEZoneHalf, 0.88f - QTEZoneHalf);
	if (FMath::Abs(Center - QTENeedle) < 0.2f)
	{
		Center = QTENeedle < 0.5f ? FMath::Min(0.88f - QTEZoneHalf, Center + 0.35f) : FMath::Max(0.12f + QTEZoneHalf, Center - 0.35f);
	}
	QTEZoneCenter = Center;
}

bool ANoclipCharacter::TryGeneratorQTE()
{
	ANoclipPowerGenerator* Gen = GetGeneratorInReach();
	if (!Gen || (GrabComponent && GrabComponent->IsHolding()))
	{
		return false;
	}
	const double Now = GetWorld()->GetTimeSeconds();
	if (!bQTEActive)
	{
		bQTEActive = true;
		QTENeedle = 0.f;
		QTEDir = 1.f;
		QTEStreak = 0;
		NewQTEZone();
		QTENextPress = Now + 0.2;
		return true;
	}
	if (Now < QTENextPress)
	{
		return true;
	}
	QTENextPress = Now + 0.22;
	const bool bHit = FMath::Abs(QTENeedle - QTEZoneCenter) <= QTEZoneHalf;
	Server_GeneratorAttempt(Gen, bHit);
	bQTELastHit = bHit;
	QTELastTime = Now;
	QTEStreak = bHit ? QTEStreak + 1 : 0;
	NewQTEZone();
	PlayRumble(bHit ? 0.25f : 0.7f, bHit ? 0.06f : 0.2f);
	return true;
}

void ANoclipCharacter::TickGeneratorQTE(float DeltaSeconds)
{
	const double Now = GetWorld()->GetTimeSeconds();
	if (Now >= NextGeneratorLookup)
	{
		NextGeneratorLookup = Now + 0.5;
		CachedGenerator.Reset();
		for (TActorIterator<ANoclipPowerGenerator> It(GetWorld()); It; ++It)
		{
			CachedGenerator = *It;
			break;
		}
	}
	if (!bQTEActive)
	{
		return;
	}
	if (!GetGeneratorInReach() || (GrabComponent && GrabComponent->IsHolding()))
	{
		bQTEActive = false;
		return;
	}
	const float Speed = 0.9f + FMath::Min(QTEStreak, 8) * 0.12f;
	QTENeedle += QTEDir * Speed * DeltaSeconds;
	if (QTENeedle >= 1.f) { QTENeedle = 1.f; QTEDir = -1.f; }
	if (QTENeedle <= 0.f) { QTENeedle = 0.f; QTEDir = 1.f; }
}

void ANoclipCharacter::Server_GeneratorAttempt_Implementation(ANoclipPowerGenerator* Generator, bool bHit)
{
	if (IsValid(Generator) && !bPendingDeath)
	{
		Generator->ServerAttempt(this, bHit);
	}
}

void ANoclipCharacter::OnThrowStarted(const FInputActionValue& Value)
{
	GrabComponent->BeginThrowCharge();
}

void ANoclipCharacter::OnThrowCompleted(const FInputActionValue& Value)
{
	const bool bWasCharging = GrabComponent->IsHolding() && GrabComponent->GetThrowCharge() > 0.f;
	const float Charge = GrabComponent->GetThrowCharge();
	GrabComponent->ReleaseThrow();
	if (bWasCharging)
	{
		PlayRumble(0.3f + 0.6f * Charge, 0.1f + 0.12f * Charge);
	}
}

void ANoclipCharacter::ProduceInput_Implementation(int32 SimTimeMs, FMoverInputCmdContext& InputCmdResult)
{
	FCharacterDefaultInputs& CharacterInputs = InputCmdResult.InputCollection.FindOrAddMutableDataByType<FCharacterDefaultInputs>();
	FNoclipMoverInputs& NoclipInputs = InputCmdResult.InputCollection.FindOrAddMutableDataByType<FNoclipMoverInputs>();

	APlayerController* PC = Cast<APlayerController>(GetController());
	if (!PC)
	{
		if (GetLocalRole() == ROLE_Authority)
		{
			// Sem dono: input neutro.
			CharacterInputs = FCharacterDefaultInputs();
			NoclipInputs = FNoclipMoverInputs();
		}
		return;
	}

	const FRotator ControlRotation = PC->GetControlRotation();
	CharacterInputs.ControlRotation = ControlRotation;

	// Movimento relativo ao yaw da camera.
	const FRotator YawOnly(0.f, ControlRotation.Yaw, 0.f);
	const FVector WorldMove = YawOnly.RotateVector(CachedMoveInput).GetClampedToMaxSize(1.f);
	CharacterInputs.SetMoveInput(EMoveInputType::DirectionalIntent, WorldMove);

	// Primeira pessoa: o corpo sempre olha para onde a camera aponta.
	CharacterInputs.OrientationIntent = YawOnly.Vector();

	// Jump buffer: o "just pressed" continua sendo enviado ate o pulo acontecer ou a janela fechar.
	const float DeltaSeconds = GetWorld() ? GetWorld()->GetDeltaSeconds() : 0.016f;
	const bool bJumpBuffered = JumpBufferRemaining > 0.f;
	CharacterInputs.bIsJumpJustPressed = bJumpBuffered;
	CharacterInputs.bIsJumpPressed = bJumpHeld;
	if (bJumpBuffered)
	{
		JumpBufferRemaining -= DeltaSeconds;
		if (MoverComponent->IsFalling() && MoverComponent->GetVelocity().Z > 50.f)
		{
			JumpBufferRemaining = 0.f; // o pulo ja saiu
		}
	}

	CharacterInputs.SuggestedMovementMode = NAME_None;
	CharacterInputs.bUsingMovementBase = false;

	NoclipInputs.bWantsSprint = bSprintHeld && !bExhausted;
	NoclipInputs.SetCarrySpeedScale(GrabComponent->GetCarrySpeedScale());
}

void ANoclipCharacter::SyncCharacterFromPlayerState()
{
	const ANoclipPlayerState* PS = GetPlayerState<ANoclipPlayerState>();
	const int32 Wanted = PS ? PS->GetCharacterIndex() : (AppliedCharacter >= 0 ? AppliedCharacter : 0);
	if (Wanted != AppliedCharacter)
	{
		ApplyCharacter(Wanted);
	}
	UNoclipCharacterAnimInstance* Anim = CharacterMesh ? Cast<UNoclipCharacterAnimInstance>(CharacterMesh->GetAnimInstance()) : nullptr;
	if (Anim && AppliedCharacter >= 0 && Anim->GetCharacterIndex() != AppliedCharacter)
	{
		Anim->SetCharacter(AppliedCharacter);
	}
}

void ANoclipCharacter::ApplyCharacter(int32 Index)
{
	Index = NoclipCharacters::ClampPlayable(Index);
	USkeletalMesh* Mesh = NoclipCharacters::LoadMesh(Index);
	if (!Mesh || !CharacterMesh)
	{
		return;
	}
	AppliedCharacter = Index;
	const FNoclipCharacterDef& Def = NoclipCharacters::Get(Index);
	CharacterMesh->SetRelativeRotation(FRotator(0.f, Def.MeshYaw, 0.f));
	if (CharacterMesh->AnimClass != UNoclipCharacterAnimInstance::StaticClass())
	{
		CharacterMesh->SetAnimInstanceClass(UNoclipCharacterAnimInstance::StaticClass());
	}
	CharacterMesh->SetSkeletalMesh(Mesh, true);
	if (BodyMesh)
	{
		BodyMesh->SetVisibility(false);
	}
}

void ANoclipCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	SyncCharacterFromPlayerState();

	if (HasAuthority())
	{
		UpdateServerHealth(DeltaSeconds);
		if (bPendingDeath)
		{
			if (ANoclipGameMode* GM = GetWorld()->GetAuthGameMode<ANoclipGameMode>())
			{
				GM->NotifyPlayerDied(this, PendingDeathCause, LastHitFrom);
				return; // destruido
			}
			bPendingDeath = false; // sem GameMode do expediente (nao deveria acontecer)
			Health = 1.f;
		}
	}

	if (!IsLocallyControlled() && GetNetMode() != NM_DedicatedServer)
	{
		UpdateRemoteFootsteps(DeltaSeconds);
	}

	if (IsLocallyControlled())
	{
		// Menu aberto (CommonUI em modo Menu): o jogo nao recebe input e os "Completed" podem nao chegar.
		const ANoclipPlayerController* NoclipPC = Cast<ANoclipPlayerController>(GetController());
		if (NoclipPC && NoclipPC->IsUIMenuOpen())
		{
			CachedMoveInput = FVector::ZeroVector;
			LookStickValue = FVector2D::ZeroVector;
			bSprintHeld = false;
			bJumpHeld = false;
		}
		ApplyStickLook(DeltaSeconds);
		UpdateStamina(DeltaSeconds);
		UpdateCameraFeel(DeltaSeconds);
		TickGeneratorQTE(DeltaSeconds);
	}
}

void ANoclipCharacter::UpdateCameraFeel(float DeltaSeconds)
{
	if (DeltaSeconds <= 0.f)
	{
		return;
	}

	const FVector Velocity = MoverComponent->GetVelocity();
	const FVector HorizontalVelocity(Velocity.X, Velocity.Y, 0.f);
	const float Speed = HorizontalVelocity.Size();
	const bool bOnGround = MoverComponent->IsOnGround();
	const bool bSprinting = bSprintHeld && Speed > WalkSpeed * 1.15f;

	// --- FOV de sprint ---
	const FNoclipUserSettings& UserSettings = FNoclipUserSettings::Get();
	const float BobScale = UserSettings.GetBobScale();
	const float TargetFOV = static_cast<float>(UserSettings.FieldOfView) + (bSprinting ? SprintFOVBonus : 0.f);
	CurrentFOV = FMath::FInterpTo(CurrentFOV, TargetFOV, DeltaSeconds, 8.f);
	FirstPersonCamera->SetFieldOfView(CurrentFOV);

	// --- Pouso: mola amortecida ---
	if (!bOnGround)
	{
		LastAirborneZSpeed = FMath::Min(LastAirborneZSpeed, Velocity.Z);
	}
	else if (!bWasOnGround)
	{
		const float FallSpeed = -LastAirborneZSpeed;
		if (FallSpeed > 150.f)
		{
			LandingVelocity -= (FallSpeed / 1000.f) * LandingDipPer1000 * 12.f * BobScale;
			const float LandIntensity = FMath::Clamp(FallSpeed / 1200.f, 0.f, 1.f);
			OnLandedFeel(LandIntensity);
			if (UNoclipAudioSubsystem* Audio = UNoclipAudioSubsystem::Get(this))
			{
				Audio->PlayLanding(this, LandIntensity, true);
			}
			PlayRumble(0.15f + 0.65f * LandIntensity, 0.08f + 0.12f * LandIntensity);
		}
		LastAirborneZSpeed = 0.f;
	}
	bWasOnGround = bOnGround;

	const float SpringAccel = -LandingSpringStiffness * LandingOffset - LandingSpringDamping * LandingVelocity;
	LandingVelocity += SpringAccel * DeltaSeconds;
	LandingOffset += LandingVelocity * DeltaSeconds;
	LandingOffset = FMath::Clamp(LandingOffset, -25.f, 5.f);

	// --- Head bob ---
	float BobZ = 0.f;
	float BobY = 0.f;
	if (bOnGround && Speed > 20.f)
	{
		const float SpeedRatio = Speed / FMath::Max(WalkSpeed, 1.f);
		const float PrevPhase = BobPhase;
		BobPhase += DeltaSeconds * HeadBobFrequency * SpeedRatio * UE_TWO_PI;
		const float Amp = HeadBobAmplitude * BobScale * FMath::Clamp(SpeedRatio, 0.f, 1.8f);
		BobZ = FMath::Sin(BobPhase * 2.f) * Amp;
		BobY = FMath::Cos(BobPhase) * Amp * 0.5f;

		// Um passo a cada meio ciclo (cada pe).
		if (FMath::FloorToInt(PrevPhase / UE_PI) != FMath::FloorToInt(BobPhase / UE_PI))
		{
			OnFootstep(bSprinting);
			PlayFootstepSound(bSprinting, true);
		}
		BobPhase = FMath::Fmod(BobPhase, UE_TWO_PI);
	}
	else
	{
		BobPhase = FMath::FInterpTo(BobPhase, FMath::RoundToFloat(BobPhase / UE_PI) * UE_PI, DeltaSeconds, 6.f);
	}

	// --- Roll ao andar de lado ---
	const FVector Right = FRotationMatrix(FRotator(0.f, GetViewRotation().Yaw, 0.f)).GetUnitAxis(EAxis::Y);
	const float Strafe = FVector::DotProduct(HorizontalVelocity, Right) / FMath::Max(WalkSpeed, 1.f);
	CurrentRoll = FMath::FInterpTo(CurrentRoll, FMath::Clamp(Strafe, -1.f, 1.f) * StrafeRollDegrees * BobScale, DeltaSeconds, 10.f);

	FirstPersonCamera->SetRelativeLocation(FVector(0.f, BobY, EyeHeight + BobZ + LandingOffset));

	// Debug de terceira pessoa: so a camera e a visibilidade mudam (o jogo continua igual).
	const int32 ThirdPerson = CVarNoclipThirdPerson.GetValueOnGameThread();
	const bool bThirdPerson = ThirdPerson > 0;
	if (CharacterMesh && CharacterMesh->bOwnerNoSee == bThirdPerson)
	{
		CharacterMesh->SetOwnerNoSee(!bThirdPerson);
		if (ToolComponent)
		{
			ToolComponent->SetBodyVisibleToOwner(bThirdPerson);
		}
	}
	if (bThirdPerson)
	{
		// 1 = de tras, 2 = de frente, 3 = de lado.
		const FVector Offset = ThirdPerson == 2 ? FVector(190.f, -40.f, 30.f) : (ThirdPerson == 3 ? FVector(20.f, 190.f, 20.f) : FVector(-200.f, 55.f, 45.f));
		FirstPersonCamera->SetRelativeLocation(Offset);
	}
	// "Nao" com a cabeca (acao negada): balanca a camera de um lado para o outro, sem mexer na mira.
	float ShakeYaw = 0.f;
	if (ToolComponent)
	{
		const float Age = ToolComponent->GetHeadShakeAge();
		constexpr float ShakeSeconds = 0.6f;
		if (Age >= 0.f && Age < ShakeSeconds)
		{
			const float Fade = 1.f - Age / ShakeSeconds;
			ShakeYaw = FMath::Sin(Age * UE_TWO_PI * 3.3f) * 7.f * Fade * FMath::Max(BobScale, 0.35f);
		}
	}

	if (const AController* C = GetController())
	{
		const FRotator Control = C->GetControlRotation();
		// Tranco do golpe: a camera pula para cima e tomba para o lado contrario, voltando com mola.
		float HurtPitch = 0.f, HurtRoll = 0.f;
		const float HurtAge = GetHurtAge();
		constexpr float HurtKickSeconds = 0.45f;
		if (HurtAge >= 0.f && HurtAge < HurtKickSeconds)
		{
			const float K = FMath::Sin((1.f - HurtAge / HurtKickSeconds) * UE_HALF_PI) * HurtStrength;
			HurtPitch = 5.f * K * FMath::Cos(HurtAge * 22.f);
			HurtRoll = (HurtSide != 0.f ? -HurtSide : 1.f) * 7.f * K;
		}
		FRotator CamRot(Control.Pitch + HurtPitch, Control.Yaw + ShakeYaw, CurrentRoll + HurtRoll);
		if (bThirdPerson && ThirdPerson != 1)
		{
			CamRot = (GetActorLocation() + FVector(0.f, 0.f, 20.f) - FirstPersonCamera->GetComponentLocation()).Rotation();
		}
		FirstPersonCamera->SetWorldRotation(CamRot);
	}
}

FRotator ANoclipCharacter::GetViewRotation() const
{
	if (HasAuthority() && !IsLocallyControlled())
	{
		return GetAuthoritativeViewRotation();
	}
	return Super::GetViewRotation();
}

FRotator ANoclipCharacter::GetAuthoritativeViewRotation() const
{
	if (const FCharacterDefaultInputs* Inputs = MoverComponent->GetLastInputCmd().InputCollection.FindDataByType<FCharacterDefaultInputs>())
	{
		return Inputs->ControlRotation;
	}
	return GetActorRotation();
}

FVector ANoclipCharacter::GetEyeLocation() const
{
	return GetActorLocation() + FVector(0.f, 0.f, EyeHeight);
}
