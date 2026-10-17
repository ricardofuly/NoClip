// Copyright Fuly Games. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class APlayerController;

/** Acoes que aparecem como dica de botao na UI. */
enum class ENoclipPrompt : uint8
{
	Grab,
	Throw,
	Jump,
	Sprint,
	Pause,
	Ready,
	Confirm,
	Back
};

enum class ENoclipPadStyle : uint8
{
	Xbox,
	PlayStation
};

/**
 * Dicas de botao que acompanham o dispositivo em uso (CommonInput): teclado+mouse, Xbox ou PlayStation.
 * O estilo do controle e automatico, mas pode ser fixado na pausa (ou em noclip.PadGlyphs 0/1/2).
 */
namespace NoclipGlyphs
{
	NOCLIP_API bool IsUsingGamepad(const APlayerController* PC);
	NOCLIP_API ENoclipPadStyle GetPadStyle(const APlayerController* PC);
	NOCLIP_API FText GetKeyText(ENoclipPrompt Prompt, const APlayerController* PC);

	/** Auto -> Xbox -> PlayStation -> Auto. */
	NOCLIP_API void CyclePadStyleSetting();
	NOCLIP_API FText GetPadStyleSettingLabel();
}
