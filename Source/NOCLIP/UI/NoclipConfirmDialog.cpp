// Copyright Fuly Games. All Rights Reserved.

#include "UI/NoclipConfirmDialog.h"
#include "UI/NoclipButton.h"
#include "UI/NoclipUIStyle.h"
#include "UI/NoclipInputGlyphs.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/Border.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(NoclipConfirmDialog)

#define LOCTEXT_NAMESPACE "NoclipConfirm"

using namespace NoclipUI;

void UNoclipConfirmDialog::BuildTree()
{
	bIsModal = true;

	UOverlay* Root = WidgetTree->ConstructWidget<UOverlay>();
	WidgetTree->RootWidget = Root;

	// background: rgba(8,7,5,.72) por cima da tela anterior
	UBorder* Dim = Box(WidgetTree, Hex(TEXT("080705"), 0.72f), FMargin(0.f));
	UOverlaySlot* DimSlot = Root->AddChildToOverlay(Dim);
	DimSlot->SetHorizontalAlignment(HAlign_Fill);
	DimSlot->SetVerticalAlignment(VAlign_Fill);

	// role=alertdialog: 520px, #D8C38D, padding 32, gap 18
	UBorder* Card = Box(WidgetTree, Manila(), FMargin(32.f));
	UOverlaySlot* CardSlot = Root->AddChildToOverlay(Sized(WidgetTree, Card, 520.f));
	CardSlot->SetHorizontalAlignment(HAlign_Center);
	CardSlot->SetVerticalAlignment(VAlign_Center);

	UVerticalBox* Col = WidgetTree->ConstructWidget<UVerticalBox>();
	Card->SetContent(Col);

	EyebrowText = Txt(WidgetTree, Upper(LOCTEXT("Memo", "Memorando interno")), Mono(12.f, 400, 0.14f), Ink());
	AddV(Col, EyebrowText);
	TitleText = Txt(WidgetTree, FText::GetEmpty(), Oswald(40.f), Ink());
	AddV(Col, TitleText, 18.f);
	BodyText = Wrap(Txt(WidgetTree, FText::GetEmpty(), Mono(15.f), Ink()), 1.55f);
	AddV(Col, BodyText, 18.f);

	UHorizontalBox* Buttons = WidgetTree->ConstructWidget<UHorizontalBox>();
	CancelButton = WidgetTree->ConstructWidget<UNoclipButton>(UNoclipButton::StaticClass());
	CancelButton->Configure(LOCTEXT("Cancel", "Cancelar"), ENoclipButtonKind::Primary, ENoclipButtonStyle::Compact);
	CancelButton->SetFontPx(22.f);
	CancelButton->SetMinHeight(52.f);
	CancelButton->OnClicked().AddUObject(this, &UNoclipConfirmDialog::HandleCancel);
	AddH(Buttons, CancelButton, 0.f, true);

	ConfirmButton = WidgetTree->ConstructWidget<UNoclipButton>(UNoclipButton::StaticClass());
	ConfirmButton->Configure(LOCTEXT("Confirm", "Sair"), ENoclipButtonKind::Danger, ENoclipButtonStyle::Compact);
	ConfirmButton->SetFontPx(22.f);
	ConfirmButton->SetMinHeight(52.f);
	ConfirmButton->OnClicked().AddUObject(this, &UNoclipConfirmDialog::HandleConfirm);
	AddH(Buttons, ConfirmButton, 12.f, true);
	AddV(Col, Buttons, 24.f);

	UHorizontalBox* Hint = WidgetTree->ConstructWidget<UHorizontalBox>();
	AddH(Hint, Key(WidgetTree, BackKey, Ink(), ManilaRule()));
	AddH(Hint, Txt(WidgetTree, LOCTEXT("AlsoCancels", "também cancela"), Mono(13.f), Ink()), 8.f);
	AddV(Col, Hint, 18.f);

	DefaultFocusWidget = CancelButton;
}

void UNoclipConfirmDialog::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	BackKey->SetText(NoclipGlyphs::GetKeyText(ENoclipPrompt::Back, GetOwningPlayer()));
}

void UNoclipConfirmDialog::Setup(const FText& Title, const FText& Body, const FText& ConfirmLabel, TFunction<void()> InOnConfirm, const FText& Eyebrow)
{
	if (!Eyebrow.IsEmpty())
	{
		EyebrowText->SetText(Upper(Eyebrow));
	}
	TitleText->SetText(Upper(Title));
	BodyText->SetText(Body);
	ConfirmButton->SetLabel(ConfirmLabel);
	OnConfirm = MoveTemp(InOnConfirm);
}

void UNoclipConfirmDialog::HandleCancel()
{
	DeactivateWidget();
}

void UNoclipConfirmDialog::HandleConfirm()
{
	TFunction<void()> Callback = MoveTemp(OnConfirm);
	DeactivateWidget();
	if (Callback)
	{
		Callback();
	}
}

#undef LOCTEXT_NAMESPACE
