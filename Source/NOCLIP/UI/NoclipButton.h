// Copyright Fuly Games. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "CommonButtonBase.h"
#include "NoclipButton.generated.h"

class UBorder;
class UTextBlock;
class USizeBox;
class UHorizontalBox;
class UOverlay;

UENUM()
enum class ENoclipButtonKind : uint8
{
	Normal,
	Primary,
	Danger
};

/** Variacoes visuais do prototipo (classes CSS). */
UENUM()
enum class ENoclipButtonStyle : uint8
{
	/** .btn do menu: Oswald 22px, 52px de altura, rotulo a esquerda e sufixo (→ / tecla) a direita. */
	Menu,
	/** .btn compacto: Oswald 20px, 48px, centralizado. */
	Compact,
	/** .seg: opcao de radio com titulo + dica (mono 14px). */
	Seg,
	/** .step: quadrado 48x48 com − / +. */
	Step,
	/** .tab: aba lateral com barra ambar a esquerda. */
	Tab,
	/** .sel: caixa de selecao (mono 15px, 44px); o valor troca com confirmar ou ‹ ›. */
	Select,
	/** Caixa de marcar 22x22 + texto. */
	Check,
	/** Linha de tabela (lista de salas): conteudo livre via SetContent. */
	Row,
	/** Link ambar com tecla na frente ("[Esc] Voltar ao inicio"). */
	Link,
	/** Foto clicavel (cracha): sem fundo nem padding, contorno ambar com hover/foco. Conteudo via SetRowContent. */
	Photo
};

/** Estilo vazio: o visual e desenhado pelo proprio UNoclipButton. */
UCLASS()
class NOCLIP_API UNoclipButtonStyle : public UCommonButtonStyle
{
	GENERATED_BODY()

public:
	UNoclipButtonStyle();
};

/**
 * Botao do NOCLIP sobre CommonButtonBase: clicavel com mouse, Enter/Espaco ou A/Cruz, navegavel com
 * setas/direcional. Hover e foco tem o mesmo destaque (o foco segue o mouse), como no prototipo.
 */
UCLASS()
class NOCLIP_API UNoclipButton : public UCommonButtonBase
{
	GENERATED_BODY()

public:
	UNoclipButton(const FObjectInitializer& ObjectInitializer);

	/** Precisa ser chamado logo depois de criar. */
	void Configure(const FText& InLabel, ENoclipButtonKind InKind, ENoclipButtonStyle InStyle = ENoclipButtonStyle::Menu);

	void SetLabel(const FText& InLabel);

	/** Texto a direita (→, dica). bAsKey desenha como tecla. */
	void SetSuffix(const FText& InSuffix, bool bAsKey = false);

	/** Texto pequeno a direita (mono 13px, apagado, sem caixa alta). */
	void SetSuffixNote(const FText& InNote);

	/** Segunda linha do .seg. */
	void SetSubtext(const FText& InSubtext);

	/** Marcado (.seg.on, .tab.on, caixa marcada). */
	void SetOn(bool bInOn);
	bool IsOn() const { return bOn; }

	/** Valor mostrado no .sel. */
	void SetValueText(const FText& InValue);

	/** Conteudo livre (estilo Row). */
	void SetRowContent(UWidget* Content);

	void SetMinHeight(float Height);
	void SetFontPx(float CssPx);
	void SetCentered(bool bCentered);

	/** Destacado por foco de teclado/controle (ou mouse em cima, que leva o foco junto). */
	bool IsShowingFocus() const { return bFocused; }

	ENoclipButtonStyle GetButtonStyle() const { return ButtonStyle; }

protected:
	virtual void InitializeNativeClassData() override;
	virtual void NativeOnHovered() override;
	virtual void NativeOnUnhovered() override;
	virtual void HandleFocusReceived() override;
	virtual void HandleFocusLost() override;
	virtual void NativeOnEnabled() override;
	virtual void NativeOnDisabled() override;

	void Rebuild();
	void RefreshVisual();

	UPROPERTY(Transient) TObjectPtr<USizeBox> SizeBox;
	UPROPERTY(Transient) TObjectPtr<UOverlay> RootOverlay;
	UPROPERTY(Transient) TObjectPtr<UBorder> Frame;
	UPROPERTY(Transient) TObjectPtr<UBorder> FocusRing;
	UPROPERTY(Transient) TObjectPtr<UBorder> TabBar;
	UPROPERTY(Transient) TObjectPtr<UBorder> CheckBox;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> CheckMark;
	UPROPERTY(Transient) TObjectPtr<UHorizontalBox> Row;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> LabelText;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> SubText;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> SuffixText;
	UPROPERTY(Transient) TObjectPtr<UWidget> SuffixKey;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> SuffixKeyText;

	ENoclipButtonKind Kind = ENoclipButtonKind::Normal;
	ENoclipButtonStyle ButtonStyle = ENoclipButtonStyle::Menu;
	FText RawLabel;
	float FontPx = 22.f;
	bool bCentered = false;
	bool bSuffixNote = false;
	bool bOn = false;
	bool bHovered = false;
	bool bFocused = false;
};
