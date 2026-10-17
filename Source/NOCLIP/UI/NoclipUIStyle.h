// Copyright Fuly Games. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Fonts/SlateFontInfo.h"
#include "Layout/Margin.h"
#include "Components/SlateWrapperTypes.h"

class UWidget;
class UWidgetTree;
class UTextBlock;
class UBorder;
class UHorizontalBox;
class UHorizontalBoxSlot;
class UVerticalBox;
class UVerticalBoxSlot;
class USizeBox;
class UEditableTextBox;
class UUniformGridPanel;
class UUniformGridSlot;

/**
 * Kit visual da UI do NOCLIP, copiado do prototipo no Claude Design.
 *
 * Unidades: a UI usa DPI "Scale To Fit" com tela de projeto 1280x720 (igual aos artboards), entao
 * posicoes e tamanhos em Slate sao os mesmos "px" do prototipo. Fontes recebem o tamanho em px de CSS
 * e convertem para pontos do Slate (96 DPI): pt = px * 0.75.
 */
namespace NoclipUI
{
	NOCLIP_API FLinearColor Hex(const TCHAR* InHex, float Alpha = 1.f);

	// ---------- Paleta (tokens do prototipo) ----------
	inline FLinearColor Bg()         { return Hex(TEXT("12100C")); }
	inline FLinearColor BgDeep()     { return Hex(TEXT("0E0C09")); }
	inline FLinearColor Panel()      { return Hex(TEXT("1C1913")); }
	inline FLinearColor Panel2()     { return Hex(TEXT("15130E")); }
	inline FLinearColor PanelHover() { return Hex(TEXT("2A2416")); }
	inline FLinearColor Line()       { return Hex(TEXT("3A3326")); }
	inline FLinearColor Line2()      { return Hex(TEXT("2A2519")); }
	inline FLinearColor Manila()     { return Hex(TEXT("D8C38D")); }
	inline FLinearColor ManilaRule() { return Hex(TEXT("6B5A2E")); }
	inline FLinearColor Text()       { return Hex(TEXT("EDE3C8")); }
	inline FLinearColor TextBright() { return Hex(TEXT("FFF4D6")); }
	inline FLinearColor Muted()      { return Hex(TEXT("A89C7E")); }
	inline FLinearColor Soft()       { return Hex(TEXT("CFC3A3")); }
	inline FLinearColor Amber()      { return Hex(TEXT("E8B931")); }
	inline FLinearColor AmberHover() { return Hex(TEXT("F5D46A")); }
	inline FLinearColor Ink()        { return Hex(TEXT("1A1508")); }
	inline FLinearColor HudText()    { return Hex(TEXT("F3EAD2")); }
	inline FLinearColor Chip()       { return Hex(TEXT("12100C"), 0.72f); }
	inline FLinearColor ChipLine()   { return Hex(TEXT("E8B931"), 0.35f); }
	inline FLinearColor Note()       { return Hex(TEXT("221D10")); }
	inline FLinearColor NoteLine()   { return Hex(TEXT("7A6A2E")); }
	inline FLinearColor NoteText()   { return Hex(TEXT("F1DFA6")); }
	inline FLinearColor DangerLine() { return Hex(TEXT("7A3A2E")); }
	inline FLinearColor DangerDeep() { return Hex(TEXT("8E2B1E")); }

	/** Cores de estado. Com a opcao de daltonismo, verde vira azul e vermelho vira laranja. */
	NOCLIP_API FLinearColor Ok();
	NOCLIP_API FLinearColor OkText();
	NOCLIP_API FLinearColor OkDeep();
	NOCLIP_API FLinearColor Danger();
	NOCLIP_API FLinearColor DangerText();
	NOCLIP_API FLinearColor Warn();

	// ---------- Fontes ----------
	/** Oswald (titulos e botoes). Peso 700 ou 500. LetterEm = letter-spacing em em (CSS). */
	NOCLIP_API FSlateFontInfo Oswald(float CssPx, bool bBold = true, float LetterEm = 0.f);
	/** IBM Plex Mono (texto). Peso 400, 500 ou 600. */
	NOCLIP_API FSlateFontInfo Mono(float CssPx, int32 Weight = 400, float LetterEm = 0.f);

	// ---------- Construtores ----------
	NOCLIP_API UTextBlock* Txt(UWidgetTree* Tree, const FText& InText, const FSlateFontInfo& Font, const FLinearColor& Color, bool bShadow = false);
	NOCLIP_API UTextBlock* Wrap(UTextBlock* Text, float LineHeight = 1.5f);

	/** Caixa com fundo e contorno opcional (contorno com alpha 0 = sem contorno). */
	NOCLIP_API UBorder* Box(UWidgetTree* Tree, const FLinearColor& Fill, const FMargin& Padding, const FLinearColor& Outline = FLinearColor::Transparent, float OutlineWidth = 1.f, float Radius = 0.f);
	NOCLIP_API void SetBox(UBorder* Box, const FLinearColor& Fill, const FLinearColor& Outline = FLinearColor::Transparent, float OutlineWidth = 1.f, float Radius = 0.f);

	NOCLIP_API USizeBox* Sized(UWidgetTree* Tree, UWidget* Child, float Width = 0.f, float Height = 0.f);
	NOCLIP_API UWidget* Gap(UWidgetTree* Tree, float Width, float Height);

	/** Rotulo "label" do prototipo: mono 12px, caixa alta, espacado. */
	NOCLIP_API UTextBlock* Label(UWidgetTree* Tree, const FText& InText, const FLinearColor& Color = Hex(TEXT("A89C7E")));

	/** Titulo de tela (Oswald 700, caixa alta, ambar). */
	NOCLIP_API UTextBlock* H1(UWidgetTree* Tree, const FText& InText, float CssPx, const FLinearColor& Color = Hex(TEXT("E8B931")), float LetterEm = 0.03f);

	/** Tecla ("keycap"): borda 1px com base de 3px e cantos de 4px. */
	NOCLIP_API UWidget* Key(UWidgetTree* Tree, UTextBlock*& OutText, const FLinearColor& TextColor = Hex(TEXT("EDE3C8")), const FLinearColor& BorderColor = Hex(TEXT("A89C7E")),
		float Height = 28.f, float FontPx = 13.f, const FLinearColor& Fill = FLinearColor::Transparent);

	/** Linha tracejada horizontal (1px). */
	NOCLIP_API UWidget* Dashed(UWidgetTree* Tree, const FLinearColor& Color, float Thickness = 1.f);
	NOCLIP_API UWidget* Rule(UWidgetTree* Tree, const FLinearColor& Color, float Thickness = 1.f);

	/** Linhas de layout com espacamento (gap do flexbox). */
	NOCLIP_API UVerticalBoxSlot* AddV(UVerticalBox* Box, UWidget* Child, float GapBefore = 0.f, EHorizontalAlignment HAlign = HAlign_Fill);
	NOCLIP_API UHorizontalBoxSlot* AddH(UHorizontalBox* Box, UWidget* Child, float GapBefore = 0.f, bool bFill = false, EVerticalAlignment VAlign = VAlign_Center);

	/** Celula de grade que ocupa a celula inteira (cartoes do mesmo tamanho, como grid 1fr 1fr). */
	NOCLIP_API UUniformGridSlot* AddCell(UUniformGridPanel* Grid, UWidget* Child, int32 Row, int32 Column);

	/** Linha "rotulo ....... valor" (justify-content: space-between). */
	NOCLIP_API UHorizontalBox* SpaceBetween(UWidgetTree* Tree, UWidget* Left, UWidget* Right, EVerticalAlignment VAlign = VAlign_Center);

	/** Campo de texto (.input): fundo #12100C, borda #3A3326, ambar com foco. */
	NOCLIP_API UEditableTextBox* Input(UWidgetTree* Tree, const FText& Hint, float CssFontPx = 16.f);

	/** Barra de progresso feita de caixas. */
	struct NOCLIP_API FBar
	{
		UWidget* Root = nullptr;
		UBorder* Back = nullptr;
		UBorder* Fill = nullptr;
		UHorizontalBoxSlot* FillSlot = nullptr;
		UHorizontalBoxSlot* RestSlot = nullptr;

		void SetPercent(float Percent) const;
		void SetColor(const FLinearColor& Color) const;
	};
	NOCLIP_API FBar MakeBar(UWidgetTree* Tree, float Height, const FLinearColor& Back, const FLinearColor& FillColor, const FLinearColor& Outline = FLinearColor::Transparent);

	/** "mm:ss" */
	NOCLIP_API FText FormatClock(float Seconds);

	/** "$1.234" */
	NOCLIP_API FText FormatMoney(int32 Value);

	/**
	 * Nome de jogador (ou de sala) encurtado para caber no layout: "windows-22CD77A74E…".
	 * Vazio vira "Estagiario". Use em todo lugar que mostra nome (HUD, saguao, pausa, relatorio, sala de espera...).
	 */
	NOCLIP_API FString ShortName(const FString& Name, int32 MaxChars = 18);
	NOCLIP_API FText ShortNameText(const FString& Name, int32 MaxChars = 18);

	/** Caixa alta com acentos (FString::ToUpper so trata ASCII). */
	NOCLIP_API FText Upper(const FText& InText);
}
