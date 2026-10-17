// Copyright Fuly Games. All Rights Reserved.

#include "UI/NoclipHUDWidget.h"
#include "Voice/NoclipVoiceSubsystem.h"
#include "Character/NoclipToolTypes.h"
#include "UI/NoclipInputGlyphs.h"
#include "Game/NoclipGameState.h"
#include "Game/NoclipPlayerState.h"
#include "World/NoclipPowerGenerator.h"
#include "World/NoclipLevelGenerator.h"
#include "Game/NoclipPlayerController.h"
#include "Game/NoclipUserSettings.h"
#include "Character/NoclipCharacter.h"
#include "Character/NoclipGrabComponent.h"
#include "Character/NoclipToolComponent.h"
#include "World/NoclipLootActor.h"
#include "World/NoclipExtractionZone.h"
#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/TextBlock.h"
#include "Components/Border.h"
#include "Components/SizeBox.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "GameFramework/PlayerState.h"
#include "EngineUtils.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(NoclipHUDWidget)

#define LOCTEXT_NAMESPACE "NoclipHUD"

using namespace NoclipUI;

namespace
{
	UCanvasPanelSlot* Place(UCanvasPanel* Canvas, UWidget* Widget, const FVector2D& Anchor, const FVector2D& Alignment, const FVector2D& Position)
	{
		UCanvasPanelSlot* S = Canvas->AddChildToCanvas(Widget);
		S->SetAnchors(FAnchors(Anchor.X, Anchor.Y));
		S->SetAlignment(Alignment);
		S->SetPosition(Position);
		S->SetAutoSize(true);
		return S;
	}

	void Show(UWidget* Widget, bool bShow)
	{
		if (Widget)
		{
			const ESlateVisibility Wanted = bShow ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed;
			if (Widget->GetVisibility() != Wanted)
			{
				Widget->SetVisibility(Wanted);
			}
		}
	}

	/** .chip { background: rgba(18,16,12,.72); border: 1px solid rgba(232,185,49,.35); padding: 10px 14px } */
	UBorder* ChipBox(UWidgetTree* Tree, const FMargin& Padding = FMargin(14.f, 10.f))
	{
		return Box(Tree, Chip(), Padding, ChipLine(), 1.f);
	}

	FText PhaseName(ENoclipShiftPhase Phase, bool bExtracting)
	{
		if (bExtracting)
		{
			return LOCTEXT("PhaseExtract", "Extração");
		}
		switch (Phase)
		{
		case ENoclipShiftPhase::Lobby:     return LOCTEXT("PhaseLobby", "Saguão");
		case ENoclipShiftPhase::Preparing: return LOCTEXT("PhasePrep", "Preparando");
		case ENoclipShiftPhase::Active:    return LOCTEXT("PhaseActive", "Expediente");
		case ENoclipShiftPhase::Collapse:  return LOCTEXT("PhaseCollapse", "Colapso");
		case ENoclipShiftPhase::Ended:     return LOCTEXT("PhaseEnded", "Encerrado");
		}
		return FText::GetEmpty();
	}
}

UNoclipHUDWidget::UNoclipHUDWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bIsMenu = false;
	bClosesOnBack = false;
}

UNoclipHUDWidget::FHint UNoclipHUDWidget::MakeHint(const FText& Label, float FontPx)
{
	// .key do HUD: 26px, borda #EDE3C8, fundo rgba(18,16,12,.55), mono 12px
	FHint Hint;
	UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>();
	AddH(Row, Key(WidgetTree, Hint.Key, Text(), Text(), 26.f, 12.f, Hex(TEXT("12100C"), 0.55f)));
	Hint.Label = Txt(WidgetTree, Label, Mono(FontPx), HudText(), true);
	AddH(Row, Hint.Label, 6.f);
	Hint.Root = Row;
	return Hint;
}

void UNoclipHUDWidget::BuildTree()
{
	Canvas = WidgetTree->ConstructWidget<UCanvasPanel>();
	Canvas->SetVisibility(ESlateVisibility::HitTestInvisible);
	WidgetTree->RootWidget = Canvas;

	// Colapso: tinta avermelhada sobre o jogo (tint rgba(20,4,0,.45) do prototipo).
	CollapseTint = Box(WidgetTree, Hex(TEXT("140400"), 0.45f), FMargin(0.f));
	UCanvasPanelSlot* TintSlot = Canvas->AddChildToCanvas(CollapseTint);
	TintSlot->SetAnchors(FAnchors(0.f, 0.f, 1.f, 1.f));
	TintSlot->SetOffsets(FMargin(0.f));
	Show(CollapseTint, false);

	// Microfone aberto (canto inferior direito, acima da lista da equipe).
	{
		UBorder* Chip = ChipBox(WidgetTree);
		MicText = Txt(WidgetTree, FText::GetEmpty(), Mono(12.f, 600), Amber(), true);
		Chip->SetContent(MicText);
		MicChip = Chip;
		Place(Canvas, MicChip, FVector2D(1.f, 1.f), FVector2D(1.f, 1.f), FVector2D(-32.f, -150.f));
		Show(MicChip, false);
	}

	// Dano: flash vermelho na tela inteira (some em ~0,6 s).
	HurtTint = Box(WidgetTree, Hex(TEXT("B3160C"), 1.f), FMargin(0.f));
	UCanvasPanelSlot* HurtSlot = Canvas->AddChildToCanvas(HurtTint);
	HurtSlot->SetAnchors(FAnchors(0.f, 0.f, 1.f, 1.f));
	HurtSlot->SetOffsets(FMargin(0.f));
	Show(HurtTint, false);

	// ---------- Topo esquerdo (32, 28): fase + relogio ----------
	{
		UVerticalBox* Col = WidgetTree->ConstructWidget<UVerticalBox>();
		PhaseText = Txt(WidgetTree, FText::GetEmpty(), Mono(13.f, 400, 0.14f), HudText(), true);
		AddV(Col, PhaseText);
		ClockText = Txt(WidgetTree, FText::FromString(TEXT("--:--")), Oswald(64.f, false), HudText(), true);
		ClockText->SetLineHeightPercentage(1.f / 1.2f);
		AddV(Col, ClockText, 2.f);

		UBorder* Banner = Box(WidgetTree, DangerDeep(), FMargin(10.f, 6.f));
		UHorizontalBox* BannerRow = WidgetTree->ConstructWidget<UHorizontalBox>();
		AddH(BannerRow, Txt(WidgetTree, FText::FromString(TEXT("⚠")), Mono(14.f, 600), Hex(TEXT("FFE7E1"))));
		AddH(BannerRow, Txt(WidgetTree, LOCTEXT("Collapse", "O nível está desligando — vá para a Sala Manila"), Mono(14.f, 600), Hex(TEXT("FFE7E1"))), 8.f);
		Banner->SetContent(BannerRow);
		AddV(Col, Banner, 8.f, HAlign_Left);
		CollapseBanner = Banner;
		Show(CollapseBanner, false);

		Place(Canvas, Col, FVector2D(0.f, 0.f), FVector2D(0.f, 0.f), FVector2D(32.f, 28.f));
	}

	// ---------- Topo direito (right 32, top 28, 300 de largura): quota ----------
	{
		UBorder* Chip = ChipBox(WidgetTree);
		UVerticalBox* Col = WidgetTree->ConstructWidget<UVerticalBox>();
		Chip->SetContent(Col);
		AddV(Col, SpaceBetween(WidgetTree,
			Txt(WidgetTree, Upper(LOCTEXT("InRoomLabel", "Na Sala Manila")), Mono(13.f, 400, 0.1f), HudText(), true),
			Txt(WidgetTree, Upper(LOCTEXT("QuotaLabel", "Quota")), Mono(13.f, 400, 0.1f), HudText(), true)));
		InRoomText = Txt(WidgetTree, FormatMoney(0), Oswald(34.f, false), Amber(), true);
		QuotaText = Txt(WidgetTree, FText::GetEmpty(), Oswald(22.f, false), HudText(), true);
		AddV(Col, SpaceBetween(WidgetTree, InRoomText, QuotaText, VAlign_Bottom), 8.f);
		QuotaBar = MakeBar(WidgetTree, 10.f, FLinearColor(1.f, 1.f, 1.f, 0.12f), Amber());
		AddV(Col, QuotaBar.Root, 8.f);
		QuotaHintText = Txt(WidgetTree, FText::GetEmpty(), Mono(12.f), Soft(), true);
		AddV(Col, QuotaHintText, 8.f);
		QuotaChip = Sized(WidgetTree, Chip, 300.f);
		Place(Canvas, QuotaChip, FVector2D(1.f, 0.f), FVector2D(1.f, 0.f), FVector2D(-32.f, 28.f));
	}

	// ---------- Contagem (preparando) ----------
	{
		UVerticalBox* Col = WidgetTree->ConstructWidget<UVerticalBox>();
		CountdownTitle = Txt(WidgetTree, Upper(LOCTEXT("ShiftStartsIn", "O expediente começa em")), Mono(13.f, 400, 0.14f), HudText(), true);
		AddV(Col, CountdownTitle, 0.f, HAlign_Center);
		CountdownText = Txt(WidgetTree, FText::GetEmpty(), Oswald(120.f), Amber(), true);
		AddV(Col, CountdownText, 0.f, HAlign_Center);
		CountdownInfo = Txt(WidgetTree, FText::GetEmpty(), Mono(15.f), HudText(), true);
		AddV(Col, CountdownInfo, 4.f, HAlign_Center);
		CountdownPanel = Col;
		Place(Canvas, CountdownPanel, FVector2D(0.5f, 0.32f), FVector2D(0.5f, 0.5f), FVector2D::ZeroVector);
		Show(CountdownPanel, false);
	}

	// ---------- Centro: mira 4px + dica de agarrar (top 386) ----------
	{
		UBorder* Dot = Box(WidgetTree, HudText(), FMargin(0.f), FLinearColor(0.f, 0.f, 0.f, 0.6f), 1.f);
		Crosshair = Sized(WidgetTree, Dot, 4.f, 4.f);
		Place(Canvas, Crosshair, FVector2D(0.5f, 0.5f), FVector2D(0.5f, 0.5f), FVector2D::ZeroVector);

		UVerticalBox* Col = WidgetTree->ConstructWidget<UVerticalBox>();
		GrabHint = MakeHint(LOCTEXT("Grab", "Agarrar"), 15.f);
		GrabHint.Label->SetFont(Mono(15.f));
		AddV(Col, GrabHint.Root, 0.f, HAlign_Center);
		PromptDetail = Txt(WidgetTree, FText::GetEmpty(), Mono(13.f), Text(), true);
		PromptDetail->SetJustification(ETextJustify::Center);
		AddV(Col, PromptDetail, 4.f, HAlign_Center);
		PromptPanel = Col;
		Place(Canvas, PromptPanel, FVector2D(0.5f, 0.5f), FVector2D(0.5f, 0.f), FVector2D(0.f, 26.f));
		Show(PromptPanel, false);

		ThrowBar = MakeBar(WidgetTree, 4.f, Hex(TEXT("12100C"), 0.7f), Amber());
		Place(Canvas, Sized(WidgetTree, ThrowBar.Root, 120.f), FVector2D(0.5f, 0.5f), FVector2D(0.5f, 0.f), FVector2D(0.f, 12.f));
		Show(ThrowBar.Root, false);
	}

	// ---------- Elevador (left 440, top 520, 400 de largura) ----------
	{
		UVerticalBox* Col = WidgetTree->ConstructWidget<UVerticalBox>();
		ElevatorText = Txt(WidgetTree, FText::GetEmpty(), Mono(14.f, 600), HudText(), true);
		AddV(Col, ElevatorText, 0.f, HAlign_Center);
		ElevatorBar = MakeBar(WidgetTree, 12.f, Hex(TEXT("12100C"), 0.7f), Amber(), Amber());
		AddV(Col, ElevatorBar.Root, 6.f);
		AddV(Col, Txt(WidgetTree, LOCTEXT("ElevatorHint", "Quem sair da sala interrompe a subida"), Mono(12.f), Soft(), true), 6.f, HAlign_Center);
		ElevatorPanel = Sized(WidgetTree, Col, 400.f);
		Place(Canvas, ElevatorPanel, FVector2D(0.5f, 1.f), FVector2D(0.5f, 0.f), FVector2D(0.f, -200.f));
		Show(ElevatorPanel, false);
	}

	// ---------- Item segurado + folego (left 32, bottom 32, 360 de largura) ----------
	{
		UBorder* Chip = ChipBox(WidgetTree);
		UVerticalBox* Col = WidgetTree->ConstructWidget<UVerticalBox>();
		Chip->SetContent(Col);

		UHorizontalBox* Top = WidgetTree->ConstructWidget<UHorizontalBox>();
		HeldName = Txt(WidgetTree, FText::GetEmpty(), Mono(16.f, 600), HudText(), true);
		AddH(Top, HeldName, 0.f, true, VAlign_Bottom);
		HeldValue = Txt(WidgetTree, FText::GetEmpty(), Oswald(24.f, false), HudText(), true);
		AddH(Top, HeldValue, 8.f, false, VAlign_Bottom);
		HeldLost = Txt(WidgetTree, FText::GetEmpty(), Oswald(14.f, false), DangerText(), true);
		AddH(Top, HeldLost, 4.f, false, VAlign_Bottom);
		HeldTop = Top;
		AddV(Col, Top);

		HeldDesc = Txt(WidgetTree, FText::GetEmpty(), Mono(12.f), Soft(), true);
		HeldDesc->SetAutoWrapText(true);
		AddV(Col, HeldDesc, 2.f);

		UHorizontalBox* Hints = WidgetTree->ConstructWidget<UHorizontalBox>();
		DropHint = MakeHint(LOCTEXT("Drop", "soltar"), 13.f);
		AddH(Hints, DropHint.Root);
		ThrowHint = MakeHint(LOCTEXT("Throw", "segurar e soltar para arremessar"), 13.f);
		AddH(Hints, ThrowHint.Root, 14.f);
		HeldHints = Hints;
		AddV(Col, Hints, 8.f);

		UHorizontalBox* Breath = WidgetTree->ConstructWidget<UHorizontalBox>();
		AddH(Breath, Txt(WidgetTree, LOCTEXT("Stamina", "Fôlego"), Mono(12.f), Soft(), true));
		StaminaBar = MakeBar(WidgetTree, 6.f, FLinearColor(1.f, 1.f, 1.f, 0.12f), Text());
		AddH(Breath, StaminaBar.Root, 8.f, true);
		AddV(Col, Breath, 8.f);

		UHorizontalBox* Life = WidgetTree->ConstructWidget<UHorizontalBox>();
		AddH(Life, Sized(WidgetTree, Txt(WidgetTree, LOCTEXT("Health", "Saúde"), Mono(12.f), Soft(), true), 48.f));
		HealthBar = MakeBar(WidgetTree, 6.f, FLinearColor(1.f, 1.f, 1.f, 0.12f), OkText());
		AddH(Life, HealthBar.Root, 0.f, true);
		HealthRow = Life;
		AddV(Col, Life, 6.f);

		HeldChip = Sized(WidgetTree, Chip, 360.f);
		Place(Canvas, HeldChip, FVector2D(0.f, 1.f), FVector2D(0.f, 1.f), FVector2D(32.f, -32.f));
		Show(HeldChip, false);
	}

	// ---------- Equipe (right 32, bottom 32) ----------
	{
		UVerticalBox* Col = WidgetTree->ConstructWidget<UVerticalBox>();
		for (int32 i = 0; i < 6; ++i)
		{
			FCrewRow Row;
			UBorder* RowChip = ChipBox(WidgetTree, FMargin(12.f, 6.f));
			UHorizontalBox* H = WidgetTree->ConstructWidget<UHorizontalBox>();
			RowChip->SetContent(H);
			Row.Dot = Box(WidgetTree, Line(), FMargin(0.f), Soft(), 1.f);
			AddH(H, Sized(WidgetTree, Row.Dot, 10.f, 10.f));
			Row.Name = Txt(WidgetTree, FText::GetEmpty(), Mono(14.f), HudText(), true);
			AddH(H, Row.Name, 10.f);
			Row.State = Txt(WidgetTree, FText::GetEmpty(), Mono(12.f), Soft(), true);
			AddH(H, Row.State, 10.f);
			AddV(Col, RowChip, 6.f, HAlign_Right);
			Row.Root = RowChip;
			Show(Row.Root, false);
			CrewRows.Add(Row);
		}
		Place(Canvas, Col, FVector2D(1.f, 1.f), FVector2D(1.f, 1.f), FVector2D(-32.f, -32.f));
	}


	// ---------- Cinto de ferramentas (centro, bottom 44): 1 lanterna / 2 kit medico / 3 celular ----------
	{
		UVerticalBox* Col = WidgetTree->ConstructWidget<UVerticalBox>();
		UHorizontalBox* Hints = WidgetTree->ConstructWidget<UHorizontalBox>();
		UseHint = MakeHint(FText::GetEmpty(), 13.f);
		AddH(Hints, UseHint.Root);
		ToolDropHint = MakeHint(LOCTEXT("ToolDrop", "soltar"), 13.f);
		AddH(Hints, ToolDropHint.Root, 14.f);
		ToolHintRow = Hints;
		AddV(Col, Hints, 0.f, HAlign_Center);

		UHorizontalBox* Belt = WidgetTree->ConstructWidget<UHorizontalBox>();
		for (int32 i = 0; i < 3; ++i)
		{
			FToolSlot S;
			S.Box = ChipBox(WidgetTree, FMargin(10.f, 6.f));
			UVerticalBox* In = WidgetTree->ConstructWidget<UVerticalBox>();
			S.Box->SetContent(In);
			UHorizontalBox* Top = WidgetTree->ConstructWidget<UHorizontalBox>();
			AddH(Top, Key(WidgetTree, S.Key, Text(), Text(), 22.f, 11.f, Hex(TEXT("12100C"), 0.55f)));
			S.Name = Txt(WidgetTree, NoclipTools::Get(NoclipTools::FromSlot(i + 1)).Short, Mono(12.f, 600), HudText(), true);
			S.Name->SetTextOverflowPolicy(ETextOverflowPolicy::Ellipsis);
			AddH(Top, S.Name, 6.f, true);
			AddV(In, Top);
			UHorizontalBox* Bottom = WidgetTree->ConstructWidget<UHorizontalBox>();
			S.Battery = MakeBar(WidgetTree, 4.f, FLinearColor(1.f, 1.f, 1.f, 0.12f), Amber());
			AddH(Bottom, S.Battery.Root, 0.f, true);
			S.Extra = Txt(WidgetTree, FText::GetEmpty(), Mono(11.f), Soft(), true);
			AddH(Bottom, S.Extra, 6.f);
			AddV(In, Sized(WidgetTree, Bottom, 0.f, 14.f), 4.f);
			AddH(Belt, Sized(WidgetTree, S.Box, 118.f), i == 0 ? 0.f : 8.f);
			ToolSlots.Add(S);
		}
		AddV(Col, Belt, 8.f, HAlign_Center);
		ToolBelt = Col;
		Place(Canvas, ToolBelt, FVector2D(0.5f, 1.f), FVector2D(0.5f, 1.f), FVector2D(0.f, -44.f));
		Show(ToolBelt, false);
	}

	// ---------- Curativo em andamento (abaixo da mira) ----------
	{
		UVerticalBox* Col = WidgetTree->ConstructWidget<UVerticalBox>();
		UseText = Txt(WidgetTree, LOCTEXT("Healing", "Aplicando curativo…"), Mono(13.f), HudText(), true);
		AddV(Col, UseText, 0.f, HAlign_Center);
		UseBar = MakeBar(WidgetTree, 5.f, Hex(TEXT("12100C"), 0.7f), OkText());
		AddV(Col, Sized(WidgetTree, UseBar.Root, 160.f), 6.f, HAlign_Center);
		UsePanel = Col;
		Place(Canvas, UsePanel, FVector2D(0.5f, 0.5f), FVector2D(0.5f, 0.f), FVector2D(0.f, 70.f));
		Show(UsePanel, false);
	}

	// ---------- Aviso: maos ocupadas / nao pode ----------
	{
		UBorder* Chip = Box(WidgetTree, Hex(TEXT("12100C"), 0.82f), FMargin(16.f, 10.f), Danger(), 1.f);
		UVerticalBox* Col = WidgetTree->ConstructWidget<UVerticalBox>();
		Chip->SetContent(Col);
		DenyTitle = Txt(WidgetTree, FText::GetEmpty(), Oswald(16.f, true, 0.06f), DangerText(), true);
		AddV(Col, DenyTitle, 0.f, HAlign_Center);
		DenyDetail = Txt(WidgetTree, FText::GetEmpty(), Mono(13.f), HudText(), true);
		DenyDetail->SetJustification(ETextJustify::Center);
		AddV(Col, DenyDetail, 4.f, HAlign_Center);
		DenyPanel = Chip;
		Place(Canvas, DenyPanel, FVector2D(0.5f, 0.5f), FVector2D(0.5f, 0.f), FVector2D(0.f, 108.f));
		Show(DenyPanel, false);
	}

	// ---------- Tela do celular (celular levantado) ----------
	{
		const FLinearColor Lcd = Hex(TEXT("C9CF5A"));
		const FLinearColor LcdInk = Hex(TEXT("1E2410"));
		UBorder* Body = Box(WidgetTree, Hex(TEXT("17181B")), FMargin(14.f, 18.f, 14.f, 22.f), Hex(TEXT("3A3B40")), 2.f, 22.f);
		UVerticalBox* BodyCol = WidgetTree->ConstructWidget<UVerticalBox>();
		Body->SetContent(BodyCol);
		AddV(BodyCol, Sized(WidgetTree, Box(WidgetTree, Hex(TEXT("2A2B30")), FMargin(0.f), FLinearColor::Transparent, 0.f, 2.f), 40.f, 4.f), 0.f, HAlign_Center);

		UBorder* Screen = Box(WidgetTree, Lcd, FMargin(12.f, 10.f), Hex(TEXT("0E0F10")), 2.f, 3.f);
		UVerticalBox* Col = WidgetTree->ConstructWidget<UVerticalBox>();
		Screen->SetContent(Col);
		PhoneSignal = Txt(WidgetTree, FText::GetEmpty(), Mono(11.f, 600), LcdInk);
		AddV(Col, SpaceBetween(WidgetTree, Txt(WidgetTree, FText::FromString(TEXT("BASELINE MÓVEL")), Mono(11.f, 600), LcdInk), PhoneSignal));
		AddV(Col, Dashed(WidgetTree, LcdInk), 6.f);
		PhoneShift = Txt(WidgetTree, FText::GetEmpty(), Mono(11.f), LcdInk);
		AddV(Col, PhoneShift, 8.f, HAlign_Center);
		PhoneClock = Txt(WidgetTree, FText::GetEmpty(), Oswald(40.f, true, 0.04f), LcdInk);
		AddV(Col, PhoneClock, 0.f, HAlign_Center);
		PhoneTarget = Txt(WidgetTree, Upper(LOCTEXT("PhoneManila", "Sala Manila")), Mono(11.f, 600, 0.12f), LcdInk);
		AddV(Col, PhoneTarget, 10.f, HAlign_Center);
		PhoneArrow = Txt(WidgetTree, FText::FromString(TEXT("▲")), Oswald(34.f, true), LcdInk);
		PhoneArrow->SetRenderTransformPivot(FVector2D(0.5f, 0.55f));
		AddV(Col, PhoneArrow, 2.f, HAlign_Center);
		PhoneDistance = Txt(WidgetTree, FText::GetEmpty(), Mono(13.f, 600), LcdInk);
		AddV(Col, PhoneDistance, 2.f, HAlign_Center);
		AddV(Col, Dashed(WidgetTree, LcdInk), 8.f);
		PhoneQuota = Wrap(Txt(WidgetTree, FText::GetEmpty(), Mono(11.f), LcdInk), 1.3f);
		AddV(Col, PhoneQuota, 6.f);
		AddV(BodyCol, Sized(WidgetTree, Screen, 200.f, 236.f), 12.f, HAlign_Center);

		PhoneFooter = Txt(WidgetTree, FText::GetEmpty(), Mono(11.f), Hex(TEXT("8C8E94")));
		AddV(BodyCol, PhoneFooter, 12.f, HAlign_Center);
		PhonePanel = Sized(WidgetTree, Body, 232.f);
		Place(Canvas, PhonePanel, FVector2D(0.6f, 1.f), FVector2D(0.5f, 1.f), FVector2D(0.f, -18.f));
		Show(PhonePanel, false);
	}

	// ---------- Noclip: imagem falhando (tela inteira, por baixo do resto) ----------
	GlitchTint = Box(WidgetTree, Hex(TEXT("B9F5C8"), 1.f), FMargin(0.f));
	{
		UCanvasPanelSlot* GlitchSlot = Canvas->AddChildToCanvas(GlitchTint);
		GlitchSlot->SetAnchors(FAnchors(0.f, 0.f, 1.f, 1.f));
		GlitchSlot->SetOffsets(FMargin(0.f));
		Show(GlitchTint, false);
	}
	FloorNotice = Txt(WidgetTree, FText::GetEmpty(), Mono(15.f, 600, 0.08f), Hex(TEXT("B9F5C8")), true);
	FloorNotice->SetJustification(ETextJustify::Center);
	Place(Canvas, FloorNotice, FVector2D(0.5f, 0.24f), FVector2D(0.5f, 0.5f), FVector2D::ZeroVector);
	Show(FloorNotice, false);

	// ---------- Apagao: aviso no topo ----------
	{
		UBorder* Banner = Box(WidgetTree, Hex(TEXT("12100C"), 0.85f), FMargin(16.f, 10.f), Danger(), 1.f);
		UVerticalBox* Col = WidgetTree->ConstructWidget<UVerticalBox>();
		Banner->SetContent(Col);
		PowerTitle = Txt(WidgetTree, FText::GetEmpty(), Oswald(18.f, true, 0.06f), DangerText(), true);
		AddV(Col, PowerTitle, 0.f, HAlign_Center);
		PowerDetail = Txt(WidgetTree, FText::GetEmpty(), Mono(13.f), HudText(), true);
		AddV(Col, PowerDetail, 4.f, HAlign_Center);
		PowerBanner = Banner;
		Place(Canvas, PowerBanner, FVector2D(0.5f, 0.f), FVector2D(0.5f, 0.f), FVector2D(0.f, 28.f));
		Show(PowerBanner, false);
	}

	// ---------- QTE do gerador (abaixo da mira) ----------
	{
		constexpr float TrackW = 320.f;
		UVerticalBox* Col = WidgetTree->ConstructWidget<UVerticalBox>();
		QTEText = Txt(WidgetTree, FText::GetEmpty(), Mono(14.f, 600), HudText(), true);
		AddV(Col, QTEText, 0.f, HAlign_Center);
		QTEHint = Txt(WidgetTree, FText::GetEmpty(), Mono(12.f), Soft(), true);
		AddV(Col, QTEHint, 34.f, HAlign_Center); // a barra fica no meio (posicionada no canvas)
		QTEPanel = Col;
		Place(Canvas, QTEPanel, FVector2D(0.5f, 0.5f), FVector2D(0.5f, 0.f), FVector2D(0.f, 96.f));
		Show(QTEPanel, false);

		UBorder* Track = Box(WidgetTree, Hex(TEXT("12100C"), 0.8f), FMargin(0.f), HudText(), 1.f);
		QTETrack = Sized(WidgetTree, Track, TrackW, 16.f);
		Place(Canvas, QTETrack, FVector2D(0.5f, 0.5f), FVector2D(0.5f, 0.5f), FVector2D(0.f, 134.f));
		QTEZoneBox = Box(WidgetTree, Ok(), FMargin(0.f));
		QTEZone = Sized(WidgetTree, QTEZoneBox, 60.f, 12.f);
		Place(Canvas, QTEZone, FVector2D(0.5f, 0.5f), FVector2D(0.5f, 0.5f), FVector2D(0.f, 134.f));
		QTENeedle = Sized(WidgetTree, Box(WidgetTree, Amber(), FMargin(0.f)), 4.f, 26.f);
		Place(Canvas, QTENeedle, FVector2D(0.5f, 0.5f), FVector2D(0.5f, 0.5f), FVector2D(0.f, 134.f));
		Show(QTETrack, false);
		Show(QTEZone, false);
		Show(QTENeedle, false);
	}

	// ---------- Morte: quem morreu assiste aos colegas ----------
	{
		UVerticalBox* Col = WidgetTree->ConstructWidget<UVerticalBox>();
		AddV(Col, Txt(WidgetTree, Upper(LOCTEXT("DeadTitle", "Você morreu")), Oswald(64.f, true, 0.04f), DangerText(), true), 0.f, HAlign_Center);
		DeathCause = Txt(WidgetTree, FText::GetEmpty(), Mono(15.f), HudText(), true);
		AddV(Col, DeathCause, 2.f, HAlign_Center);
		UBorder* Chip = ChipBox(WidgetTree, FMargin(14.f, 8.f));
		UVerticalBox* In = WidgetTree->ConstructWidget<UVerticalBox>();
		Chip->SetContent(In);
		DeathWatching = Txt(WidgetTree, FText::GetEmpty(), Mono(14.f, 600), Amber(), true);
		AddV(In, DeathWatching, 0.f, HAlign_Center);
		DeathHint = Txt(WidgetTree, FText::GetEmpty(), Mono(12.f), Soft(), true);
		AddV(In, DeathHint, 4.f, HAlign_Center);
		AddV(Col, Chip, 14.f, HAlign_Center);
		DeathNote = Txt(WidgetTree, FText::GetEmpty(), Mono(12.f), Soft(), true);
		DeathNote->SetJustification(ETextJustify::Center);
		AddV(Col, DeathNote, 10.f, HAlign_Center);
		DeathPanel = Col;
		Place(Canvas, DeathPanel, FVector2D(0.5f, 0.f), FVector2D(0.5f, 0.f), FVector2D(0.f, 120.f));
		Show(DeathPanel, false);
	}

	// ---------- Dica de pausa (.devlink, bottom 8) ----------
	{
		UBorder* Link = Box(WidgetTree, Hex(TEXT("12100C"), 0.8f), FMargin(8.f, 6.f));
		PauseHint = Txt(WidgetTree, FText::GetEmpty(), Mono(12.f), Soft());
		Link->SetContent(PauseHint);
		Place(Canvas, Link, FVector2D(0.5f, 1.f), FVector2D(0.5f, 1.f), FVector2D(0.f, -8.f));
	}
}

void UNoclipHUDWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	RefreshAccumulator += InDeltaTime;
	if (RefreshAccumulator >= 1.f / 30.f)
	{
		RefreshAccumulator = 0.f;
		Refresh();
	}
}

ANoclipExtractionZone* UNoclipHUDWidget::GetZone()
{
	if (!CachedZone.IsValid() && GetWorld())
	{
		for (TActorIterator<ANoclipExtractionZone> It(GetWorld()); It; ++It)
		{
			CachedZone = *It;
			break;
		}
	}
	return CachedZone.Get();
}

void UNoclipHUDWidget::Refresh()
{
	const ANoclipGameState* GS = GetNoclipGameState();
	const ANoclipCharacter* Pawn = Cast<ANoclipCharacter>(GetOwningPlayerPawn());
	const APlayerController* PC = GetOwningPlayer();

	PauseHint->SetText(FText::Format(LOCTEXT("PauseHint", "{0} · pausa"), NoclipGlyphs::GetKeyText(ENoclipPrompt::Pause, PC)));

	if (!GS)
	{
		return;
	}
	RefreshTimer(GS);
	RefreshQuota(GS);
	RefreshCenter(GS, Pawn);
	RefreshHeld(Pawn);
	RefreshExtraction(GS);
	RefreshCrew(GS);
	RefreshTools(GS, Pawn);
	RefreshDeny(Pawn);
	RefreshPhone(GS, Pawn);
	RefreshHurt(Pawn);
	RefreshMic();
	RefreshDeath(GS);
	RefreshPower(GS, Pawn);
	RefreshNoclip(Pawn);
}

void UNoclipHUDWidget::RefreshPower(const ANoclipGameState* GS, const ANoclipCharacter* Pawn)
{
	const bool bInShift = GS->GetPhase() == ENoclipShiftPhase::Active || GS->GetPhase() == ENoclipShiftPhase::Collapse;
	const bool bOut = bInShift && GS->IsPowerOut();
	const ANoclipPowerGenerator* Gen = nullptr;
	if (bOut)
	{
		for (TActorIterator<ANoclipPowerGenerator> It(GetWorld()); It; ++It)
		{
			Gen = *It;
			break;
		}
	}
	Show(PowerBanner, bOut);
	if (bOut)
	{
		const int32 Pct = Gen ? FMath::RoundToInt(Gen->GetProgress() * 100.f) : 0;
		PowerTitle->SetText(Upper(FText::Format(LOCTEXT("PowerOut", "⚡ Queda de energia · gerador {0}%"), FText::AsNumber(Pct))));
		const int32 Workers = Gen ? Gen->GetWorkers() : 0;
		PowerDetail->SetText(Workers > 0
			? FText::Format(LOCTEXT("PowerWorkers", "{0} no gerador · quanto mais gente, mais rápido"), FText::AsNumber(Workers))
			: LOCTEXT("PowerHint", "Religuem o gerador · o celular mostra onde ele está"));
	}

	// QTE: perto do gerador, de maos vazias.
	const ANoclipPowerGenerator* InReach = (bOut && Pawn) ? Pawn->GetGeneratorInReach() : nullptr;
	const bool bHolding = Pawn && Pawn->GetGrabComponent() && Pawn->GetGrabComponent()->IsHolding();
	const bool bQTE = InReach && Pawn->IsGeneratorQTEActive();
	Show(QTEPanel, InReach != nullptr);
	Show(QTETrack, bQTE);
	Show(QTEZone, bQTE);
	Show(QTENeedle, bQTE);
	if (!InReach)
	{
		return;
	}
	const FText Key = NoclipGlyphs::GetKeyText(ENoclipPrompt::Grab, GetOwningPlayer());
	if (!bQTE)
	{
		QTEText->SetText(bHolding ? LOCTEXT("QTEDropFirst", "Solte o que está carregando para mexer no gerador")
			: FText::Format(LOCTEXT("QTEStart", "[{0}] Religar o gerador"), Key));
		QTEHint->SetText(LOCTEXT("QTEStartHint", "Acerte o ponteiro na faixa verde · errar faz faísca (e barulho)"));
		return;
	}
	const float ResultAge = Pawn->GetQTEResultAge();
	const bool bFlash = ResultAge < 0.35f;
	QTEText->SetText(bFlash
		? (Pawn->GetQTELastHit() ? LOCTEXT("QTEHit", "Encaixou!") : LOCTEXT("QTEMiss", "Faísca! Cuidado com o barulho"))
		: FText::Format(LOCTEXT("QTEGo", "[{0}] no verde · {1}%"), Key, FText::AsNumber(FMath::RoundToInt(InReach->GetProgress() * 100.f))));
	QTEText->SetColorAndOpacity(FSlateColor(bFlash ? (Pawn->GetQTELastHit() ? Ok() : DangerText()) : HudText()));
	QTEHint->SetText(LOCTEXT("QTEHintActive", "Afaste-se para parar"));

	constexpr float TrackW = 320.f;
	const float ZoneW = FMath::Max(8.f, Pawn->GetQTEZoneHalf() * 2.f * TrackW);
	QTEZone->SetWidthOverride(ZoneW);
	if (UCanvasPanelSlot* ZoneSlot = Cast<UCanvasPanelSlot>(QTEZone->Slot))
	{
		ZoneSlot->SetPosition(FVector2D((Pawn->GetQTEZoneCenter() - 0.5f) * TrackW, 134.f));
	}
	if (UCanvasPanelSlot* NeedleSlot = Cast<UCanvasPanelSlot>(QTENeedle->Slot))
	{
		NeedleSlot->SetPosition(FVector2D((Pawn->GetQTENeedle() - 0.5f) * TrackW, 134.f));
	}
	const bool bInZone = FMath::Abs(Pawn->GetQTENeedle() - Pawn->GetQTEZoneCenter()) <= Pawn->GetQTEZoneHalf();
	SetBox(QTEZoneBox, bInZone ? Ok() : Hex(TEXT("3E7A4A")));
}

void UNoclipHUDWidget::RefreshNoclip(const ANoclipCharacter* Pawn)
{
	const float Age = Pawn ? Pawn->GetNoclipAge() : 100.f;
	// Imagem falhando: piscadas rapidas e cada vez mais fracas.
	const bool bGlitch = Age >= 0.f && Age < 0.9f;
	Show(GlitchTint, bGlitch);
	if (bGlitch)
	{
		const float Fade = 1.f - Age / 0.9f;
		const float Flicker = FMath::Frac(Age * 23.f) < 0.5f ? 1.f : 0.35f;
		GlitchTint->SetRenderOpacity(0.55f * Fade * Flicker);
	}
	const bool bNotice = Pawn && Age >= 0.f && Age < 5.f;
	Show(FloorNotice, bNotice);
	if (bNotice)
	{
		const ENoclipGlitchKind Kind = static_cast<ENoclipGlitchKind>(Pawn->GetLastNoclipKind());
		const bool bDown = Kind == ENoclipGlitchKind::FakeWall || Kind == ENoclipGlitchKind::FakeFloor;
		FloorNotice->SetText(bDown
			? LOCTEXT("NoticeDown", "Você atravessou… SUBSOLO\nProcure uma SAÍDA DE SERVIÇO — o celular aponta")
			: (Kind == ENoclipGlitchKind::ServiceExit ? LOCTEXT("NoticeExit", "De volta ao Level 0 · a Sala Manila é logo ali")
				: LOCTEXT("NoticeUp", "De volta ao Level 0… mas onde?")));
		FloorNotice->SetRenderOpacity(FMath::Clamp((5.f - Age) / 1.f, 0.f, 1.f));
	}
}

void UNoclipHUDWidget::RefreshDeath(const ANoclipGameState* GS)
{
	const ANoclipPlayerController* PC = Cast<ANoclipPlayerController>(GetOwningPlayer());
	const bool bDead = PC && PC->IsSpectating();
	Show(DeathPanel, bDead);
	Show(Crosshair, !bDead);
	if (!bDead)
	{
		return;
	}
	const ANoclipPlayerState* PS = PC->GetPlayerState<ANoclipPlayerState>();
	switch (PS ? PS->GetDeathCause() : ENoclipDeathCause::Other)
	{
	case ENoclipDeathCause::Hound:    DeathCause->SetText(LOCTEXT("DeathHound", "Um cão te alcançou.")); break;
	case ENoclipDeathCause::Shambler: DeathCause->SetText(LOCTEXT("DeathShambler", "O paciente te pegou.")); break;
	case ENoclipDeathCause::Fall:     DeathCause->SetText(LOCTEXT("DeathFall", "A queda foi alta demais.")); break;
	default:                          DeathCause->SetText(LOCTEXT("DeathOther", "Os corredores ficaram com você.")); break;
	}

	const ANoclipCharacter* Target = PC->GetSpectateTarget();
	const APlayerState* TargetPS = Target ? Target->GetPlayerState() : nullptr;
	if (TargetPS)
	{
		DeathWatching->SetText(FText::Format(LOCTEXT("Watching", "Assistindo: {0}"), FText::FromString(ShortName(TargetPS->GetPlayerName(), 18))));
		DeathHint->SetText(LOCTEXT("WatchingHint", "Clique / A · D · ◀ ▶ trocar de colega · mouse gira a câmera"));
		const int32 Quota = FMath::Max(1, GS->GetQuota());
		const int32 InRoom = GS->GetValueInExtraction();
		DeathNote->SetText(InRoom >= Quota
			? LOCTEXT("DeathNoteMet", "A quota já está na Sala Manila. Torça para eles subirem.\nSó quem também morreu ouve a sua voz.")
			: FText::Format(LOCTEXT("DeathNoteMissing", "Faltam {0} na Sala Manila para a equipe bater a quota.\nSó quem também morreu ouve a sua voz."), FormatMoney(Quota - InRoom)));
	}
	else
	{
		DeathWatching->SetText(LOCTEXT("NobodyLeft", "Ninguém de pé"));
		DeathHint->SetText(LOCTEXT("NobodyLeftHint", "O expediente acaba em instantes."));
		DeathNote->SetText(LOCTEXT("NobodyLeftNote", "A Baseline vai recolher só o que já estava na Sala Manila."));
	}
}

void UNoclipHUDWidget::RefreshMic()
{
	const UNoclipVoiceSubsystem* Voice = UNoclipVoiceSubsystem::Get(this);
	if (!Voice || !MicChip)
	{
		return;
	}
	const bool bTalking = Voice->IsLocalPlayerTalking();
	const bool bPtt = Voice->IsPushToTalk();
	// Apertar para falar: o chip aparece enquanto segura a tecla. Deteccao de voz: so quando a voz passa do limiar.
	const bool bShow = bTalking || (bPtt && Voice->IsPushToTalkHeld());
	Show(MicChip, bShow);
	if (bShow)
	{
		MicText->SetText(bTalking ? LOCTEXT("MicOn", "● microfone") : LOCTEXT("MicOpen", "○ microfone"));
		MicText->SetColorAndOpacity(FSlateColor(bTalking ? Amber() : Soft()));
	}
}

void UNoclipHUDWidget::RefreshHurt(const ANoclipCharacter* Pawn)
{
	const float Age = Pawn ? Pawn->GetHurtAge() : 100.f;
	constexpr float FlashSeconds = 0.6f;
	const bool bShow = Age >= 0.f && Age < FlashSeconds;
	Show(HurtTint, bShow);
	if (bShow)
	{
		const float Fade = 1.f - Age / FlashSeconds;
		HurtTint->SetRenderOpacity(0.38f * Pawn->GetHurtStrength() * Fade * Fade);
	}
}

void UNoclipHUDWidget::RefreshTimer(const ANoclipGameState* GS)
{
	const ENoclipShiftPhase Phase = GS->GetPhase();
	const bool bCollapse = Phase == ENoclipShiftPhase::Collapse;
	const bool bClock = Phase == ENoclipShiftPhase::Active || bCollapse;
	const bool bExtracting = bClock && GS->GetExtractionProgress() > 0.f;

	PhaseText->SetText(Upper(FText::Format(LOCTEXT("PhaseFmt", "Turno {0} · {1}"), FText::AsNumber(GS->GetShiftIndex()),
		(Phase == ENoclipShiftPhase::Lobby && GS->IsArrivalPending()) ? LOCTEXT("PhaseArrival", "Elevador") : PhaseName(Phase, bExtracting))));
	Show(ClockText, bClock);
	if (bClock)
	{
		ClockText->SetText(FormatClock(GS->GetPhaseTimeRemaining()));
		ClockText->SetColorAndOpacity(FSlateColor(bCollapse ? Warn() : HudText()));
	}
	Show(CollapseBanner, bCollapse);
	Show(CollapseTint, bCollapse);

	const bool bPrep = Phase == ENoclipShiftPhase::Preparing;
	const bool bArrival = Phase == ENoclipShiftPhase::Lobby && GS->IsArrivalPending();
	Show(CountdownPanel, bPrep || bArrival);
	if (bArrival)
	{
		// Vindo da sala de espera: o elevador "desce" enquanto a equipe termina de conectar.
		const int32 Expected = FMath::Max(GS->GetExpectedArrivals(), 1);
		CountdownTitle->SetText(Upper(LOCTEXT("ElevatorDescending", "Elevador descendo para o Level 0")));
		CountdownText->SetText(FText::Format(LOCTEXT("ArrivedFmt", "{0}/{1}"), FText::AsNumber(FMath::Min(GS->PlayerArray.Num(), Expected)), FText::AsNumber(Expected)));
		CountdownInfo->SetText(LOCTEXT("ArrivalInfo", "Aguardando a equipe chegar ao andar"));
	}
	if (bPrep)
	{
		CountdownTitle->SetText(Upper(LOCTEXT("ShiftStartsIn", "O expediente começa em")));
		CountdownText->SetText(FText::AsNumber(FMath::Max(1, FMath::CeilToInt(GS->GetPhaseTimeRemaining()))));
		CountdownInfo->SetText(FText::Format(LOCTEXT("PrepInfo", "Quota {0} · leve os achados para a Sala Manila"), FormatMoney(GS->GetQuota())));
	}
}

void UNoclipHUDWidget::RefreshQuota(const ANoclipGameState* GS)
{
	const ENoclipShiftPhase Phase = GS->GetPhase();
	const bool bShow = Phase == ENoclipShiftPhase::Active || Phase == ENoclipShiftPhase::Collapse || Phase == ENoclipShiftPhase::Preparing;
	Show(QuotaChip, bShow);
	if (!bShow)
	{
		return;
	}

	const int32 Quota = FMath::Max(1, GS->GetQuota());
	const int32 InRoom = GS->GetValueInExtraction();
	const bool bMet = InRoom >= Quota;
	const FLinearColor ValueColor = bMet ? Ok() : Amber();

	InRoomText->SetText(FormatMoney(InRoom));
	InRoomText->SetColorAndOpacity(FSlateColor(ValueColor));
	QuotaText->SetText(FText::Format(LOCTEXT("QuotaFmt", "/ {0}"), FormatMoney(Quota)));
	QuotaBar.SetPercent(static_cast<float>(InRoom) / Quota);
	QuotaBar.SetColor(ValueColor);
	QuotaHintText->SetText(bMet
		? LOCTEXT("QuotaMet", "Quota batida ✓ · o excedente vira caixa")
		: FText::Format(LOCTEXT("QuotaMissing", "Faltam {0} dentro da sala"), FormatMoney(Quota - InRoom)));
}

void UNoclipHUDWidget::RefreshCenter(const ANoclipGameState* GS, const ANoclipCharacter* Pawn)
{
	const UNoclipGrabComponent* Grab = Pawn ? Pawn->GetGrabComponent() : nullptr;
	const APlayerController* PC = GetOwningPlayer();

	const float Charge = Grab ? Grab->GetThrowCharge() : 0.f;
	Show(ThrowBar.Root, Charge > 0.f);
	if (Charge > 0.f)
	{
		ThrowBar.SetPercent(Charge);
		ThrowBar.SetColor(Charge >= 1.f ? Warn() : Amber());
	}

	bool bTooHeavy = false;
	AActor* Candidate = (Grab && !Grab->IsHolding()) ? Grab->FindGrabCandidate(bTooHeavy) : nullptr;
	Show(PromptPanel, Candidate != nullptr);
	if (!Candidate)
	{
		return;
	}

	GrabHint.Key->SetText(NoclipGlyphs::GetKeyText(ENoclipPrompt::Grab, PC));
	GrabHint.Label->SetText(bTooHeavy ? LOCTEXT("TooHeavy", "Pesado demais") : LOCTEXT("Grab", "Agarrar"));
	GrabHint.Label->SetColorAndOpacity(FSlateColor(bTooHeavy ? DangerText() : HudText()));

	const ANoclipLootActor* Loot = Cast<ANoclipLootActor>(Candidate);
	if (Loot)
	{
		const FText Name = Loot->GetDisplayName().IsEmpty() ? LOCTEXT("UnknownLoot", "Achado") : Loot->GetDisplayName();
		FNumberFormattingOptions Kg;
		Kg.MaximumFractionalDigits = 0;
		FText Detail = FNoclipUserSettings::Get().bShowValueOnAim
			? FText::Format(LOCTEXT("LootDetail", "{0} · {1} · {2} kg"), Name, FormatMoney(Loot->GetCurrentValue()), FText::AsNumber(Loot->GetMassKg(), &Kg))
			: FText::Format(LOCTEXT("LootDetailNoValue", "{0} · {1} kg"), Name, FText::AsNumber(Loot->GetMassKg(), &Kg));
		// Achado de equipe: quantos precisam e quantos ja estao segurando.
		if (Loot->IsTeamCarry())
		{
			Detail = FText::Format(LOCTEXT("LootTeam", "{0}\nPrecisa de {1} estagiários para levantar · {2}/{1} segurando"),
				Detail, FText::AsNumber(Loot->GetRequiredCarriers()), FText::AsNumber(Loot->GetNumCarriers()));
			if (!bTooHeavy)
			{
				GrabHint.Label->SetText(Loot->GetNumCarriers() > 0 ? LOCTEXT("GrabJoin", "Ajudar a carregar") : LOCTEXT("GrabTeam", "Agarrar (em equipe)"));
			}
		}
		PromptDetail->SetText(Detail);
	}
	Show(PromptDetail, Loot != nullptr);
}

void UNoclipHUDWidget::RefreshHeld(const ANoclipCharacter* Pawn)
{
	const UNoclipGrabComponent* Grab = Pawn ? Pawn->GetGrabComponent() : nullptr;
	const ANoclipLootActor* Loot = Grab ? Cast<ANoclipLootActor>(Grab->GetHeldActor()) : nullptr;
	const bool bHolding = Grab && Grab->IsHolding();
	const float Stamina = Pawn ? Pawn->GetStamina() : 1.f;

	// O cartao aparece segurando algo; sem nada na mao, so o folego / a saude quando nao estao cheios.
	const float Health = Pawn ? Pawn->GetHealth() / FMath::Max(Pawn->GetMaxHealth(), 1.f) : 1.f;
	Show(HeldChip, bHolding || Stamina < 0.999f || Health < 0.999f);
	Show(HealthRow, Health < 0.999f);
	HealthBar.SetPercent(Health);
	HealthBar.SetColor(Health < 0.35f ? DangerText() : OkText());
	Show(HeldTop, bHolding);
	Show(HeldHints, bHolding);
	Show(HeldDesc, bHolding && Loot && !Loot->GetDescription().IsEmpty());
	StaminaBar.SetPercent(Stamina);
	StaminaBar.SetColor(Stamina < 0.3f ? Warn() : Text());
	if (!bHolding)
	{
		return;
	}

	const APlayerController* PC = GetOwningPlayer();
	DropHint.Key->SetText(NoclipGlyphs::GetKeyText(ENoclipPrompt::Grab, PC));
	ThrowHint.Key->SetText(NoclipGlyphs::GetKeyText(ENoclipPrompt::Throw, PC));
	Show(ThrowHint.Root, !(Loot && Loot->IsTeamCarry())); // achado de equipe nao se arremessa

	if (Loot)
	{
		HeldName->SetText(Loot->GetDisplayName().IsEmpty() ? LOCTEXT("UnknownLoot", "Achado") : Loot->GetDisplayName());
		HeldValue->SetText(FormatMoney(Loot->GetCurrentValue()));
		if (Loot->IsTeamCarry())
		{
			// Carregando em equipe: o cartao mostra a equipe em vez da piada.
			const int32 Need = Loot->GetRequiredCarriers();
			const int32 Have = Loot->GetNumCarriers();
			HeldDesc->SetText(Have >= Need
				? FText::Format(LOCTEXT("TeamLifted", "Carregando em {0} · peso dividido ({1} kg cada)"), FText::AsNumber(Have), FText::AsNumber(FMath::RoundToInt(Loot->GetMassKg() / FMath::Max(Have, 1))))
				: FText::Format(LOCTEXT("TeamDragging", "Arrastando ({0}/{1}) · falta {2} para levantar · o arrasto faz barulho"), FText::AsNumber(Have), FText::AsNumber(Need), FText::AsNumber(Need - Have)));
			HeldDesc->SetColorAndOpacity(FSlateColor(Have >= Need ? Ok() : Warn()));
			Show(HeldDesc, true);
		}
		else
		{
			HeldDesc->SetText(Loot->GetDescription());
			HeldDesc->SetColorAndOpacity(FSlateColor(Soft()));
		}
		const int32 Lost = Loot->GetBaseValue() - Loot->GetCurrentValue();
		HeldLost->SetText(Lost > 0 ? FText::Format(LOCTEXT("HeldLost", "(−{0})"), FText::AsNumber(Lost)) : FText::GetEmpty());
		HeldLost->SetColorAndOpacity(FSlateColor(DangerText()));
	}
	else
	{
		HeldName->SetText(LOCTEXT("HeldObject", "Objeto"));
		HeldValue->SetText(FText::GetEmpty());
		HeldLost->SetText(FText::GetEmpty());
	}
}

void UNoclipHUDWidget::RefreshExtraction(const ANoclipGameState* GS)
{
	const ENoclipShiftPhase Phase = GS->GetPhase();
	const bool bActive = Phase == ENoclipShiftPhase::Active || Phase == ENoclipShiftPhase::Collapse;
	const int32 Inside = GS->GetPlayersInExtraction();
	// O elevador espera so quem esta de pe.
	int32 Alive = 0;
	for (TActorIterator<ANoclipCharacter> It(GetWorld()); It; ++It)
	{
		++Alive;
	}
	const int32 Total = FMath::Max(1, Alive);
	const bool bShow = bActive && Inside > 0;
	Show(ElevatorPanel, bShow);
	if (!bShow)
	{
		return;
	}

	const float Progress = GS->GetExtractionProgress();
	ElevatorText->SetText(Progress > 0.f
		? FText::Format(LOCTEXT("ElevatorUp", "Elevador subindo · {0} de {1} na sala"), FText::AsNumber(Inside), FText::AsNumber(Total))
		: FText::Format(LOCTEXT("ElevatorWait", "Elevador esperando todos · {0} de {1} na sala"), FText::AsNumber(Inside), FText::AsNumber(Total)));
	ElevatorBar.SetPercent(Progress);
}

void UNoclipHUDWidget::RefreshCrew(const ANoclipGameState* GS)
{
	const ANoclipExtractionZone* Zone = GetZone();
	const APlayerState* LocalPS = GetOwningPlayer() ? GetOwningPlayer()->PlayerState : nullptr;
	const UNoclipVoiceSubsystem* Voice = UNoclipVoiceSubsystem::Get(this);
	const bool bInShift = GS->GetPhase() == ENoclipShiftPhase::Active || GS->GetPhase() == ENoclipShiftPhase::Collapse;

	int32 Row = 0;
	for (const APlayerState* PS : GS->PlayerArray)
	{
		if (!PS || Row >= CrewRows.Num())
		{
			continue;
		}
		FCrewRow& R = CrewRows[Row++];
		Show(R.Root, true);

		const FString Name = ShortName(PS->GetPlayerName(), 18);
		R.Name->SetText(PS == LocalPS ? FText::Format(LOCTEXT("YouFmt", "{0} (você)"), FText::FromString(Name)) : FText::FromString(Name));

		const APawn* P = PS->GetPawn();
		const bool bInRoom = bInShift && Zone && P && Zone->IsInside(P->GetActorLocation());
		const ANoclipPlayerState* NPS = Cast<ANoclipPlayerState>(PS);
		const ANoclipPlayerState* LocalNPS = Cast<ANoclipPlayerState>(LocalPS);
		const bool bDead = bInShift && NPS && NPS->IsDead();
		const bool bLocalDead = LocalNPS && LocalNPS->IsDead();
		// Voz: quem esta falando acende o ponto (mortos so aparecem falando para outros mortos).
		const bool bTalking = Voice && Voice->IsPlayerTalking(PS) && (!bDead || bLocalDead || PS == LocalPS);
		const bool bOnLine = UNoclipVoiceSubsystem::IsOnPhoneLine(PS);
		TArray<FText> Tags;
		if (bDead)
		{
			Tags.Add(LOCTEXT("CrewDead", "morto"));
		}
		if (bTalking)
		{
			Tags.Add(LOCTEXT("CrewTalking", "falando"));
		}
		if (bOnLine)
		{
			Tags.Add(LOCTEXT("CrewOnPhone", "no celular"));
		}
		if (bInRoom)
		{
			Tags.Add(LOCTEXT("CrewInRoom", "na sala"));
		}
		R.State->SetText(FText::Join(FText::FromString(TEXT(" · ")), Tags));
		Show(R.State, Tags.Num() > 0);
		const FLinearColor DotColor = bTalking ? Amber() : (bDead ? Danger() : (bInRoom ? Ok() : Line()));
		SetBox(R.Dot, DotColor, bTalking ? Amber() : (bDead ? Danger() : (bInRoom ? Ok() : Soft())), 1.f);
		R.Name->SetColorAndOpacity(FSlateColor(bDead ? Soft() : HudText()));
	}
	for (; Row < CrewRows.Num(); ++Row)
	{
		Show(CrewRows[Row].Root, false);
	}
}

namespace
{
	FText SlotKeyText(int32 Slot, const APlayerController* PC)
	{
		if (NoclipGlyphs::IsUsingGamepad(PC))
		{
			return FText::FromString(Slot == 1 ? TEXT("←") : (Slot == 2 ? TEXT("↑") : TEXT("→")));
		}
		return FText::AsNumber(Slot);
	}

	FText UseKeyText(const APlayerController* PC)
	{
		if (NoclipGlyphs::IsUsingGamepad(PC))
		{
			return FText::FromString(NoclipGlyphs::GetPadStyle(PC) == ENoclipPadStyle::PlayStation ? TEXT("L1") : TEXT("LB"));
		}
		return FText::FromString(TEXT("F"));
	}

	FText DropKeyText(const APlayerController* PC)
	{
		return FText::FromString(NoclipGlyphs::IsUsingGamepad(PC) ? TEXT("↓") : TEXT("G"));
	}
}

void UNoclipHUDWidget::RefreshTools(const ANoclipGameState* GS, const ANoclipCharacter* Pawn)
{
	const UNoclipToolComponent* Tools = Pawn ? Pawn->GetToolComponent() : nullptr;
	// Celular levantado: a tela dele ocupa o lugar do cinto.
	Show(ToolBelt, Tools != nullptr && !Tools->IsPhoneRaised());
	if (!Tools)
	{
		Show(UsePanel, false);
		return;
	}
	const APlayerController* PC = GetOwningPlayer();
	const ENoclipTool Equipped = Tools->GetEquipped();
	for (int32 i = 0; i < ToolSlots.Num(); ++i)
	{
		FToolSlot& S = ToolSlots[i];
		const ENoclipTool Tool = NoclipTools::FromSlot(i + 1);
		const bool bOwned = Tools->Owns(Tool);
		const bool bInHand = Equipped == Tool;
		S.Key->SetText(SlotKeyText(i + 1, PC));
		SetBox(S.Box, bInHand ? Hex(TEXT("2A2416"), 0.85f) : Chip(), bInHand ? Amber() : ChipLine(), bInHand ? 2.f : 1.f);
		S.Box->SetRenderOpacity(bOwned ? 1.f : 0.38f);
		S.Name->SetColorAndOpacity(FSlateColor(bInHand ? Amber() : HudText()));

		const bool bBattery = Tool == ENoclipTool::Flashlight && bOwned;
		Show(S.Battery.Root, bBattery);
		if (bBattery)
		{
			const float B = Tools->GetBattery();
			S.Battery.SetPercent(B);
			S.Battery.SetColor(B < NoclipTools::FlashlightLowBattery ? DangerText() : (Tools->IsFlashlightOn() ? Amber() : Soft()));
		}
		FText Extra;
		if (!bOwned)
		{
			Extra = Tool == ENoclipTool::Medkit ? LOCTEXT("SlotEmptyKit", "sem kits") : LOCTEXT("SlotEmpty", "vazio");
		}
		else if (Tool == ENoclipTool::Medkit)
		{
			Extra = FText::Format(LOCTEXT("KitCount", "×{0}"), FText::AsNumber(Tools->GetMedkitCount()));
		}
		else if (Tool == ENoclipTool::Flashlight)
		{
			Extra = Tools->IsFlashlightOn() ? LOCTEXT("On", "lig.") : LOCTEXT("Off", "desl.");
		}
		else
		{
			Extra = LOCTEXT("NoSignal", "sem sinal");
		}
		S.Extra->SetText(Extra);
	}

	// Dicas do que esta na mao.
	Show(ToolHintRow, Equipped != ENoclipTool::None);
	if (Equipped != ENoclipTool::None)
	{
		UseHint.Key->SetText(UseKeyText(PC));
		ToolDropHint.Key->SetText(DropKeyText(PC));
		FText UseLabel;
		switch (Equipped)
		{
		case ENoclipTool::Flashlight: UseLabel = Tools->IsFlashlightOn() ? LOCTEXT("UseFlashOff", "desligar") : LOCTEXT("UseFlashOn", "ligar"); break;
		case ENoclipTool::Medkit:     UseLabel = LOCTEXT("UseKit", "segurar para curar"); break;
		case ENoclipTool::Phone:      UseLabel = Tools->IsPhoneRaised() ? LOCTEXT("UsePhoneDown", "abaixar") : LOCTEXT("UsePhoneUp", "olhar a tela"); break;
		default: break;
		}
		UseHint.Label->SetText(UseLabel);
	}

	const float Progress = Tools->GetUseProgress();
	Show(UsePanel, Progress > 0.f);
	if (Progress > 0.f)
	{
		UseBar.SetPercent(Progress);
	}
}

void UNoclipHUDWidget::RefreshDeny(const ANoclipCharacter* Pawn)
{
	const UNoclipToolComponent* Tools = Pawn ? Pawn->GetToolComponent() : nullptr;
	const UWorld* World = GetWorld();
	const float Age = (Tools && World) ? static_cast<float>(World->GetTimeSeconds() - Tools->GetLastDenyTime()) : 100.f;
	constexpr float Duration = 2.6f;
	bool bShow = Tools && Age >= 0.f && Age < Duration && Tools->GetLastDenyReason() != ENoclipHandsDenyReason::None;
	if (bShow)
	{
		// O jogador ja resolveu (guardou a ferramenta / soltou o achado): o aviso some na hora.
		const UNoclipGrabComponent* G = Pawn->GetGrabComponent();
		const ENoclipHandsDenyReason R = Tools->GetLastDenyReason();
		if ((R == ENoclipHandsDenyReason::HeavyNeedsBothHands && !Tools->HasToolInHand())
			|| (R == ENoclipHandsDenyReason::HandsFullHeavy && !(G && G->IsHoldingTwoHanded())))
		{
			bShow = false;
		}
	}
	Show(DenyPanel, bShow);
	if (!bShow)
	{
		return;
	}
	DenyPanel->SetRenderOpacity(FMath::Clamp((Duration - Age) / 0.4f, 0.f, 1.f));

	const APlayerController* PC = GetOwningPlayer();
	const ENoclipTool Equipped = Tools->GetEquipped();
	const NoclipTools::FToolDef& Def = NoclipTools::Get(Equipped);
	const UNoclipGrabComponent* Grab = Pawn->GetGrabComponent();
	const ANoclipLootActor* Held = Grab ? Cast<ANoclipLootActor>(Grab->GetHeldActor()) : nullptr;
	switch (Tools->GetLastDenyReason())
	{
	case ENoclipHandsDenyReason::HeavyNeedsBothHands:
		DenyTitle->SetText(Upper(LOCTEXT("DenyHeavy", "Precisa das duas mãos")));
		DenyDetail->SetText(FText::Format(LOCTEXT("DenyHeavyDetail", "Guarde {0} [{1}] ou solte no chão [{2}] para carregar isso."),
			Def.Short, SlotKeyText(Def.Slot, PC), DropKeyText(PC)));
		break;
	case ENoclipHandsDenyReason::HandsFullHeavy:
		DenyTitle->SetText(Upper(LOCTEXT("DenyFull", "Mãos ocupadas")));
		DenyDetail->SetText(FText::Format(LOCTEXT("DenyFullDetail", "{0} precisa das duas mãos. Solte [{1}] antes de pegar uma ferramenta."),
			Held && !Held->GetDisplayName().IsEmpty() ? Held->GetDisplayName() : LOCTEXT("ThisLoot", "Esse achado"), NoclipGlyphs::GetKeyText(ENoclipPrompt::Grab, PC)));
		break;
	case ENoclipHandsDenyReason::NotOwned:
		DenyTitle->SetText(Upper(LOCTEXT("DenyNotOwned", "Não está no cinto")));
		DenyDetail->SetText(LOCTEXT("DenyNotOwnedDetail", "Vaga vazia (ou já cheia). Kits e lanternas ficam no almoxarifado do saguão."));
		break;
	case ENoclipHandsDenyReason::AlreadyHealthy:
		DenyTitle->SetText(Upper(LOCTEXT("DenyHealthy", "Saúde cheia")));
		DenyDetail->SetText(LOCTEXT("DenyHealthyDetail", "Guarde o kit para quando precisar."));
		break;
	case ENoclipHandsDenyReason::NoBattery:
		DenyTitle->SetText(Upper(LOCTEXT("DenyBattery", "Lanterna sem bateria")));
		DenyDetail->SetText(FText::Format(LOCTEXT("DenyBatteryDetail", "Solte [{0}] e compre outra no almoxarifado."), DropKeyText(PC)));
		break;
	default:
		break;
	}
}

void UNoclipHUDWidget::RefreshPhone(const ANoclipGameState* GS, const ANoclipCharacter* Pawn)
{
	const UNoclipToolComponent* Tools = Pawn ? Pawn->GetToolComponent() : nullptr;
	const bool bShow = Tools && Tools->IsPhoneRaised();
	Show(PhonePanel, bShow);
	if (!bShow)
	{
		return;
	}
	const ENoclipShiftPhase Phase = GS->GetPhase();
	const bool bClock = Phase == ENoclipShiftPhase::Active || Phase == ENoclipShiftPhase::Collapse;
	PhoneShift->SetText(Upper(FText::Format(LOCTEXT("PhoneShift", "Turno {0} · {1}"), FText::AsNumber(GS->GetShiftIndex()), PhaseName(Phase, false))));
	PhoneClock->SetText(bClock ? FormatClock(GS->GetPhaseTimeRemaining()) : FText::FromString(TEXT("--:--")));
	// Alvo do celular: o gerador no apagao, a saida de servico no subsolo, a Sala Manila no resto.
	const ANoclipExtractionZone* Zone = GetZone();
	bool bHasTarget = Zone != nullptr;
	FVector TargetLocation = Zone ? Zone->GetActorLocation() : FVector::ZeroVector;
	FText TargetName = LOCTEXT("PhoneManila", "Sala Manila");
	const ANoclipLevelGenerator* LevelGen = nullptr;
	for (TActorIterator<ANoclipLevelGenerator> It(GetWorld()); It; ++It)
	{
		LevelGen = *It;
		break;
	}
	const bool bInShiftPhone = Phase == ENoclipShiftPhase::Active || Phase == ENoclipShiftPhase::Collapse;
	if (bInShiftPhone && GS->IsPowerOut())
	{
		for (TActorIterator<ANoclipPowerGenerator> It(GetWorld()); It; ++It)
		{
			TargetLocation = It->GetActorLocation();
			TargetName = LOCTEXT("PhoneGenerator", "Gerador");
			bHasTarget = true;
			break;
		}
	}
	else if (LevelGen && LevelGen->HasSubFloor() && LevelGen->GetFloorAt(Pawn->GetActorLocation()) == 1 && LevelGen->GetServiceExits().Num() > 0)
	{
		float Best = TNumericLimits<float>::Max();
		for (const FVector& Exit : LevelGen->GetServiceExits())
		{
			const float D = FVector::Dist2D(Exit, Pawn->GetActorLocation());
			if (D < Best)
			{
				Best = D;
				TargetLocation = Exit;
			}
		}
		TargetName = LOCTEXT("PhoneExit", "Saída de serviço");
		bHasTarget = true;
	}
	PhoneTarget->SetText(Upper(TargetName));
	float Meters = -1.f;
	if (bHasTarget)
	{
		const FVector To = TargetLocation - Pawn->GetActorLocation();
		Meters = To.Size2D() / 100.f;
		const float Rel = FRotator::NormalizeAxis(To.Rotation().Yaw - Pawn->GetAimRotation().Yaw);
		PhoneArrow->SetRenderTransformAngle(Rel);
		FNumberFormattingOptions M;
		M.MaximumFractionalDigits = 0;
		PhoneDistance->SetText(Meters < 3.f ? LOCTEXT("PhoneHere", "você chegou") : FText::Format(LOCTEXT("PhoneMeters", "{0} m"), FText::AsNumber(Meters, &M)));
	}
	else
	{
		PhoneArrow->SetRenderTransformAngle(0.f);
		PhoneDistance->SetText(LOCTEXT("PhoneNoZone", "sem localização"));
	}
	const int32 Bars = FMath::Max(0, NoclipTools::PhoneSignalBars(GetWorld(), Pawn->GetActorLocation()));
	PhoneSignal->SetText(Bars == 0 ? LOCTEXT("PhoneNoSignal", "SEM SINAL") : FText::FromString(FString::ChrN(Bars, TEXT('▮')) + FString::ChrN(3 - Bars, TEXT('▯'))));
	const int32 Quota = FMath::Max(1, GS->GetQuota());
	const int32 InRoom = GS->GetValueInExtraction();
	PhoneQuota->SetText(InRoom >= Quota
		? FText::Format(LOCTEXT("PhoneQuotaMet", "Quota {0} batida ✓"), FormatMoney(Quota))
		: FText::Format(LOCTEXT("PhoneQuota", "Quota {0} · faltam {1} na sala"), FormatMoney(Quota), FormatMoney(Quota - InRoom)));
	// Ligacao do grupo: quem mais esta com o celular levantado.
	TArray<FString> Line;
	if (const UNoclipVoiceSubsystem* Voice = UNoclipVoiceSubsystem::Get(this))
	{
		Voice->GetPhoneLineNames(Line);
	}
	FText Footer = FText::Format(LOCTEXT("PhoneFooter", "[{0}] abaixar"), UseKeyText(GetOwningPlayer()));
	if (Line.Num() > 0)
	{
		TArray<FText> Names;
		for (const FString& N : Line)
		{
			Names.Add(FText::FromString(ShortName(N, 10)));
		}
		Footer = FText::Format(LOCTEXT("PhoneLine", "Na linha: {0}  ·  {1}"), FText::Join(FText::FromString(TEXT(", ")), Names), Footer);
	}
	else
	{
		Footer = FText::Format(LOCTEXT("PhoneNobody", "Ligação aberta — ninguém na linha  ·  {0}"), Footer);
	}
	PhoneFooter->SetText(Footer);
}

#undef LOCTEXT_NAMESPACE
