// Copyright Fuly Games. All Rights Reserved.

#include "UI/NoclipInputGlyphs.h"
#include "CommonInputSubsystem.h"
#include "CommonInputTypeEnum.h"
#include "GameFramework/PlayerController.h"
#include "Engine/LocalPlayer.h"
#include "HAL/IConsoleManager.h"
#include "Game/NoclipUserSettings.h"

#define LOCTEXT_NAMESPACE "NoclipGlyphs"

static TAutoConsoleVariable<int32> CVarNoclipPadGlyphs(
	TEXT("noclip.PadGlyphs"), 0,
	TEXT("Forca os icones do controle na UI: 0 = usa a opcao do jogador, 1 = Xbox, 2 = PlayStation."));

namespace NoclipGlyphs
{
	static const UCommonInputSubsystem* GetInput(const APlayerController* PC)
	{
		const ULocalPlayer* LP = PC ? PC->GetLocalPlayer() : nullptr;
		return LP ? UCommonInputSubsystem::Get(LP) : nullptr;
	}

	bool IsUsingGamepad(const APlayerController* PC)
	{
		const UCommonInputSubsystem* Input = GetInput(PC);
		return Input && Input->GetCurrentInputType() == ECommonInputType::Gamepad;
	}

	ENoclipPadStyle GetPadStyle(const APlayerController* PC)
	{
		const int32 Forced = CVarNoclipPadGlyphs.GetValueOnGameThread();
		const int32 Setting = Forced != 0 ? Forced : FNoclipUserSettings::Get().PadGlyphs;
		if (Setting == 1)
		{
			return ENoclipPadStyle::Xbox;
		}
		if (Setting == 2)
		{
			return ENoclipPadStyle::PlayStation;
		}

		if (const UCommonInputSubsystem* Input = GetInput(PC))
		{
			const FString Name = Input->GetCurrentGamepadName().ToString();
			if (Name.Contains(TEXT("PS")) || Name.Contains(TEXT("Dual")) || Name.Contains(TEXT("Sony")))
			{
				return ENoclipPadStyle::PlayStation;
			}
		}
		return ENoclipPadStyle::Xbox;
	}

	FText GetKeyText(ENoclipPrompt Prompt, const APlayerController* PC)
	{
		if (!IsUsingGamepad(PC))
		{
			switch (Prompt)
			{
			case ENoclipPrompt::Grab:    return LOCTEXT("KbGrab", "E");
			case ENoclipPrompt::Throw:   return LOCTEXT("KbThrow", "Mouse 1");
			case ENoclipPrompt::Jump:    return LOCTEXT("KbJump", "Espaço");
			case ENoclipPrompt::Sprint:  return LOCTEXT("KbSprint", "Shift");
			case ENoclipPrompt::Pause:   return LOCTEXT("KbPause", "Esc");
			case ENoclipPrompt::Ready:   return LOCTEXT("KbReady", "Enter");
			case ENoclipPrompt::Confirm: return LOCTEXT("KbConfirm", "Enter");
			case ENoclipPrompt::Back:    return LOCTEXT("KbBack", "Esc");
			}
			return FText::GetEmpty();
		}

		if (GetPadStyle(PC) == ENoclipPadStyle::PlayStation)
		{
			switch (Prompt)
			{
			case ENoclipPrompt::Grab:    return LOCTEXT("PsGrab", "□");
			case ENoclipPrompt::Throw:   return LOCTEXT("PsThrow", "R2");
			case ENoclipPrompt::Jump:    return LOCTEXT("PsJump", "×");
			case ENoclipPrompt::Sprint:  return LOCTEXT("PsSprint", "L2");
			case ENoclipPrompt::Pause:   return LOCTEXT("PsPause", "Options");
			case ENoclipPrompt::Ready:   return LOCTEXT("PsReady", "△");
			case ENoclipPrompt::Confirm: return LOCTEXT("PsConfirm", "×");
			case ENoclipPrompt::Back:    return LOCTEXT("PsBack", "○");
			}
			return FText::GetEmpty();
		}

		switch (Prompt)
		{
		case ENoclipPrompt::Grab:    return LOCTEXT("XbGrab", "X");
		case ENoclipPrompt::Throw:   return LOCTEXT("XbThrow", "RT");
		case ENoclipPrompt::Jump:    return LOCTEXT("XbJump", "A");
		case ENoclipPrompt::Sprint:  return LOCTEXT("XbSprint", "LT");
		case ENoclipPrompt::Pause:   return LOCTEXT("XbPause", "Menu");
		case ENoclipPrompt::Ready:   return LOCTEXT("XbReady", "Y");
		case ENoclipPrompt::Confirm: return LOCTEXT("XbConfirm", "A");
		case ENoclipPrompt::Back:    return LOCTEXT("XbBack", "B");
		}
		return FText::GetEmpty();
	}

	void CyclePadStyleSetting()
	{
		FNoclipUserSettings& Settings = FNoclipUserSettings::Get();
		Settings.PadGlyphs = (Settings.PadGlyphs + 1) % 3;
		Settings.Save();
	}

	FText GetPadStyleSettingLabel()
	{
		switch (FNoclipUserSettings::Get().PadGlyphs)
		{
		case 1:  return LOCTEXT("PadXbox", "Xbox");
		case 2:  return LOCTEXT("PadPS", "PlayStation");
		default: return LOCTEXT("PadAuto", "Automático");
		}
	}
}

#undef LOCTEXT_NAMESPACE
