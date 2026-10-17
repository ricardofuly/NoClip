// Copyright Fuly Games. All Rights Reserved.

#include "UI/NoclipUIStyle.h"
#include "Components/UniformGridPanel.h"
#include "Components/UniformGridSlot.h"
#include "Game/NoclipUserSettings.h"
#include "Blueprint/WidgetTree.h"
#include "Components/TextBlock.h"
#include "Components/Border.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/SizeBox.h"
#include "Components/Spacer.h"
#include "Components/EditableTextBox.h"
#include "Fonts/CompositeFont.h"
#include "Styling/CoreStyle.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "Brushes/SlateColorBrush.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "Misc/CoreDelegates.h"

namespace NoclipUI
{
	FLinearColor Hex(const TCHAR* InHex, float Alpha)
	{
		FLinearColor C(FColor::FromHex(InHex));
		C.A = Alpha;
		return C;
	}

	// ---------------------------------------------------------------- Cores de estado

	static bool CB() { return FNoclipUserSettings::Get().UsesColorblindPalette(); }
	FLinearColor Ok()         { return CB() ? Hex(TEXT("5AA9E6")) : Hex(TEXT("8CC66B")); }
	FLinearColor OkText()     { return CB() ? Hex(TEXT("A9D4F5")) : Hex(TEXT("B9E39F")); }
	FLinearColor OkDeep()     { return CB() ? Hex(TEXT("1F5C8F")) : Hex(TEXT("2F6B22")); }
	FLinearColor Danger()     { return CB() ? Hex(TEXT("F0A030")) : Hex(TEXT("E0644F")); }
	FLinearColor DangerText() { return CB() ? Hex(TEXT("F7C98A")) : Hex(TEXT("F2A293")); }
	FLinearColor Warn()       { return CB() ? Hex(TEXT("F7C98A")) : Hex(TEXT("FF8A75")); }

	// ---------------------------------------------------------------- Fontes

	namespace
	{
		TSharedPtr<FStandaloneCompositeFont> GOswald;
		TSharedPtr<FStandaloneCompositeFont> GMono;
		bool GFontsTried = false;

		FString FontPath(const TCHAR* File)
		{
			return FPaths::ConvertRelativePathToFull(FPaths::ProjectContentDir() / TEXT("NOCLIP/UI/Fonts") / File);
		}

		void AddFallback(FCompositeFont& Font)
		{
			// Simbolos que as fontes do prototipo nao tem (□ △ ○ → ✓ ⚠).
			const FString Fallback = FontPath(TEXT("DejaVuSans.ttf"));
			if (IFileManager::Get().FileExists(*Fallback))
			{
				Font.FallbackTypeface.Typeface.AppendFont(TEXT("Regular"), Fallback, EFontHinting::Default, EFontLoadingPolicy::LazyLoad);
			}
		}

		void EnsureFonts()
		{
			if (GFontsTried)
			{
				return;
			}
			GFontsTried = true;

			const FString OswaldBold = FontPath(TEXT("Oswald-Bold.ttf"));
			if (IFileManager::Get().FileExists(*OswaldBold))
			{
				GOswald = MakeShared<FStandaloneCompositeFont>(TEXT("Bold"), OswaldBold, EFontHinting::Default, EFontLoadingPolicy::LazyLoad);
				GOswald->DefaultTypeface.AppendFont(TEXT("Medium"), FontPath(TEXT("Oswald-Medium.ttf")), EFontHinting::Default, EFontLoadingPolicy::LazyLoad);
				AddFallback(*GOswald);
			}

			const FString MonoRegular = FontPath(TEXT("IBMPlexMono-Regular.ttf"));
			if (IFileManager::Get().FileExists(*MonoRegular))
			{
				GMono = MakeShared<FStandaloneCompositeFont>(TEXT("Regular"), MonoRegular, EFontHinting::Default, EFontLoadingPolicy::LazyLoad);
				GMono->DefaultTypeface.AppendFont(TEXT("Medium"), FontPath(TEXT("IBMPlexMono-Medium.ttf")), EFontHinting::Default, EFontLoadingPolicy::LazyLoad);
				GMono->DefaultTypeface.AppendFont(TEXT("SemiBold"), FontPath(TEXT("IBMPlexMono-SemiBold.ttf")), EFontHinting::Default, EFontLoadingPolicy::LazyLoad);
				AddFallback(*GMono);
			}

			FCoreDelegates::OnPreExit.AddLambda([]()
			{
				GOswald.Reset();
				GMono.Reset();
			});
		}

		int32 Spacing(float LetterEm)
		{
			return FMath::RoundToInt(LetterEm * 1000.f);
		}
	}

	FSlateFontInfo Oswald(float CssPx, bool bBold, float LetterEm)
	{
		EnsureFonts();
		FSlateFontInfo Font = GOswald.IsValid()
			? FSlateFontInfo(GOswald, CssPx * 0.75f, bBold ? FName(TEXT("Bold")) : FName(TEXT("Medium")))
			: FCoreStyle::GetDefaultFontStyle("Bold", FMath::RoundToInt(CssPx * 0.75f));
		Font.LetterSpacing = Spacing(LetterEm);
		return Font;
	}

	FSlateFontInfo Mono(float CssPx, int32 Weight, float LetterEm)
	{
		EnsureFonts();
		const FName Typeface = Weight >= 600 ? FName(TEXT("SemiBold")) : (Weight >= 500 ? FName(TEXT("Medium")) : FName(TEXT("Regular")));
		FSlateFontInfo Font = GMono.IsValid()
			? FSlateFontInfo(GMono, CssPx * 0.75f, Typeface)
			: FCoreStyle::GetDefaultFontStyle(Weight >= 600 ? "Bold" : "Regular", FMath::RoundToInt(CssPx * 0.75f));
		Font.LetterSpacing = Spacing(LetterEm);
		return Font;
	}

	// ---------------------------------------------------------------- Construtores

	UTextBlock* Txt(UWidgetTree* Tree, const FText& InText, const FSlateFontInfo& Font, const FLinearColor& Color, bool bShadow)
	{
		UTextBlock* T = Tree->ConstructWidget<UTextBlock>();
		T->SetText(InText);
		T->SetFont(Font);
		T->SetColorAndOpacity(FSlateColor(Color));
		if (bShadow)
		{
			// .hud { text-shadow: 0 2px 0 rgba(0,0,0,.75) }
			T->SetShadowOffset(FVector2D(0.f, 2.f));
			T->SetShadowColorAndOpacity(FLinearColor(0.f, 0.f, 0.f, 0.75f));
		}
		return T;
	}

	UTextBlock* Wrap(UTextBlock* Text, float LineHeight)
	{
		Text->SetAutoWrapText(true);
		Text->SetLineHeightPercentage(LineHeight / 1.2f);
		return Text;
	}

	void SetBox(UBorder* InBox, const FLinearColor& Fill, const FLinearColor& Outline, float OutlineWidth, float Radius)
	{
		if (!InBox)
		{
			return;
		}
		if ((Outline.A > 0.f && OutlineWidth > 0.f) || Radius > 0.f)
		{
			InBox->SetBrush(FSlateRoundedBoxBrush(Fill, Radius, Outline, Outline.A > 0.f ? OutlineWidth : 0.f));
		}
		else
		{
			InBox->SetBrush(FSlateColorBrush(Fill));
		}
		InBox->SetBrushColor(FLinearColor::White);
	}

	UBorder* Box(UWidgetTree* Tree, const FLinearColor& Fill, const FMargin& Padding, const FLinearColor& Outline, float OutlineWidth, float Radius)
	{
		UBorder* B = Tree->ConstructWidget<UBorder>();
		SetBox(B, Fill, Outline, OutlineWidth, Radius);
		B->SetPadding(Padding);
		return B;
	}

	USizeBox* Sized(UWidgetTree* Tree, UWidget* Child, float Width, float Height)
	{
		USizeBox* S = Tree->ConstructWidget<USizeBox>();
		if (Width > 0.f) { S->SetWidthOverride(Width); }
		if (Height > 0.f) { S->SetHeightOverride(Height); }
		if (Child) { S->AddChild(Child); }
		return S;
	}

	UWidget* Gap(UWidgetTree* Tree, float Width, float Height)
	{
		USpacer* S = Tree->ConstructWidget<USpacer>();
		S->SetSize(FVector2D(Width, Height));
		return S;
	}

	UTextBlock* Label(UWidgetTree* Tree, const FText& InText, const FLinearColor& Color)
	{
		return Txt(Tree, Upper(InText), Mono(12.f, 400, 0.14f), Color);
	}

	UTextBlock* H1(UWidgetTree* Tree, const FText& InText, float CssPx, const FLinearColor& Color, float LetterEm)
	{
		return Txt(Tree, Upper(InText), Oswald(CssPx, true, LetterEm), Color);
	}

	UWidget* Key(UWidgetTree* Tree, UTextBlock*& OutText, const FLinearColor& TextColor, const FLinearColor& BorderColor, float Height, float FontPx, const FLinearColor& Fill)
	{
		// .key { min-width:28px; height:28px; padding:0 7px; border:1px; border-bottom:3px; border-radius:4px; font: 13px mono }
		UOverlay* Cap = Tree->ConstructWidget<UOverlay>();

		UBorder* Outline = Box(Tree, Fill, FMargin(0.f), BorderColor, 1.f, 4.f);
		UOverlaySlot* OutlineSlot = Cap->AddChildToOverlay(Outline);
		OutlineSlot->SetHorizontalAlignment(HAlign_Fill);
		OutlineSlot->SetVerticalAlignment(VAlign_Fill);

		UBorder* Bottom = Box(Tree, BorderColor, FMargin(0.f), FLinearColor::Transparent, 0.f, 2.f);
		UOverlaySlot* BottomSlot = Cap->AddChildToOverlay(Sized(Tree, Bottom, 0.f, 3.f));
		BottomSlot->SetHorizontalAlignment(HAlign_Fill);
		BottomSlot->SetVerticalAlignment(VAlign_Bottom);

		OutText = Txt(Tree, FText::GetEmpty(), Mono(FontPx), TextColor);
		OutText->SetJustification(ETextJustify::Center);
		UOverlaySlot* TextSlot = Cap->AddChildToOverlay(OutText);
		TextSlot->SetHorizontalAlignment(HAlign_Center);
		TextSlot->SetVerticalAlignment(VAlign_Center);
		TextSlot->SetPadding(FMargin(Height < 28.f ? 6.f : 7.f, 0.f, Height < 28.f ? 6.f : 7.f, 2.f));

		USizeBox* Size = Tree->ConstructWidget<USizeBox>();
		Size->SetMinDesiredWidth(Height);
		Size->SetHeightOverride(Height);
		Size->AddChild(Cap);
		return Size;
	}

	UWidget* Rule(UWidgetTree* Tree, const FLinearColor& Color, float Thickness)
	{
		return Sized(Tree, Box(Tree, Color, FMargin(0.f)), 0.f, Thickness);
	}

	UWidget* Dashed(UWidgetTree* Tree, const FLinearColor& Color, float Thickness)
	{
		// border: 1px dashed -> tracos de 3px com 3px de intervalo, cortados na largura disponivel.
		UHorizontalBox* Row = Tree->ConstructWidget<UHorizontalBox>();
		for (int32 i = 0; i < 160; ++i)
		{
			UHorizontalBoxSlot* DashSlot = Row->AddChildToHorizontalBox(Sized(Tree, Box(Tree, Color, FMargin(0.f)), 3.f, Thickness));
			DashSlot->SetPadding(FMargin(0.f, 0.f, 3.f, 0.f));
		}
		USizeBox* Clip = Sized(Tree, Row, 0.f, Thickness);
		Clip->SetClipping(EWidgetClipping::ClipToBounds);
		return Clip;
	}

	UVerticalBoxSlot* AddV(UVerticalBox* InBox, UWidget* Child, float GapBefore, EHorizontalAlignment HAlign)
	{
		UVerticalBoxSlot* S = InBox->AddChildToVerticalBox(Child);
		S->SetPadding(FMargin(0.f, InBox->GetChildrenCount() > 1 ? GapBefore : 0.f, 0.f, 0.f));
		S->SetHorizontalAlignment(HAlign);
		return S;
	}

	UHorizontalBoxSlot* AddH(UHorizontalBox* InBox, UWidget* Child, float GapBefore, bool bFill, EVerticalAlignment VAlign)
	{
		UHorizontalBoxSlot* S = InBox->AddChildToHorizontalBox(Child);
		S->SetPadding(FMargin(InBox->GetChildrenCount() > 1 ? GapBefore : 0.f, 0.f, 0.f, 0.f));
		S->SetVerticalAlignment(VAlign);
		if (bFill)
		{
			S->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		}
		return S;
	}

	UHorizontalBox* SpaceBetween(UWidgetTree* Tree, UWidget* Left, UWidget* Right, EVerticalAlignment VAlign)
	{
		UHorizontalBox* Row = Tree->ConstructWidget<UHorizontalBox>();
		AddH(Row, Left, 0.f, true, VAlign);
		if (Right)
		{
			AddH(Row, Right, 12.f, false, VAlign);
		}
		return Row;
	}

	UEditableTextBox* Input(UWidgetTree* Tree, const FText& Hint, float CssFontPx)
	{
		UEditableTextBox* Field = Tree->ConstructWidget<UEditableTextBox>();
		FEditableTextBoxStyle Style = Field->GetWidgetStyle();
		Style.SetBackgroundImageNormal(FSlateRoundedBoxBrush(Bg(), 0.f, Line(), 1.f));
		Style.SetBackgroundImageHovered(FSlateRoundedBoxBrush(Bg(), 0.f, Hex(TEXT("5A4F38")), 1.f));
		Style.SetBackgroundImageFocused(FSlateRoundedBoxBrush(Bg(), 0.f, Amber(), 1.f));
		Style.SetBackgroundImageReadOnly(FSlateRoundedBoxBrush(Bg(), 0.f, Line(), 1.f));
		Style.SetPadding(FMargin(14.f, 12.f));
		Style.SetFont(Mono(CssFontPx));
		Style.SetForegroundColor(FSlateColor(Text()));
		Style.SetFocusedForegroundColor(FSlateColor(TextBright()));
		Field->SetWidgetStyle(Style);
		Field->SetHintText(Hint);
		return Field;
	}

	void FBar::SetPercent(float Percent) const
	{
		const float P = FMath::Clamp(Percent, 0.f, 1.f);
		if (FillSlot)
		{
			FSlateChildSize S(ESlateSizeRule::Fill);
			S.Value = P;
			FillSlot->SetSize(S);
		}
		if (RestSlot)
		{
			FSlateChildSize S(ESlateSizeRule::Fill);
			S.Value = 1.f - P;
			RestSlot->SetSize(S);
		}
	}

	void FBar::SetColor(const FLinearColor& Color) const
	{
		SetBox(Fill, Color);
	}

	FBar MakeBar(UWidgetTree* Tree, float Height, const FLinearColor& BackColor, const FLinearColor& FillColor, const FLinearColor& Outline)
	{
		FBar Bar;
		Bar.Back = Box(Tree, BackColor, FMargin(Outline.A > 0.f ? 1.f : 0.f), Outline);
		UHorizontalBox* Row = Tree->ConstructWidget<UHorizontalBox>();
		Bar.Back->SetContent(Row);
		Bar.Fill = Box(Tree, FillColor, FMargin(0.f));
		Bar.FillSlot = Row->AddChildToHorizontalBox(Bar.Fill);
		USpacer* Rest = Tree->ConstructWidget<USpacer>();
		Bar.RestSlot = Row->AddChildToHorizontalBox(Rest);
		Bar.Root = Sized(Tree, Bar.Back, 0.f, Height);
		Bar.SetPercent(0.f);
		return Bar;
	}

	FText FormatClock(float Seconds)
	{
		const int32 Total = FMath::Max(0, FMath::CeilToInt(Seconds));
		return FText::FromString(FString::Printf(TEXT("%02d:%02d"), Total / 60, Total % 60));
	}

	FText FormatMoney(int32 Value)
	{
		FNumberFormattingOptions Opts;
		Opts.UseGrouping = true;
		return FText::Format(NSLOCTEXT("NoclipUI", "MoneyFmt", "${0}"), FText::AsNumber(Value, &Opts));
	}

	FText Upper(const FText& InText)
	{
		return InText.ToUpper();
	}

	UUniformGridSlot* AddCell(UUniformGridPanel* Grid, UWidget* Child, int32 Row, int32 Column)
	{
		UUniformGridSlot* S = Grid->AddChildToUniformGrid(Child, Row, Column);
		S->SetHorizontalAlignment(HAlign_Fill);
		S->SetVerticalAlignment(VAlign_Fill);
		return S;
	}

	FString ShortName(const FString& Name, int32 MaxChars)
	{
		const FString Clean = Name.TrimStartAndEnd();
		if (Clean.IsEmpty())
		{
			return FString(TEXT("Estagiário"));
		}
		MaxChars = FMath::Max(MaxChars, 4);
		if (Clean.Len() <= MaxChars)
		{
			return Clean;
		}
		return Clean.Left(MaxChars - 1).TrimEnd() + TEXT("…");
	}

	FText ShortNameText(const FString& Name, int32 MaxChars)
	{
		return FText::FromString(ShortName(Name, MaxChars));
	}
}
