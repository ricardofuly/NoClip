// Copyright Fuly Games. All Rights Reserved.

#include "Game/NoclipGameState.h"
#include "Net/UnrealNetwork.h"
#include "World/NoclipLevelGenerator.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(NoclipGameState)

void ANoclipGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ANoclipGameState, LevelSeed);
	DOREPLIFETIME(ANoclipGameState, bHasLevelSeed);
	DOREPLIFETIME(ANoclipGameState, Phase);
	DOREPLIFETIME(ANoclipGameState, PhaseEndServerTime);
	DOREPLIFETIME(ANoclipGameState, PhaseDurationSeconds);
	DOREPLIFETIME(ANoclipGameState, ShiftIndex);
	DOREPLIFETIME(ANoclipGameState, Quota);
	DOREPLIFETIME(ANoclipGameState, Money);
	DOREPLIFETIME(ANoclipGameState, ValueInExtraction);
	DOREPLIFETIME(ANoclipGameState, ExtractionProgress);
	DOREPLIFETIME(ANoclipGameState, PlayersInExtraction);
	DOREPLIFETIME(ANoclipGameState, LastResult);
	DOREPLIFETIME(ANoclipGameState, Leader);
	DOREPLIFETIME(ANoclipGameState, RoomName);
	DOREPLIFETIME(ANoclipGameState, InviteCode);
	DOREPLIFETIME(ANoclipGameState, bJoinInProgress);
	DOREPLIFETIME(ANoclipGameState, ShopOwned);
	DOREPLIFETIME(ANoclipGameState, MaxPlayers);
	DOREPLIFETIME(ANoclipGameState, RoomPrivacy);
	DOREPLIFETIME(ANoclipGameState, bArrivalPending);
	DOREPLIFETIME(ANoclipGameState, ExpectedArrivals);
	DOREPLIFETIME(ANoclipGameState, bPowerOut);
	DOREPLIFETIME(ANoclipGameState, PowerChangeServerTime);
	DOREPLIFETIME(ANoclipGameState, LayoutShiftMask);
}

void ANoclipGameState::SetPowerOut(bool bOut)
{
	if (HasAuthority() && bPowerOut != bOut)
	{
		bPowerOut = bOut;
		PowerChangeServerTime = GetServerWorldTimeSeconds();
		ForceNetUpdate();
	}
}

void ANoclipGameState::SetLayoutShiftMask(uint32 Mask)
{
	if (HasAuthority() && LayoutShiftMask != Mask)
	{
		LayoutShiftMask = Mask;
		ForceNetUpdate();
		OnRep_LayoutShift();
	}
}

void ANoclipGameState::OnRep_LayoutShift()
{
	// Cada maquina move os tampoes do seu proprio nivel gerado (e toca o barulho onde mudou).
	for (TActorIterator<ANoclipLevelGenerator> It(GetWorld()); It; ++It)
	{
		if (It->GetAppliedShiftMask() != LayoutShiftMask)
		{
			It->ApplyShiftMask(LayoutShiftMask, It->GetGeneratedSeed() != INDEX_NONE && (Phase == ENoclipShiftPhase::Active || Phase == ENoclipShiftPhase::Collapse));
		}
	}
}

void ANoclipGameState::SetRoomExtras(int32 InMaxPlayers, uint8 InPrivacy)
{
	if (HasAuthority())
	{
		MaxPlayers = FMath::Clamp(InMaxPlayers, 1, 6);
		RoomPrivacy = InPrivacy;
	}
}

void ANoclipGameState::SetArrival(bool bPending, int32 InExpected)
{
	if (HasAuthority())
	{
		bArrivalPending = bPending;
		ExpectedArrivals = InExpected;
	}
}

void ANoclipGameState::RefreshLeader()
{
	if (!HasAuthority() || !GetWorld())
	{
		return;
	}
	APlayerState* NewLeader = nullptr;
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		const APlayerController* PC = It->Get();
		if (PC && PC->IsLocalController() && PC->PlayerState)
		{
			NewLeader = PC->PlayerState;
			break;
		}
	}
	if (!NewLeader)
	{
		NewLeader = (Leader && PlayerArray.Contains(Leader)) ? Leader.Get() : (PlayerArray.Num() > 0 ? PlayerArray[0].Get() : nullptr);
	}
	SetLeader(NewLeader);
}

void ANoclipGameState::SetRoomInfo(const FString& InRoomName, const FString& InInviteCode, bool bInJoinInProgress)
{
	if (HasAuthority())
	{
		RoomName = InRoomName;
		InviteCode = InInviteCode;
		bJoinInProgress = bInJoinInProgress;
	}
}

void ANoclipGameState::AddOwned(int32 ItemIndex)
{
	if (!HasAuthority() || ItemIndex < 0)
	{
		return;
	}
	if (ShopOwned.Num() <= ItemIndex)
	{
		ShopOwned.SetNumZeroed(ItemIndex + 1);
	}
	++ShopOwned[ItemIndex];
	OnEconomyChanged.Broadcast();
}

void ANoclipGameState::ResetOwned()
{
	if (HasAuthority())
	{
		ShopOwned.Reset();
		OnEconomyChanged.Broadcast();
	}
}

void ANoclipGameState::SetLeader(APlayerState* InLeader)
{
	if (HasAuthority() && Leader != InLeader)
	{
		Leader = InLeader;
	}
}

// ---------------------------------------------------------------- Seed

void ANoclipGameState::SetLevelSeed(int32 NewSeed)
{
	if (!HasAuthority())
	{
		return;
	}
	LevelSeed = NewSeed;
	bHasLevelSeed = true;
	OnRep_LevelSeed();
}

void ANoclipGameState::OnRep_LevelSeed()
{
	bHasLevelSeed = true;
	OnLevelSeedReady.Broadcast(LevelSeed);
}

// ---------------------------------------------------------------- Turno

float ANoclipGameState::GetPhaseTimeRemaining() const
{
	if (PhaseEndServerTime <= 0.0)
	{
		return 0.f;
	}
	return FMath::Max(0.f, static_cast<float>(PhaseEndServerTime - GetServerWorldTimeSeconds()));
}

float ANoclipGameState::GetPhaseProgress() const
{
	if (PhaseDurationSeconds <= 0.f || PhaseEndServerTime <= 0.0)
	{
		return 0.f;
	}
	return FMath::Clamp(1.f - GetPhaseTimeRemaining() / PhaseDurationSeconds, 0.f, 1.f);
}

void ANoclipGameState::SetPhase(ENoclipShiftPhase NewPhase, float DurationSeconds)
{
	if (!HasAuthority())
	{
		return;
	}
	const ENoclipShiftPhase Old = Phase;
	Phase = NewPhase;
	PhaseEndServerTime = DurationSeconds > 0.f ? GetServerWorldTimeSeconds() + DurationSeconds : 0.0;
	PhaseDurationSeconds = FMath::Max(0.f, DurationSeconds);
	if (Old != NewPhase)
	{
		OnRep_Phase(Old);
	}
}

void ANoclipGameState::OnRep_Phase(ENoclipShiftPhase OldPhase)
{
	OnPhaseChanged.Broadcast(OldPhase, Phase);
}

void ANoclipGameState::SetShiftInfo(int32 InShiftIndex, int32 InQuota)
{
	ShiftIndex = InShiftIndex;
	Quota = InQuota;
	OnRep_Economy();
}

void ANoclipGameState::SetMoney(int32 InMoney)
{
	Money = InMoney;
	OnRep_Economy();
}

void ANoclipGameState::OnRep_Economy()
{
	OnEconomyChanged.Broadcast();
}

void ANoclipGameState::SetExtractionStatus(int32 InValue, float InProgress, int32 InPlayers)
{
	ValueInExtraction = InValue;
	ExtractionProgress = InProgress;
	PlayersInExtraction = InPlayers;
}

void ANoclipGameState::PublishResult(const FNoclipShiftResult& Result)
{
	LastResult = Result;
	OnRep_LastResult();
}

void ANoclipGameState::OnRep_LastResult()
{
	OnShiftEnded.Broadcast(LastResult);
}
