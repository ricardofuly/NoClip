// Copyright Fuly Games. All Rights Reserved.

#include "Game/NoclipPlayerController.h"
#include "Character/NoclipCharacterCatalog.h"
#include "Game/NoclipGameState.h"
#include "Game/NoclipPlayerState.h"
#include "Character/NoclipCharacter.h"
#include "Camera/CameraTypes.h"
#include "CollisionQueryParams.h"
#include "NOCLIP.h"
#include "Game/NoclipGameMode.h"
#include "Game/NoclipWaitingRoomGameMode.h"
#include "World/NoclipLevelGenerator.h"
#include "EngineUtils.h"
#include "HAL/IConsoleManager.h"
#include "Session/NoclipSessionSubsystem.h"
#include "UI/NoclipPrimaryLayout.h"
#include "Game/NoclipUserSettings.h"
#include "Blueprint/UserWidget.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "GameFramework/PlayerState.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "Kismet/KismetSystemLibrary.h"
#include "OnlineSubsystemUtils.h"
#include "Interfaces/OnlineExternalUIInterface.h"
#include "UObject/ConstructorHelpers.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(NoclipPlayerController)

ANoclipPlayerController::ANoclipPlayerController()
{
	static ConstructorHelpers::FObjectFinder<UInputMappingContext> IMCFinder(TEXT("/Game/NOCLIP/Input/IMC_Default.IMC_Default"));
	static ConstructorHelpers::FObjectFinder<UInputAction> PauseFinder(TEXT("/Game/NOCLIP/Input/IA_Pause.IA_Pause"));
	static ConstructorHelpers::FObjectFinder<UInputAction> ReadyFinder(TEXT("/Game/NOCLIP/Input/IA_Ready.IA_Ready"));
	if (IMCFinder.Succeeded())
	{
		DefaultMappingContext = IMCFinder.Object;
	}
	if (PauseFinder.Succeeded())
	{
		PauseAction = PauseFinder.Object;
	}
	if (ReadyFinder.Succeeded())
	{
		ReadyAction = ReadyFinder.Object;
	}
}

void ANoclipPlayerController::BeginPlay()
{
	Super::BeginPlay();

	if (IsLocalController())
	{
		// O pawn tambem adiciona este contexto ao ser possuido; aqui garante pausa/iniciar mesmo sem pawn.
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer()))
		{
			if (DefaultMappingContext && !Subsystem->HasMappingContext(DefaultMappingContext))
			{
				Subsystem->AddMappingContext(DefaultMappingContext, 0);
			}
		}
		CreateUI();
		// O cracha escolhido vive nesta maquina; cada mapa novo (viagem dura) avisa o servidor de novo.
		Server_SetCharacter(NoclipCharacters::GetLocalChoice());
	}
}

void ANoclipPlayerController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UILayout)
	{
		UILayout->RemoveFromParent();
		UILayout = nullptr;
	}
	Super::EndPlay(EndPlayReason);
}

void ANoclipPlayerController::CreateUI()
{
	if (UILayout || !GetLocalPlayer())
	{
		return;
	}
	UILayout = CreateWidget<UNoclipPrimaryLayout>(this, UNoclipPrimaryLayout::StaticClass());
	if (UILayout)
	{
		UILayout->AddToPlayerScreen(10);
		if (IsFrontend())
		{
			UILayout->ShowMainMenu();
		}
		else if (IsWaitingRoom())
		{
			UILayout->ShowWaitingRoom();
		}
		else
		{
			UILayout->ShowHUD();
		}
	}
}

void ANoclipPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();

	UEnhancedInputComponent* Input = Cast<UEnhancedInputComponent>(InputComponent);
	if (!Input)
	{
		return;
	}
	if (PauseAction)
	{
		Input->BindAction(PauseAction, ETriggerEvent::Started, this, &ANoclipPlayerController::OnPauseStarted);
	}
	if (ReadyAction)
	{
		Input->BindAction(ReadyAction, ETriggerEvent::Started, this, &ANoclipPlayerController::OnReadyStarted);
		Input->BindAction(ReadyAction, ETriggerEvent::Completed, this, &ANoclipPlayerController::OnReadyCompleted);
	}
}

void ANoclipPlayerController::OnPauseStarted(const FInputActionValue& Value)
{
	NoclipPause();
}

void ANoclipPlayerController::NoclipPause()
{
	if (UILayout)
	{
		UILayout->TogglePauseMenu();
	}
}

bool ANoclipPlayerController::IsUIMenuOpen() const
{
	return UILayout && UILayout->IsMenuOpen();
}

bool ANoclipPlayerController::CanStartShift() const
{
	const UWorld* World = GetWorld();
	const ANoclipGameState* GS = World ? World->GetGameState<ANoclipGameState>() : nullptr;
	return GS && PlayerState && GS->GetLeader() == PlayerState && GS->GetPhase() == ENoclipShiftPhase::Lobby;
}

void ANoclipPlayerController::OnReadyStarted(const FInputActionValue& Value)
{
	if (CanStartShift() && !IsUIMenuOpen())
	{
		ReadyHoldStartTime = GetWorld()->GetRealTimeSeconds();
		bReadyFired = false;
	}
}

void ANoclipPlayerController::OnReadyCompleted(const FInputActionValue& Value)
{
	ReadyHoldStartTime = -1.0;
}

float ANoclipPlayerController::GetReadyHoldProgress() const
{
	if (ReadyHoldStartTime < 0.0 || !GetWorld())
	{
		return 0.f;
	}
	if (ReadyHoldSeconds <= 0.f)
	{
		return 1.f;
	}
	return FMath::Clamp(static_cast<float>((GetWorld()->GetRealTimeSeconds() - ReadyHoldStartTime) / ReadyHoldSeconds), 0.f, 1.f);
}

bool ANoclipPlayerController::IsSpectating() const
{
	const ANoclipPlayerState* PS = GetPlayerState<ANoclipPlayerState>();
	return PS && PS->IsDead() && GetPawn() == nullptr;
}

ANoclipCharacter* ANoclipPlayerController::GetSpectateTarget() const
{
	return SpectateTarget.Get();
}

void ANoclipPlayerController::CycleSpectate(int32 Direction)
{
	TArray<ANoclipCharacter*> Alive;
	for (TActorIterator<ANoclipCharacter> It(GetWorld()); It; ++It)
	{
		if (IsValid(*It) && !It->IsActorBeingDestroyed() && It->GetPlayerState())
		{
			Alive.Add(*It);
		}
	}
	if (Alive.Num() == 0)
	{
		SpectateTarget = nullptr;
		return;
	}
	Alive.Sort([](const ANoclipCharacter& A, const ANoclipCharacter& B) { return A.GetPlayerState()->GetPlayerId() < B.GetPlayerState()->GetPlayerId(); });
	const int32 Current = Alive.IndexOfByKey(SpectateTarget.Get());
	const int32 Next = Current == INDEX_NONE ? 0 : (Current + Direction + Alive.Num()) % Alive.Num();
	SpectateTarget = Alive[Next];
	SpectateYaw = Alive[Next]->GetActorRotation().Yaw;
}

void ANoclipPlayerController::TickSpectate(float DeltaTime)
{
	if (!IsSpectating())
	{
		if (const APawn* P = GetPawn())
		{
			LastAliveLocation = P->GetActorLocation();
		}
		SpectateTarget = nullptr;
		bSpectatePivotValid = false;
		bWasSpectating = false;
		return;
	}

	if (!bWasSpectating)
	{
		bWasSpectating = true;
		SpectateYaw = GetControlRotation().Yaw;
		SpectatePitch = -18.f;
	}
	if (GetViewTarget() != this)
	{
		SetViewTarget(this);
	}

	if (!IsUIMenuOpen())
	{
		float MouseX = 0.f, MouseY = 0.f;
		GetInputMouseDelta(MouseX, MouseY);
		SpectateYaw += MouseX * 0.35f + GetInputAnalogKeyState(EKeys::Gamepad_RightX) * 150.f * DeltaTime;
		SpectatePitch = FMath::Clamp(SpectatePitch + MouseY * 0.35f + GetInputAnalogKeyState(EKeys::Gamepad_RightY) * 110.f * DeltaTime, -70.f, 35.f);

		if (WasInputKeyJustPressed(EKeys::LeftMouseButton) || WasInputKeyJustPressed(EKeys::D) || WasInputKeyJustPressed(EKeys::Gamepad_DPad_Right) || WasInputKeyJustPressed(EKeys::Gamepad_RightTrigger))
		{
			CycleSpectate(1);
		}
		else if (WasInputKeyJustPressed(EKeys::RightMouseButton) || WasInputKeyJustPressed(EKeys::A) || WasInputKeyJustPressed(EKeys::Gamepad_DPad_Left) || WasInputKeyJustPressed(EKeys::Gamepad_LeftTrigger))
		{
			CycleSpectate(-1);
		}
	}

	// Colega morreu / saiu: passa para o proximo vivo.
	if (!SpectateTarget.IsValid() || SpectateTarget->IsActorBeingDestroyed())
	{
		CycleSpectate(1);
	}

	const FVector Wanted = SpectateTarget.IsValid() ? SpectateTarget->GetActorLocation() + FVector(0.f, 0.f, 45.f) : LastAliveLocation + FVector(0.f, 0.f, 60.f);
	SpectatePivot = bSpectatePivotValid ? FMath::VInterpTo(SpectatePivot, Wanted, DeltaTime, 9.f) : Wanted;
	bSpectatePivotValid = true;
}

void ANoclipPlayerController::CalcCamera(float DeltaTime, FMinimalViewInfo& OutResult)
{
	if (!IsSpectating() || !bSpectatePivotValid)
	{
		Super::CalcCamera(DeltaTime, OutResult);
		return;
	}
	// Camera em terceira pessoa atras do colega, sem atravessar parede.
	const FRotator Rot(SpectatePitch, SpectateYaw, 0.f);
	const FVector Desired = SpectatePivot - Rot.Vector() * 300.f;
	FVector Location = Desired;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(NoclipSpectateCamera), false);
	if (SpectateTarget.IsValid())
	{
		Params.AddIgnoredActor(SpectateTarget.Get());
	}
	FHitResult Hit;
	if (GetWorld()->SweepSingleByChannel(Hit, SpectatePivot, Desired, FQuat::Identity, ECC_Camera, FCollisionShape::MakeSphere(14.f), Params))
	{
		Location = Hit.Location;
	}
	OutResult.Location = Location;
	OutResult.Rotation = Rot;
	OutResult.FOV = 90.f;
}

void ANoclipPlayerController::PlayerTick(float DeltaTime)
{
	Super::PlayerTick(DeltaTime);

	if (IsLocalController())
	{
		TickSpectate(DeltaTime);
	}

	// Opcoes de video que vivem no mundo (pos-processo, tremido): reaplica quando o nivel e regerado.
	if (IsLocalController())
	{
		const ANoclipGameState* VisualGS = GetWorld()->GetGameState<ANoclipGameState>();
		const int32 Seed = VisualGS && VisualGS->HasLevelSeed() ? VisualGS->GetLevelSeed() : 0;
		if (Seed != LastVisualSeed)
		{
			LastVisualSeed = Seed;
			VisualApplyDelay = 0.3f;
		}
		if (VisualApplyDelay >= 0.f)
		{
			VisualApplyDelay -= DeltaTime;
			if (VisualApplyDelay < 0.f)
			{
				FNoclipUserSettings::Get().ApplyToWorld(GetWorld());
			}
		}

		// Colapso: o nivel apaga de fora para dentro nesta maquina (o estado vem do GameState).
		if (VisualGS)
		{
			if (!CachedGenerator.IsValid())
			{
				for (TActorIterator<ANoclipLevelGenerator> It(GetWorld()); It; ++It)
				{
					CachedGenerator = *It;
					break;
				}
			}
			if (ANoclipLevelGenerator* Gen = CachedGenerator.Get())
			{
				static IConsoleVariable* GlitchDebug = IConsoleManager::Get().FindConsoleVariable(TEXT("noclip.DebugGlitches"));
				if (GlitchDebug && GlitchDebug->GetInt() > 0)
				{
					Gen->DebugDrawGlitches();
				}
				const ENoclipShiftPhase Phase = VisualGS->GetPhase();
				const bool bCollapse = Phase == ENoclipShiftPhase::Collapse;
				// No relatorio continua escuro; volta a acender no saguao ou no proximo turno.
				if (bCollapse || Phase != ENoclipShiftPhase::Ended)
				{
					Gen->UpdateCollapseLights(bCollapse, bCollapse ? VisualGS->GetPhaseProgress() : 0.f, static_cast<float>(VisualGS->GetServerWorldTimeSeconds()),
						VisualGS->IsPowerOut(), VisualGS->GetPowerChangeTime());
				}
			}
		}
	}

	if (ReadyHoldStartTime < 0.0)
	{
		return;
	}
	if (!CanStartShift() || IsUIMenuOpen())
	{
		ReadyHoldStartTime = -1.0;
		return;
	}
	if (!bReadyFired && GetReadyHoldProgress() >= 1.f)
	{
		bReadyFired = true;
		ReadyHoldStartTime = -1.0;
		if (FNoclipUserSettings::Get().bRumble)
		{
			PlayDynamicForceFeedback(0.6f, 0.18f, true, true, true, true);
		}
		NoclipStart();
	}
}

void ANoclipPlayerController::RequestSetReady(bool bReady)
{
	UE_LOG(LogNoclip, Log, TEXT("%s pede pronto=%d"), *GetNameSafe(PlayerState), bReady ? 1 : 0);
	Server_SetReady(bReady);
}
void ANoclipPlayerController::RequestBuyItem(int32 ItemIndex) { Server_BuyItem(ItemIndex); }

void ANoclipPlayerController::RequestSetCharacter(int32 CharacterIndex)
{
	NoclipCharacters::SetLocalChoice(CharacterIndex);
	Server_SetCharacter(NoclipCharacters::GetLocalChoice());
}

void ANoclipPlayerController::Server_SetCharacter_Implementation(int32 CharacterIndex)
{
	if (ANoclipPlayerState* PS = GetPlayerState<ANoclipPlayerState>())
	{
		PS->SetCharacterIndex(CharacterIndex);
	}
}
void ANoclipPlayerController::RequestSkipToLobby() { Server_SkipToLobby(); }

void ANoclipPlayerController::Server_SetReady_Implementation(bool bReady)
{
	if (ANoclipWaitingRoomGameMode* WaitingGM = GetWorld()->GetAuthGameMode<ANoclipWaitingRoomGameMode>())
	{
		UE_LOG(LogNoclip, Log, TEXT("Servidor: %s pronto=%d"), *GetNameSafe(PlayerState), bReady ? 1 : 0);
		WaitingGM->SetPlayerReady(this, bReady);
		return;
	}
	if (ANoclipGameMode* GM = GetWorld()->GetAuthGameMode<ANoclipGameMode>())
	{
		GM->SetPlayerReady(this, bReady);
	}
}

void ANoclipPlayerController::Server_BuyItem_Implementation(int32 ItemIndex)
{
	if (ANoclipGameMode* GM = GetWorld()->GetAuthGameMode<ANoclipGameMode>())
	{
		GM->RequestBuyItem(this, ItemIndex);
	}
}

void ANoclipPlayerController::Server_SkipToLobby_Implementation()
{
	if (ANoclipGameMode* GM = GetWorld()->GetAuthGameMode<ANoclipGameMode>())
	{
		GM->RequestSkipToLobby(this);
	}
}

bool ANoclipPlayerController::ShowInviteOverlay()
{
	const IOnlineSubsystem* OSS = Online::GetSubsystem(GetWorld());
	const IOnlineExternalUIPtr ExternalUI = OSS ? OSS->GetExternalUIInterface() : nullptr;
	return ExternalUI.IsValid() && ExternalUI->ShowInviteUI(0, NAME_GameSession);
}

void ANoclipPlayerController::LeaveToMainMenu()
{
	if (UNoclipSessionSubsystem* Sessions = GetGameInstance()->GetSubsystem<UNoclipSessionSubsystem>())
	{
		Sessions->ReturnToMainMenu(this);
	}
}

void ANoclipPlayerController::QuitToDesktop()
{
	UKismetSystemLibrary::QuitGame(this, this, EQuitPreference::Quit, false);
}

void ANoclipPlayerController::NoclipHost(const FString& MapPath)
{
	if (UNoclipSessionSubsystem* Sessions = GetGameInstance()->GetSubsystem<UNoclipSessionSubsystem>())
	{
		Sessions->HostSession(6, MapPath);
	}
}

void ANoclipPlayerController::NoclipFind()
{
	if (UNoclipSessionSubsystem* Sessions = GetGameInstance()->GetSubsystem<UNoclipSessionSubsystem>())
	{
		Sessions->FindSessions();
	}
}

void ANoclipPlayerController::NoclipJoin(int32 Index)
{
	if (UNoclipSessionSubsystem* Sessions = GetGameInstance()->GetSubsystem<UNoclipSessionSubsystem>())
	{
		Sessions->JoinSessionByIndex(Index);
	}
}

void ANoclipPlayerController::NoclipSeed(int32 Seed)
{
	Server_SetSeed(Seed);
}

void ANoclipPlayerController::Server_SetSeed_Implementation(int32 Seed)
{
	if (ANoclipGameState* GS = GetWorld()->GetGameState<ANoclipGameState>())
	{
		GS->SetLevelSeed(Seed);
	}
}

void ANoclipPlayerController::NoclipStart()
{
	Server_RequestStartShift();
}

void ANoclipPlayerController::Server_RequestStartShift_Implementation()
{
	if (ANoclipWaitingRoomGameMode* WaitingGM = GetWorld()->GetAuthGameMode<ANoclipWaitingRoomGameMode>())
	{
		WaitingGM->RequestDepart(this);
		return;
	}
	if (ANoclipGameMode* GM = GetWorld()->GetAuthGameMode<ANoclipGameMode>())
	{
		GM->RequestStartShift(this);
	}
}
