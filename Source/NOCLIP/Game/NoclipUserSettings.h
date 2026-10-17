// Copyright Fuly Games. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class UWorld;

enum class ENoclipSettingsSection : uint8
{
	Video,
	Audio,
	Controls,
	Accessibility
};

/**
 * Opcoes do jogador (por maquina), salvas em GameUserSettings.ini na secao [NoclipUserSettings].
 * Mesmas opcoes da tela de Opcoes do prototipo. O personagem e a HUD leem a cada uso (vale na hora);
 * as que mexem no mundo (pos-processo, tremido dos vertices) sao aplicadas por ApplyToWorld.
 */
struct NOCLIP_API FNoclipUserSettings
{
	// ---------- Video ----------
	/** 0 = 240 linhas, 1 = 360, 2 = 480, 3 = nativa (sem filtro PSX). */
	int32 PSXLines = 0;
	/** 0..100 */
	int32 Dither = 55;
	bool bVertexWobble = true;
	/** Graus (70..110). */
	int32 FieldOfView = 90;
	/** 0 = 60, 1 = 120, 2 = 144, 3 = sem limite. */
	int32 FPSLimit = 0;
	/** Linhas de varredura por cima da interface. */
	bool bScanlines = true;

	// ---------- Audio ----------
	int32 MasterVolume = 80;
	int32 VoiceVolume = 90;
	/** Musica e trilha (o resto do som segue o volume geral). */
	int32 MusicVolume = 70;
	/** 0 = deteccao de voz, 1 = apertar para falar. */
	int32 MicMode = 0;
	int32 MicSensitivity = 40;
	/** Fone/caixa (id do Audio Mixer) e microfone (nome do dispositivo). Vazio = padrao do sistema. */
	FString OutputDevice;
	FString InputDevice;

	// ---------- Controles ----------
	float MouseSensitivity = 1.f;
	float StickSensitivity = 1.f;
	bool bInvertY = false;
	bool bRumble = true;
	/** 0 = automatico, 1 = Xbox, 2 = PlayStation. */
	int32 PadGlyphs = 0;

	// ---------- Acessibilidade ----------
	bool bReduceFlashing = false;
	/** 0..100: passos, pouso e inclinacao lateral. */
	int32 CameraBob = 100;
	/** 0 = 100%, 1 = 125%, 2 = 150%. */
	int32 TextScale = 0;
	/** 0 = desligado, 1 = protanopia, 2 = deuteranopia, 3 = tritanopia. */
	int32 Colorblind = 0;
	bool bShowValueOnAim = true;

	// ---------- Historico (cracha do menu) ----------
	FString LastShiftSummary;

	// ---------- Estagiario escolhido (ficha de admissao) ----------
	/** Indice em NoclipCharacters; -1 = ainda nao escolheu (sorteia na primeira vez). */
	int32 CharacterIndex = -1;

	static FNoclipUserSettings& Get();
	void Save() const;

	/** Volta a secao para os valores padrao. */
	void ResetSection(ENoclipSettingsSection Section);

	/** Aplica o que e global (FPS, volume, escala da UI). */
	void ApplyGlobal() const;

	/** Aplica o que vive no mundo (pos-processo PSX, tremido dos vertices). Chame depois de gerar o nivel. */
	void ApplyToWorld(UWorld* World) const;

	float GetBobScale() const { return FMath::Clamp(CameraBob / 100.f, 0.f, 1.f); }
	bool UsesColorblindPalette() const { return Colorblind != 0; }

	static constexpr float MinSensitivity = 0.25f;
	static constexpr float MaxSensitivity = 3.f;
	static constexpr float SensitivityStep = 0.25f;

private:
	void Load();
	bool bLoaded = false;
};
