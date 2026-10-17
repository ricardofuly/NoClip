// Copyright Fuly Games. All Rights Reserved.

#include "UI/NoclipButton.h"
#include "UI/NoclipUIStyle.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/TextBlock.h"
#include "Components/SizeBox.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Brushes/SlateNoResource.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(NoclipButton)

using namespace NoclipUI;

namespace
{
	/** Key() devolve o texto por referencia a ponteiro cru; os membros aqui sao TObjectPtr. */
	UWidget* MakeKeyInto(UWidgetTree* Tree, TObjectPtr<UTextBlock>& OutText, const FLinearColor& TextColor, const FLinearColor& BorderColor)
	{
		UTextBlock* Raw = nullptr;
		UWidget* Cap = Key(Tree, Raw, TextColor, BorderColor);
		OutText = Raw;
		return Cap;
	}
}

UNoclipButtonStyle::UNoclipButtonStyle()
{
	const FSlateBrush Empty = FSlateNoResource();
	NormalBase = Empty;
	NormalHovered = Empty;
	NormalPressed = Empty;
	SelectedBase = Empty;
	SelectedHovered = Empty;
	SelectedPressed = Empty;
	Disabled = Empty;
	ButtonPadding = FMargin(0.f);
	CustomPadding = FMargin(0.f);
	MinWidth = 0;
	MinHeight = 0;
}

UNoclipButton::UNoclipButton(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	Style = UNoclipButtonStyle::StaticClass();
}

void UNoclipButton::InitializeNativeClassData()
{
	Super::InitializeNativeClassData();

	// Classe nativa sem Blueprint: a arvore e montada aqui, antes do CommonButtonBase embrulhar a raiz no botao interno.
	if (!WidgetTree)
	{
		WidgetTree = NewObject<UWidgetTree>(this, TEXT("WidgetTree"), RF_Transient);
	}
	if (WidgetTree->RootWidget)
	{
		return;
	}

	SizeBox = WidgetTree->ConstructWidget<USizeBox>();
	RootOverlay = WidgetTree->ConstructWidget<UOverlay>();
	SizeBox->AddChild(RootOverlay);

	Frame = Box(WidgetTree, Panel(), FMargin(20.f, 0.f), Line(), 1.f);
	Frame->SetVerticalAlignment(VAlign_Center);
	UOverlaySlot* FrameSlot = RootOverlay->AddChildToOverlay(Frame);
	FrameSlot->SetHorizontalAlignment(HAlign_Fill);
	FrameSlot->SetVerticalAlignment(VAlign_Fill);

	// *:focus-visible { outline: 2px solid #E8B931; outline-offset: 3px } — desenhado fora da caixa, sem mudar o layout.
	FocusRing = Box(WidgetTree, FLinearColor::Transparent, FMargin(0.f), Amber(), 2.f);
	FocusRing->SetVisibility(ESlateVisibility::Collapsed);
	UOverlaySlot* RingSlot = RootOverlay->AddChildToOverlay(FocusRing);
	RingSlot->SetHorizontalAlignment(HAlign_Fill);
	RingSlot->SetVerticalAlignment(VAlign_Fill);
	RingSlot->SetPadding(FMargin(-5.f));

	Row = WidgetTree->ConstructWidget<UHorizontalBox>();
	Frame->SetContent(Row);

	WidgetTree->RootWidget = SizeBox;
}

void UNoclipButton::Configure(const FText& InLabel, ENoclipButtonKind InKind, ENoclipButtonStyle InStyle)
{
	Kind = InKind;
	ButtonStyle = InStyle;
	RawLabel = InLabel;
	switch (ButtonStyle)
	{
	case ENoclipButtonStyle::Menu:    FontPx = 22.f; bCentered = false; break;
	case ENoclipButtonStyle::Compact: FontPx = 20.f; bCentered = true; break;
	case ENoclipButtonStyle::Tab:     FontPx = 22.f; bCentered = false; break;
	case ENoclipButtonStyle::Step:    FontPx = 22.f; bCentered = true; break;
	case ENoclipButtonStyle::Seg:     FontPx = 14.f; bCentered = false; break;
	case ENoclipButtonStyle::Select:  FontPx = 15.f; bCentered = false; break;
	case ENoclipButtonStyle::Check:   FontPx = 15.f; bCentered = false; break;
	case ENoclipButtonStyle::Row:     FontPx = 15.f; bCentered = false; break;
	case ENoclipButtonStyle::Link:    FontPx = 14.f; bCentered = false; break;
	case ENoclipButtonStyle::Photo:   FontPx = 13.f; bCentered = false; break;
	}
	Rebuild();
}

void UNoclipButton::Rebuild()
{
	if (!Row)
	{
		return;
	}
	Row->ClearChildren();
	LabelText = nullptr;
	SubText = nullptr;
	SuffixText = nullptr;
	SuffixKey = nullptr;
	SuffixKeyText = nullptr;
	TabBar = nullptr;
	CheckBox = nullptr;
	CheckMark = nullptr;

	const bool bOswald = ButtonStyle == ENoclipButtonStyle::Menu || ButtonStyle == ENoclipButtonStyle::Compact || ButtonStyle == ENoclipButtonStyle::Tab;
	const FSlateFontInfo LabelFont = bOswald ? Oswald(FontPx, false, ButtonStyle == ENoclipButtonStyle::Tab ? 0.05f : 0.06f)
		: (ButtonStyle == ENoclipButtonStyle::Step ? Mono(FontPx) : Mono(FontPx, ButtonStyle == ENoclipButtonStyle::Seg ? 600 : 400));

	float Height = 52.f;
	FMargin Pad(20.f, 0.f);
	switch (ButtonStyle)
	{
	case ENoclipButtonStyle::Compact: Height = 48.f; Pad = FMargin(18.f, 0.f); break;
	case ENoclipButtonStyle::Tab:     Height = 52.f; Pad = FMargin(0.f); break;
	case ENoclipButtonStyle::Step:    Height = 48.f; Pad = FMargin(0.f); break;
	case ENoclipButtonStyle::Seg:     Height = 48.f; Pad = FMargin(16.f, 8.f); break;
	case ENoclipButtonStyle::Select:  Height = 44.f; Pad = FMargin(12.f, 0.f); break;
	case ENoclipButtonStyle::Check:   Height = 44.f; Pad = FMargin(0.f); break;
	case ENoclipButtonStyle::Row:     Height = 56.f; Pad = FMargin(18.f, 0.f); break;
	case ENoclipButtonStyle::Link:    Height = 28.f; Pad = FMargin(0.f); break;
	case ENoclipButtonStyle::Photo:   Height = 0.f; Pad = FMargin(1.f); break;
	default: break;
	}
	SizeBox->SetMinDesiredHeight(Height);
	if (ButtonStyle == ENoclipButtonStyle::Step)
	{
		SizeBox->SetWidthOverride(48.f);
		SizeBox->SetHeightOverride(48.f);
	}
	Frame->SetPadding(Pad);

	if (ButtonStyle == ENoclipButtonStyle::Tab)
	{
		// border-left: 3px solid (ambar quando ativa)
		TabBar = Box(WidgetTree, FLinearColor::Transparent, FMargin(0.f));
		UHorizontalBoxSlot* BarSlot = Row->AddChildToHorizontalBox(Sized(WidgetTree, TabBar, 3.f, 0.f));
		BarSlot->SetVerticalAlignment(VAlign_Fill);
		AddH(Row, Gap(WidgetTree, 15.f, 1.f));
	}
	if (ButtonStyle == ENoclipButtonStyle::Check)
	{
		CheckBox = Box(WidgetTree, Bg(), FMargin(0.f), Muted(), 1.5f, 3.f);
		CheckBox->SetHorizontalAlignment(HAlign_Center);
		CheckBox->SetVerticalAlignment(VAlign_Center);
		CheckMark = Txt(WidgetTree, FText::FromString(TEXT("✓")), Mono(16.f, 600), Ink());
		CheckBox->SetContent(CheckMark);
		AddH(Row, Sized(WidgetTree, CheckBox, 22.f, 22.f));
		AddH(Row, Gap(WidgetTree, 10.f, 1.f));
	}

	if (ButtonStyle == ENoclipButtonStyle::Link)
	{
		SuffixKey = MakeKeyInto(WidgetTree, SuffixKeyText, Text(), Muted());
		AddH(Row, SuffixKey);
		LabelText = Txt(WidgetTree, RawLabel, Mono(FontPx), Amber());
		AddH(Row, LabelText, 8.f);
		RefreshVisual();
		return;
	}

	if (ButtonStyle == ENoclipButtonStyle::Seg)
	{
		UVerticalBox* Col = WidgetTree->ConstructWidget<UVerticalBox>();
		LabelText = Txt(WidgetTree, RawLabel, LabelFont, Soft());
		Col->AddChildToVerticalBox(LabelText);
		SubText = Txt(WidgetTree, FText::GetEmpty(), Mono(12.f), Muted());
		AddV(Col, SubText, 2.f);
		AddH(Row, Col, 0.f, true);
	}
	else if (ButtonStyle != ENoclipButtonStyle::Row && ButtonStyle != ENoclipButtonStyle::Photo)
	{
		const bool bUpper = bOswald;
		LabelText = Txt(WidgetTree, bUpper ? Upper(RawLabel) : RawLabel, LabelFont, Text());
		if (bCentered)
		{
			LabelText->SetJustification(ETextJustify::Center);
		}
		UHorizontalBoxSlot* LabelSlot = AddH(Row, LabelText, 0.f, !bCentered || ButtonStyle == ENoclipButtonStyle::Step);
		if (bCentered && ButtonStyle != ENoclipButtonStyle::Step)
		{
			// justify-content: center
			Row->RemoveChild(LabelText);
			AddH(Row, Gap(WidgetTree, 0.f, 0.f), 0.f, true);
			AddH(Row, LabelText, 0.f, false);
			AddH(Row, Gap(WidgetTree, 0.f, 0.f), 0.f, true);
		}
		else if (LabelSlot)
		{
			LabelSlot->SetHorizontalAlignment(ButtonStyle == ENoclipButtonStyle::Step ? HAlign_Center : HAlign_Left);
		}
	}

	if (ButtonStyle == ENoclipButtonStyle::Select)
	{
		SuffixText = Txt(WidgetTree, FText::FromString(TEXT("‹ ›")), Mono(15.f), Muted());
		AddH(Row, SuffixText, 12.f);
	}

	RefreshVisual();
}

void UNoclipButton::SetLabel(const FText& InLabel)
{
	RawLabel = InLabel;
	if (LabelText)
	{
		const bool bOswald = ButtonStyle == ENoclipButtonStyle::Menu || ButtonStyle == ENoclipButtonStyle::Compact || ButtonStyle == ENoclipButtonStyle::Tab;
		LabelText->SetText(bOswald ? Upper(InLabel) : InLabel);
	}
}

void UNoclipButton::SetSuffix(const FText& InSuffix, bool bAsKey)
{
	if (!Row)
	{
		return;
	}
	if (bAsKey || ButtonStyle == ENoclipButtonStyle::Link)
	{
		if (!SuffixKey)
		{
			if (bCentered)
			{
				// Tecla junto do texto centralizado: entra antes do espaco final.
				SuffixKey = MakeKeyInto(WidgetTree, SuffixKeyText, Kind == ENoclipButtonKind::Primary ? Ink() : Text(), Kind == ENoclipButtonKind::Primary ? Ink() : Muted());
				Row->InsertChildAt(Row->GetChildrenCount() - 1, SuffixKey);
				if (UHorizontalBoxSlot* KeySlot = Cast<UHorizontalBoxSlot>(SuffixKey->Slot))
				{
					KeySlot->SetPadding(FMargin(12.f, 0.f, 0.f, 0.f));
					KeySlot->SetVerticalAlignment(VAlign_Center);
				}
			}
			else
			{
				SuffixKey = MakeKeyInto(WidgetTree, SuffixKeyText, Kind == ENoclipButtonKind::Primary ? Ink() : Text(), Kind == ENoclipButtonKind::Primary ? Ink() : Muted());
				AddH(Row, SuffixKey, 16.f);
			}
		}
		SuffixKeyText->SetText(InSuffix);
		SuffixKey->SetVisibility(InSuffix.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	}
	else
	{
		if (!SuffixText)
		{
			SuffixText = Txt(WidgetTree, FText::GetEmpty(), Oswald(FontPx, false), Text());
			if (bCentered && Row->GetChildrenCount() > 1)
			{
				// justify-content:center com gap 12: o sufixo fica colado no texto.
				Row->InsertChildAt(Row->GetChildrenCount() - 1, SuffixText);
				if (UHorizontalBoxSlot* SufSlot = Cast<UHorizontalBoxSlot>(SuffixText->Slot))
				{
					SufSlot->SetPadding(FMargin(12.f, 0.f, 0.f, 0.f));
					SufSlot->SetVerticalAlignment(VAlign_Center);
				}
			}
			else
			{
				AddH(Row, SuffixText, 16.f);
			}
		}
		SuffixText->SetText(InSuffix);
		SuffixText->SetVisibility(InSuffix.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	}
	RefreshVisual();
}

void UNoclipButton::SetSuffixNote(const FText& InNote)
{
	SetSuffix(InNote, false);
	if (SuffixText)
	{
		SuffixText->SetFont(Mono(13.f));
	}
	bSuffixNote = true;
	RefreshVisual();
}

void UNoclipButton::SetSubtext(const FText& InSubtext)
{
	if (SubText)
	{
		SubText->SetText(InSubtext);
	}
}

void UNoclipButton::SetOn(bool bInOn)
{
	bOn = bInOn;
	RefreshVisual();
}

void UNoclipButton::SetValueText(const FText& InValue)
{
	SetLabel(InValue);
}

void UNoclipButton::SetRowContent(UWidget* Content)
{
	if (Row && Content)
	{
		Row->ClearChildren();
		AddH(Row, Content, 0.f, true);
	}
}

void UNoclipButton::SetMinHeight(float Height)
{
	if (SizeBox)
	{
		SizeBox->SetMinDesiredHeight(Height);
	}
}

void UNoclipButton::SetFontPx(float CssPx)
{
	FontPx = CssPx;
	Rebuild();
}

void UNoclipButton::SetCentered(bool bInCentered)
{
	bCentered = bInCentered;
	Rebuild();
}

void UNoclipButton::NativeOnHovered()
{
	Super::NativeOnHovered();
	bHovered = true;
	// O foco segue o mouse: um unico item destacado, e o teclado/controle continua de onde o mouse parou.
	if (IsInteractionEnabled() && !bFocused)
	{
		SetFocus();
	}
	RefreshVisual();
}

void UNoclipButton::NativeOnUnhovered()
{
	Super::NativeOnUnhovered();
	bHovered = false;
	RefreshVisual();
}

void UNoclipButton::HandleFocusReceived()
{
	Super::HandleFocusReceived();
	bFocused = true;
	RefreshVisual();
}

void UNoclipButton::HandleFocusLost()
{
	Super::HandleFocusLost();
	bFocused = false;
	RefreshVisual();
}

void UNoclipButton::NativeOnEnabled()
{
	Super::NativeOnEnabled();
	RefreshVisual();
}

void UNoclipButton::NativeOnDisabled()
{
	Super::NativeOnDisabled();
	bHovered = false;
	bFocused = false;
	RefreshVisual();
}

void UNoclipButton::RefreshVisual()
{
	if (!Frame)
	{
		return;
	}

	const bool bHot = bHovered || bFocused;
	const bool bEnabled = IsInteractionEnabled();

	FLinearColor Fill = Panel();
	FLinearColor Outline = Line();
	FLinearColor TextColor = Text();
	FLinearColor SuffixColor = Text();
	bool bRing = false;

	switch (ButtonStyle)
	{
	case ENoclipButtonStyle::Seg:
		Fill = bOn ? PanelHover() : Panel();
		Outline = bOn ? Amber() : Line();
		TextColor = bOn ? TextBright() : Soft();
		bRing = bFocused;
		break;

	case ENoclipButtonStyle::Tab:
		Fill = (bOn || bHot) ? Panel() : FLinearColor::Transparent;
		Outline = FLinearColor::Transparent;
		TextColor = (bOn || bHot) ? TextBright() : Soft();
		bRing = bFocused;
		if (TabBar)
		{
			SetBox(TabBar, bOn ? Amber() : FLinearColor::Transparent);
		}
		break;

	case ENoclipButtonStyle::Step:
		Fill = Panel();
		Outline = bHot ? Amber() : Line();
		TextColor = Text();
		bRing = bFocused;
		break;

	case ENoclipButtonStyle::Select:
		Fill = Bg();
		Outline = bHot ? Amber() : Line();
		TextColor = Text();
		SuffixColor = Muted();
		bRing = bFocused;
		break;

	case ENoclipButtonStyle::Link:
		Fill = FLinearColor::Transparent;
		Outline = FLinearColor::Transparent;
		TextColor = bHot ? AmberHover() : Amber();
		bRing = bFocused;
		break;

	case ENoclipButtonStyle::Photo:
		Fill = FLinearColor::Transparent;
		Outline = bHot ? Amber() : ManilaRule();
		bRing = bFocused;
		break;

	case ENoclipButtonStyle::Check:
		Fill = FLinearColor::Transparent;
		Outline = FLinearColor::Transparent;
		TextColor = Text();
		bRing = bFocused;
		if (CheckBox)
		{
			SetBox(CheckBox, bOn ? Amber() : Bg(), bOn ? Amber() : (bHot ? Amber() : Muted()), 1.5f, 3.f);
		}
		if (CheckMark)
		{
			CheckMark->SetVisibility(bOn ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden);
		}
		break;

	default:
		if (Kind == ENoclipButtonKind::Primary)
		{
			Fill = bHot ? AmberHover() : Amber();
			Outline = Amber();
			TextColor = Ink();
			SuffixColor = Ink();
		}
		else if (Kind == ENoclipButtonKind::Danger)
		{
			Fill = bHot ? Hex(TEXT("2A1712")) : Panel();
			Outline = bHot ? Danger() : DangerLine();
			TextColor = bHot ? Hex(TEXT("FFD2C8")) : DangerText();
			SuffixColor = TextColor;
		}
		else
		{
			Fill = bHot ? PanelHover() : Panel();
			Outline = bHot ? Amber() : Line();
			TextColor = bHot ? TextBright() : Text();
			SuffixColor = TextColor;
		}
		break;
	}

	if (!bEnabled)
	{
		// [disabled]: opacidade reduzida, sem destaque.
		Fill = ButtonStyle == ENoclipButtonStyle::Select ? Bg() : (Kind == ENoclipButtonKind::Primary ? Hex(TEXT("5E5023")) : Panel());
		Outline = Line();
		TextColor = Muted();
		SuffixColor = Muted();
		bRing = false;
	}

	SetBox(Frame, Fill, Outline, ButtonStyle == ENoclipButtonStyle::Photo && bHot ? 2.f : 1.f);
	if (LabelText)
	{
		LabelText->SetColorAndOpacity(FSlateColor(TextColor));
	}
	if (SuffixText)
	{
		SuffixText->SetColorAndOpacity(FSlateColor(bSuffixNote ? Muted() : SuffixColor));
	}
	if (SuffixKeyText)
	{
		SuffixKeyText->SetColorAndOpacity(FSlateColor(Kind == ENoclipButtonKind::Primary && bEnabled ? Ink() : Text()));
	}
	if (FocusRing)
	{
		FocusRing->SetVisibility(bRing ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
}
