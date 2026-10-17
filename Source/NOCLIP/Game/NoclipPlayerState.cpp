// Copyright Fuly Games. All Rights Reserved.

#include "Game/NoclipPlayerState.h"
#include "Character/NoclipCharacterCatalog.h"
#include "Net/UnrealNetwork.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(NoclipPlayerState)

void ANoclipPlayerState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ANoclipPlayerState, bReady);
	DOREPLIFETIME(ANoclipPlayerState, bLeftBehind);
	DOREPLIFETIME(ANoclipPlayerState, bDead);
	DOREPLIFETIME(ANoclipPlayerState, DeathCause);
	DOREPLIFETIME(ANoclipPlayerState, CharacterIndex);
	DOREPLIFETIME(ANoclipPlayerState, ToolMask);
	DOREPLIFETIME(ANoclipPlayerState, MedkitCount);
	DOREPLIFETIME(ANoclipPlayerState, DamageCaused);
	DOREPLIFETIME(ANoclipPlayerState, ValueCarried);
	DOREPLIFETIME(ANoclipPlayerState, TimeAlone);
	DOREPLIFETIME(ANoclipPlayerState, LongestThrowCm);
	DOREPLIFETIME(ANoclipPlayerState, LongestThrowItem);
}

void ANoclipPlayerState::ResetShiftStats()
{
	DamageCaused = 0;
	ValueCarried = 0;
	TimeAlone = 0.f;
	LongestThrowCm = 0.f;
	LongestThrowItem = FText::GetEmpty();
}

void ANoclipPlayerState::SetDead(bool bInDead, ENoclipDeathCause Cause)
{
	bDead = bInDead;
	DeathCause = bInDead ? Cause : ENoclipDeathCause::None;
	ForceNetUpdate();
}

void ANoclipPlayerState::SetCharacterIndex(int32 InIndex)
{
	CharacterIndex = NoclipCharacters::ClampPlayable(InIndex);
	ForceNetUpdate();
}

void ANoclipPlayerState::CopyProperties(APlayerState* PlayerState)
{
	Super::CopyProperties(PlayerState);
	if (ANoclipPlayerState* Other = Cast<ANoclipPlayerState>(PlayerState))
	{
		Other->CharacterIndex = CharacterIndex;
		Other->ToolMask = ToolMask;
		Other->MedkitCount = MedkitCount;
	}
}

bool ANoclipPlayerState::HasTool(ENoclipTool Tool) const
{
	if (Tool == ENoclipTool::Medkit)
	{
		return MedkitCount > 0;
	}
	return Tool != ENoclipTool::None && (ToolMask & NoclipTools::Bit(Tool)) != 0;
}

void ANoclipPlayerState::GiveTool(ENoclipTool Tool, int32 Count)
{
	if (Tool == ENoclipTool::Medkit)
	{
		MedkitCount = static_cast<uint8>(FMath::Clamp<int32>(MedkitCount + Count, 0, NoclipTools::MaxMedkits));
	}
	else if (Tool != ENoclipTool::None)
	{
		ToolMask |= NoclipTools::Bit(Tool);
	}
	ForceNetUpdate();
}

void ANoclipPlayerState::TakeTool(ENoclipTool Tool)
{
	if (Tool == ENoclipTool::Medkit)
	{
		MedkitCount = MedkitCount > 0 ? MedkitCount - 1 : 0;
	}
	else if (Tool != ENoclipTool::None)
	{
		ToolMask &= ~NoclipTools::Bit(Tool);
	}
	ForceNetUpdate();
}

void ANoclipPlayerState::StripShopTools()
{
	ToolMask &= ~NoclipTools::Bit(ENoclipTool::Flashlight);
	MedkitCount = 0;
	ForceNetUpdate();
}
