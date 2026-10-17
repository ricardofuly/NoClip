// Copyright Fuly Games. All Rights Reserved.

#include "UI/NoclipWaitingRoomScreen.h"
#include "UI/NoclipButton.h"
#include "UI/NoclipUIStyle.h"
#include "UI/NoclipInputGlyphs.h"
#include "UI/NoclipPrimaryLayout.h"
#include "UI/NoclipCharacterSelectScreen.h"
#include "Character/NoclipCharacterCatalog.h"
#include "Game/NoclipGameState.h"
#include "Game/NoclipPlayerState.h"
#include "Game/NoclipPlayerController.h"
#include "Game/NoclipUserSettings.h"
#include "Session/NoclipSessionSubsystem.h"
#include "World/NoclipWaitingRoom.h"
#include "World/NoclipWaitingAvatar.h"
#include "World/NoclipElevator.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/GameInstance.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerState.h"
#include "HAL/PlatformApplicationMisc.h"
#include "Misc/DateTime.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(NoclipWaitingRoomScreen)

#define LOCTEXT_NAMESPACE "NoclipWaitingRoom"

using namespace NoclipUI;

namespace
{
	constexpr float CamW = 576.f;
	constexpr float CamH = 324.f;
	constexpr int32 MaxTracks = 6;

	UNoclipButton* NewButton(UWidgetTree* Tree, const FText& Label, ENoclipButtonKind Kind, ENoclipButtonStyle Style = ENoclipButtonStyle::Menu)
	{
		UNoclipButton* Button = Tree->ConstructWidget<UNoclipButton>(UNoclipButton::StaticClass());
		Button->Configure(Label, Kind, Style);
		return Button;
	}

	UCanvasPanelSlot* Place(UCanvasPanel* Canvas, UWidget* Widget, const FAnchors& Anchors, const FVector2D& Alignment, const FMargin& Offsets, bool bAutoSize)
	{
		UCanvasPanelSlot* S = Canvas->AddChildToCanvas(Widget);
		S->SetAnchors(Anchors);
		S->SetAlignment(Alignment);
		S->SetOffsets(Offsets);
		S->SetAutoSize(bAutoSize);
		return S;
	}

	UOverlaySlot* AddOverlay(UOverlay* Overlay, UWidget* Widget, EHorizontalAlignment H, EVerticalAlignment V, const FMargin& Padding = FMargin(0.f))
	{
		UOverlaySlot* S = Overlay->AddChildToOverlay(Widget);
		S->SetHorizontalAlignment(H);
		S->SetVerticalAlignment(V);
		S->SetPadding(Padding);
		return S;
	}

	FText PrivacyName(uint8 Privacy)
	{
		switch (Privacy)
		{
		case 1:  return LOCTEXT("PrivInvite", "Só convite");
		case 2:  return LOCTEXT("PrivPublic", "Pública");
		default: return LOCTEXT("PrivFriends", "Só amigos");
		}
	}

	UNoclipSessionSubsystem* GetSessions(const UUserWidget* Widget)
	{
		const UGameInstance* GI = Widget ? Widget->GetGameInstance() : nullptr;
		return GI ? GI->GetSubsystem<UNoclipSessionSubsystem>() : nullptr;
	}
}

void UNoclipWaitingRoomScreen::BuildTree()
{
	bClosesOnBack = false;

	UCanvasPanel* Canvas = WidgetTree->ConstructWidget<UCanvasPanel>();
	WidgetTree->RootWidget = Canvas;
	Place(Canvas, Box(WidgetTree, Bg(), FMargin(0.f)), FAnchors(0.f, 0.f, 1.f, 1.f), FVector2D::ZeroVector, FMargin(0.f), false);

	// ---------- main: left 80, top 72, width 480, gap 24 ----------
	UVerticalBox* Main = WidgetTree->ConstructWidget<UVerticalBox>();
	{
		UVerticalBox* Header = WidgetTree->ConstructWidget<UVerticalBox>();
		BackLink = NewButton(WidgetTree, LOCTEXT("BackOptions", "Voltar às opções da sala"), ENoclipButtonKind::Normal, ENoclipButtonStyle::Link);
		BackLink->OnClicked().AddWeakLambda(this, [this]() { Leave(true); });
		AddV(Header, BackLink, 0.f, HAlign_Left);
		RoomLabel = Label(WidgetTree, FText::GetEmpty());
		AddV(Header, RoomLabel, 6.f);
		Title = H1(WidgetTree, LOCTEXT("Title", "Aguardando equipe"), 56.f);
		AddV(Header, Title, 6.f);
		Description = Wrap(Txt(WidgetTree, FText::GetEmpty(), Mono(15.f), Soft()));
		AddV(Header, Description, 8.f);
		AddV(Main, Header);
	}

	// Convite (cartao manila)
	{
		UBorder* Card = Box(WidgetTree, Manila(), FMargin(20.f, 18.f));
		UVerticalBox* Col = WidgetTree->ConstructWidget<UVerticalBox>();
		Card->SetContent(Col);
		PrivacyText = Txt(WidgetTree, FText::GetEmpty(), Mono(12.f), Ink());
		AddV(Col, SpaceBetween(WidgetTree, Txt(WidgetTree, Upper(LOCTEXT("InviteCode", "Código de convite")), Oswald(18.f, false, 0.08f), Ink()), PrivacyText, VAlign_Bottom));

		UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>();
		CodeText = Txt(WidgetTree, FText::FromString(TEXT("—")), Oswald(40.f, true, 0.12f), Ink());
		AddH(Row, CodeText, 0.f, true);
		UNoclipButton* Copy = NewButton(WidgetTree, FText::FromString(TEXT("❐")), ENoclipButtonKind::Normal, ENoclipButtonStyle::Step);
		Copy->SetFontPx(16.f);
		Copy->OnClicked().AddWeakLambda(this, [this]()
		{
			if (const ANoclipGameState* GS = GetNoclipGameState(); GS && !GS->GetInviteCode().IsEmpty())
			{
				FPlatformApplicationMisc::ClipboardCopy(*GS->GetInviteCode());
				bCopied = true;
				RefreshInfo();
			}
		});
		AddH(Row, Sized(WidgetTree, Copy, 44.f, 44.f), 10.f);
		UNoclipButton* Steam = NewButton(WidgetTree, LOCTEXT("SteamInvite", "Convidar pela Steam"), ENoclipButtonKind::Normal, ENoclipButtonStyle::Compact);
		Steam->SetFontPx(18.f);
		Steam->SetMinHeight(44.f);
		Steam->OnClicked().AddWeakLambda(this, [this]() { InviteViaSteam(); });
		AddH(Row, Steam, 10.f);
		AddV(Col, Row, 12.f);
		AddV(Col, Dashed(WidgetTree, ManilaRule()), 12.f);
		CopyMsg = Wrap(Txt(WidgetTree, FText::GetEmpty(), Mono(13.f), Ink()));
		AddV(Col, CopyMsg, 10.f);
		InviteCard = Card;
		AddV(Main, Card, 20.f);
	}

	// Colegas (quem entrou pelo convite): sem codigo, so o resumo do embarque.
	{
		UBorder* Card = Box(WidgetTree, Manila(), FMargin(20.f, 18.f));
		UVerticalBox* Col = WidgetTree->ConstructWidget<UVerticalBox>();
		Card->SetContent(Col);
		GuestPrivacyText = Txt(WidgetTree, FText::GetEmpty(), Mono(12.f), Ink());
		AddV(Col, SpaceBetween(WidgetTree, Txt(WidgetTree, Upper(LOCTEXT("Boarding", "Ordem de embarque")), Oswald(18.f, false, 0.08f), Ink()), GuestPrivacyText, VAlign_Bottom));
		auto AddRow = [this, Col](const FText& Caption, UTextBlock*& OutValue, float GapBefore)
		{
			OutValue = Txt(WidgetTree, FText::GetEmpty(), Mono(14.f, 600), Ink());
			OutValue->SetTextOverflowPolicy(ETextOverflowPolicy::Ellipsis);
			AddV(Col, SpaceBetween(WidgetTree, Txt(WidgetTree, Caption, Mono(14.f), Ink()), OutValue), GapBefore);
		};
		AddRow(LOCTEXT("GuestHost", "Host"), GuestHostText, 12.f);
		AddRow(LOCTEXT("GuestReady", "Prontos"), GuestReadyText, 8.f);
		AddRow(LOCTEXT("GuestYou", "Você"), GuestYouText, 8.f);
		AddV(Col, Dashed(WidgetTree, ManilaRule()), 12.f);
		AddV(Col, Wrap(Txt(WidgetTree, LOCTEXT("GuestHint", "O elevador desce quando todos estiverem prontos, ou quando o host mandar. Quer chamar mais alguém? Use o convite da Steam."), Mono(13.f), Ink())), 10.f);
		GuestCard = Card;
		AddV(Main, Card, 20.f);
	}

	// Acoes da sala
	//  host:    [Descer para o Level 0 ->]  /  [Estou pronto | Fechar sala]
	//  colegas: [Estou pronto]              /  [Convidar pela Steam | Sair da sala]
	{
		UVerticalBox* Nav = WidgetTree->ConstructWidget<UVerticalBox>();
		DepartButton = NewButton(WidgetTree, LOCTEXT("Depart", "Descer para o Level 0"), ENoclipButtonKind::Primary);
		DepartButton->SetFontPx(24.f);
		DepartButton->SetMinHeight(60.f);
		DepartButton->SetSuffix(FText::FromString(TEXT("→")));
		DepartButton->OnClicked().AddWeakLambda(this, [this]()
		{
			if (ANoclipPlayerController* PC = GetNoclipPC())
			{
				PC->NoclipStart();
			}
		});
		AddV(Nav, DepartButton);

		ReadyButton = NewButton(WidgetTree, LOCTEXT("MarkReady", "Estou pronto"), ENoclipButtonKind::Primary);
		ReadyButton->OnClicked().AddWeakLambda(this, [this]()
		{
			ANoclipPlayerController* PC = GetNoclipPC();
			const ANoclipPlayerState* PS = PC ? PC->GetPlayerState<ANoclipPlayerState>() : nullptr;
			if (PC)
			{
				PC->RequestSetReady(!(PS && PS->IsReady()));
			}
		});
		ReadyBig = Sized(WidgetTree, nullptr);
		AddV(Nav, ReadyBig, 0.f);

		UHorizontalBox* Pair = WidgetTree->ConstructWidget<UHorizontalBox>();
		ReadySmall = Sized(WidgetTree, nullptr);
		AddH(Pair, ReadySmall, 0.f, true, VAlign_Fill);
		SteamButton = NewButton(WidgetTree, LOCTEXT("SteamShort", "Convidar pela Steam"), ENoclipButtonKind::Normal, ENoclipButtonStyle::Compact);
		SteamButton->SetFontPx(18.f);
		SteamButton->SetMinHeight(52.f);
		SteamButton->OnClicked().AddWeakLambda(this, [this]() { InviteViaSteam(); });
		AddH(Pair, SteamButton, 0.f, true, VAlign_Fill);
		CloseButton = NewButton(WidgetTree, LOCTEXT("CloseRoom", "Fechar sala"), ENoclipButtonKind::Danger, ENoclipButtonStyle::Compact);
		CloseButton->SetFontPx(18.f);
		CloseButton->SetMinHeight(52.f);
		CloseButton->OnClicked().AddWeakLambda(this, [this]() { Leave(false); });
		AddH(Pair, CloseButton, 12.f, true, VAlign_Fill);
		AddV(Nav, Pair, 12.f);

		NavNote = Wrap(Txt(WidgetTree, FText::GetEmpty(), Mono(12.f), Muted()));
		AddV(Nav, NavNote, 12.f);
		AddV(Main, Nav, 20.f);
	}
	Place(Canvas, Sized(WidgetTree, Main, 480.f), FAnchors(0.f, 0.f), FVector2D::ZeroVector, FMargin(80.f, 56.f, 0.f, 0.f), true);

	// ---------- aside: right 64, top 72, width 576, gap 14 ----------
	UVerticalBox* Aside = WidgetTree->ConstructWidget<UVerticalBox>();
	{
		UBorder* Frame = Box(WidgetTree, Hex(TEXT("0B0A08")), FMargin(1.f), Line(), 1.f);
		UOverlay* Cam = WidgetTree->ConstructWidget<UOverlay>();
		Frame->SetContent(Cam);

		CameraImage = WidgetTree->ConstructWidget<UImage>();
		CameraImage->SetColorAndOpacity(FLinearColor::White);
		AddOverlay(Cam, CameraImage, HAlign_Fill, VAlign_Fill);

		// Vinheta (box-shadow inset do prototipo): bordas escuras.
		UBorder* Vignette = Box(WidgetTree, FLinearColor::Transparent, FMargin(0.f), FLinearColor(0.f, 0.f, 0.f, 0.45f), 14.f);
		Vignette->SetVisibility(ESlateVisibility::HitTestInvisible);
		AddOverlay(Cam, Vignette, HAlign_Fill, VAlign_Fill);

		TrackCanvas = WidgetTree->ConstructWidget<UCanvasPanel>();
		TrackCanvas->SetVisibility(ESlateVisibility::HitTestInvisible);
		AddOverlay(Cam, TrackCanvas, HAlign_Fill, VAlign_Fill);
		for (int32 i = 0; i < MaxTracks; ++i)
		{
			FTrack T;
			T.Box = Box(WidgetTree, FLinearColor::Transparent, FMargin(0.f), OkText(), 1.f);
			UCanvasPanelSlot* BoxSlot = TrackCanvas->AddChildToCanvas(T.Box);
			BoxSlot->SetAutoSize(false);
			T.Tag = Box(WidgetTree, Hex(TEXT("12100C"), 0.75f), FMargin(4.f, 1.f));
			T.TagText = Txt(WidgetTree, FText::GetEmpty(), Mono(10.f, 400, 0.08f), OkText(), true);
			T.Tag->SetContent(T.TagText);
			UCanvasPanelSlot* TagSlot = TrackCanvas->AddChildToCanvas(T.Tag);
			TagSlot->SetAutoSize(true);
			T.Box->SetVisibility(ESlateVisibility::Collapsed);
			T.Tag->SetVisibility(ESlateVisibility::Collapsed);
			Tracks.Add(T);
		}

		const FLinearColor Osd = Hex(TEXT("E9E4D2"));
		AddOverlay(Cam, Txt(WidgetTree, FText::FromString(TEXT("CAM 03 · ESPERA · ELEV. B")), Mono(12.f, 400, 0.08f), Osd, true), HAlign_Left, VAlign_Top, FMargin(12.f, 10.f));
		UHorizontalBox* Rec = WidgetTree->ConstructWidget<UHorizontalBox>();
		RecDot = Sized(WidgetTree, Box(WidgetTree, Hex(TEXT("E0644F")), FMargin(0.f), FLinearColor::Transparent, 0.f, 4.f), 8.f, 8.f);
		AddH(Rec, RecDot);
		AddH(Rec, Txt(WidgetTree, FText::FromString(TEXT("REC")), Mono(12.f, 400, 0.08f), Osd, true), 6.f);
		AddOverlay(Cam, Rec, HAlign_Right, VAlign_Top, FMargin(12.f, 10.f));
		StampText = Txt(WidgetTree, FText::GetEmpty(), Mono(12.f, 400, 0.08f), Osd, true);
		AddOverlay(Cam, StampText, HAlign_Left, VAlign_Bottom, FMargin(12.f, 10.f));
		DoorText = Txt(WidgetTree, FText::GetEmpty(), Mono(12.f, 400, 0.08f), Osd, true);
		AddOverlay(Cam, DoorText, HAlign_Right, VAlign_Bottom, FMargin(12.f, 10.f));

		AddV(Aside, Sized(WidgetTree, Frame, CamW, CamH));
		AddV(Aside, SpaceBetween(WidgetTree, Label(WidgetTree, LOCTEXT("Circuit", "Baseline Recuperações · circuito interno")), Label(WidgetTree, LOCTEXT("Ground", "Andar térreo"))), 6.f);
	}
	{
		UBorder* Panel = Box(WidgetTree, NoclipUI::Panel(), FMargin(16.f, 14.f), Line(), 1.f);
		UVerticalBox* Col = WidgetTree->ConstructWidget<UVerticalBox>();
		Panel->SetContent(Col);
		CountText = Txt(WidgetTree, FText::GetEmpty(), Oswald(22.f, false), Amber());
		AddV(Col, SpaceBetween(WidgetTree, Label(WidgetTree, LOCTEXT("CrewInRoom", "Equipe na sala")), CountText, VAlign_Bottom));
		CrewGrid = WidgetTree->ConstructWidget<UVerticalBox>();
		AddV(Col, CrewGrid, 8.f);
		AddV(Aside, Panel, 14.f);
	}
	{
		CharacterLink = NewButton(WidgetTree, FText::GetEmpty(), ENoclipButtonKind::Normal, ENoclipButtonStyle::Row);
		CharacterLink->SetMinHeight(36.f);
		UHorizontalBox* H = WidgetTree->ConstructWidget<UHorizontalBox>();
		AddH(H, Txt(WidgetTree, LOCTEXT("YourIntern", "Seu estagiário:"), Mono(14.f), Text()));
		CharacterNameText = Txt(WidgetTree, FText::GetEmpty(), Mono(14.f, 600), Amber());
		CharacterNameText->SetTextOverflowPolicy(ETextOverflowPolicy::Ellipsis);
		AddH(H, CharacterNameText, 8.f, true);
		AddH(H, Txt(WidgetTree, LOCTEXT("Change", "Trocar →"), Mono(14.f), Amber()), 8.f);
		CharacterLink->SetRowContent(H);
		CharacterLink->OnClicked().AddWeakLambda(this, [this]()
		{
			const ANoclipWaitingRoom* Room = GetRoom();
			if (Room && Room->GetPhase() != ENoclipWaitingPhase::Gathering)
			{
				return;
			}
			if (UNoclipPrimaryLayout* L = GetLayout())
			{
				L->PushMenu(UNoclipCharacterSelectScreen::StaticClass());
			}
		});
		AddV(Aside, CharacterLink, 8.f);
		// Versao embaixo da coluna da direita (no rodape ela batia nos botoes da esquerda em 720p).
		AddV(Aside, Txt(WidgetTree, LOCTEXT("Build", "NOCLIP_0.1 · build de desenvolvimento"), Mono(12.f), Muted()), 10.f, HAlign_Right);
	}
	Place(Canvas, Sized(WidgetTree, Aside, CamW), FAnchors(1.f, 0.f), FVector2D(1.f, 0.f), FMargin(-64.f, 56.f, 0.f, 0.f), true);

	// ---------- footer: left 80, right 64, bottom 32 ----------
	{
		UHorizontalBox* Footer = WidgetTree->ConstructWidget<UHorizontalBox>();
		UHorizontalBox* Hints = WidgetTree->ConstructWidget<UHorizontalBox>();
		auto Hint = [this, Hints](UTextBlock*& KeyA, UTextBlock** KeyB, const FText& LabelText, float GapBefore)
		{
			UHorizontalBox* H = WidgetTree->ConstructWidget<UHorizontalBox>();
			AddH(H, Key(WidgetTree, KeyA));
			if (KeyB)
			{
				AddH(H, Key(WidgetTree, *KeyB), 8.f);
			}
			AddH(H, Txt(WidgetTree, LabelText, Mono(13.f), Muted()), 8.f);
			AddH(Hints, H, GapBefore);
		};
		Hint(KeyUp, &KeyDown, LOCTEXT("Navigate", "navegar"), 0.f);
		Hint(KeyConfirm, nullptr, LOCTEXT("Confirm", "confirmar"), 20.f);
		Hint(KeyBack, nullptr, LOCTEXT("Back", "voltar"), 20.f);
		AddH(Footer, Hints);
		// Dicas de teclas no canto inferior direito, abaixo da coluna da direita (a esquerda vai ate o fim da tela).
		Place(Canvas, Footer, FAnchors(1.f, 1.f), FVector2D(1.f, 1.f), FMargin(-64.f, -22.f, 0.f, 0.f), true);
	}

	DefaultFocusWidget = DepartButton;
}

void UNoclipWaitingRoomScreen::NativeOnActivated()
{
	ApplyRole(IsLeader());
	RefreshInfo();
	Super::NativeOnActivated();
	if (UNoclipPrimaryLayout* L = GetLayout())
	{
		L->SetBusy(FText::GetEmpty());
	}
}

void UNoclipWaitingRoomScreen::InviteViaSteam()
{
	ANoclipPlayerController* PC = GetNoclipPC();
	if (!PC || !PC->ShowInviteOverlay())
	{
		if (UNoclipPrimaryLayout* L = GetLayout())
		{
			L->ShowToast(IsLeader()
				? LOCTEXT("NoOverlay", "Overlay da Steam indisponível. Mande o código de convite.")
				: LOCTEXT("NoOverlayGuest", "Overlay da Steam indisponível. Peça o código de convite ao host."), false);
		}
	}
}

void UNoclipWaitingRoomScreen::ApplyRole(bool bLeader)
{
	// O mesmo botao de pronto troca de lugar: grande (colegas) ou metade da linha (host).
	bRoleApplied = true;
	bRoleIsLeader = bLeader;
	if (ReadyButton->GetParent())
	{
		ReadyButton->RemoveFromParent();
	}
	ReadyButton->Configure(FText::GetEmpty(), bLeader ? ENoclipButtonKind::Normal : ENoclipButtonKind::Primary, bLeader ? ENoclipButtonStyle::Compact : ENoclipButtonStyle::Menu);
	if (bLeader)
	{
		ReadyButton->SetFontPx(18.f);
		ReadyButton->SetMinHeight(52.f);
		ReadySmall->SetContent(ReadyButton);
	}
	else
	{
		ReadyButton->SetFontPx(24.f);
		ReadyButton->SetMinHeight(60.f);
		ReadyBig->SetContent(ReadyButton);
	}
	const ESlateVisibility HostOnly = bLeader ? ESlateVisibility::Visible : ESlateVisibility::Collapsed;
	const ESlateVisibility GuestOnly = bLeader ? ESlateVisibility::Collapsed : ESlateVisibility::Visible;
	DepartButton->SetVisibility(HostOnly);
	InviteCard->SetVisibility(HostOnly);
	ReadySmall->SetVisibility(HostOnly);
	GuestCard->SetVisibility(GuestOnly);
	SteamButton->SetVisibility(GuestOnly);
	ReadyBig->SetVisibility(GuestOnly);
	CloseButton->SetLabel(bLeader ? LOCTEXT("CloseRoom", "Fechar sala") : LOCTEXT("LeaveRoom", "Sair da sala"));
	DefaultFocusWidget = bLeader ? static_cast<UWidget*>(DepartButton) : static_cast<UWidget*>(ReadyButton);
}

ANoclipWaitingRoom* UNoclipWaitingRoomScreen::GetRoom()
{
	if (!CachedRoom.IsValid() && GetWorld())
	{
		for (TActorIterator<ANoclipWaitingRoom> It(GetWorld()); It; ++It)
		{
			CachedRoom = *It;
			break;
		}
	}
	return CachedRoom.Get();
}

void UNoclipWaitingRoomScreen::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	RefreshCamera(InDeltaTime);

	RefreshAccumulator += InDeltaTime;
	if (RefreshAccumulator >= 0.2f)
	{
		RefreshAccumulator = 0.f;
		RefreshInfo();
		RefreshCrew();
	}
}

void UNoclipWaitingRoomScreen::RefreshInfo()
{
	const APlayerController* PC = GetOwningPlayer();
	const ANoclipGameState* GS = GetNoclipGameState();
	const ANoclipWaitingRoom* Room = GetRoom();
	const bool bLeader = IsLeader();
	const ENoclipWaitingPhase Phase = Room ? Room->GetPhase() : ENoclipWaitingPhase::Gathering;
	const bool bGathering = Phase == ENoclipWaitingPhase::Gathering;

	KeyUp->SetText(FText::FromString(TEXT("↑")));
	KeyDown->SetText(FText::FromString(TEXT("↓")));
	KeyConfirm->SetText(NoclipGlyphs::GetKeyText(ENoclipPrompt::Confirm, PC));
	KeyBack->SetText(NoclipGlyphs::GetKeyText(ENoclipPrompt::Back, PC));
	BackLink->SetSuffix(NoclipGlyphs::GetKeyText(ENoclipPrompt::Back, PC), true);
	BackLink->SetLabel(bLeader ? LOCTEXT("BackOptions", "Voltar às opções da sala") : LOCTEXT("BackMenu", "Sair da sala"));

	if (GS)
	{
		RoomLabel->SetText(Upper(FText::Format(LOCTEXT("RoomLabel", "Sala de espera · {0}"), FText::FromString(ShortName(GS->GetRoomName(), 28)))));
		PrivacyText->SetText(FText::Format(LOCTEXT("Access", "Acesso: {0}"), PrivacyName(GS->GetRoomPrivacy())));
		CodeText->SetText(FText::FromString(GS->GetInviteCode().IsEmpty() ? TEXT("—") : GS->GetInviteCode()));
	}

	switch (Phase)
	{
	case ENoclipWaitingPhase::Gathering: Title->SetText(Upper(LOCTEXT("Title", "Aguardando equipe"))); break;
	case ENoclipWaitingPhase::Boarding:  Title->SetText(Upper(LOCTEXT("TitleBoarding", "Embarcando"))); break;
	case ENoclipWaitingPhase::Departing: Title->SetText(Upper(LOCTEXT("TitleDeparting", "Descendo"))); break;
	}
	Description->SetText(Phase == ENoclipWaitingPhase::Departing
		? LOCTEXT("DescDeparting", "Portas fechadas. O elevador B está descendo — próxima parada: Level 0.")
		: Phase == ENoclipWaitingPhase::Boarding
		? LOCTEXT("DescBoarding", "Portas abertas. A equipe está entrando no elevador B — próxima parada: Level 0.")
		: (bLeader
			? LOCTEXT("DescHost", "O elevador desce quando todos marcarem pronto, ou quando você mandar. Quem entrar agora começa o turno 1 com a equipe.")
			: LOCTEXT("DescClient", "Você entrou na sala. Marque pronto quando estiver preparado — o turno 1 começa assim que o elevador chegar ao Level 0.")));

	CopyMsg->SetText(bCopied ? LOCTEXT("Copied", "Código copiado. Cole no chat da equipe.") : LOCTEXT("CopyHint", "Mande o código para quem vai entrar, ou chame pela Steam."));

	if (!bRoleApplied || bRoleIsLeader != bLeader)
	{
		ApplyRole(bLeader);
	}
	const ANoclipPlayerState* PS = PC ? PC->GetPlayerState<ANoclipPlayerState>() : nullptr;
	const bool bReady = PS && PS->IsReady();
	DepartButton->SetIsEnabled(bGathering);
	ReadyButton->SetLabel(bReady ? LOCTEXT("ReadyOn", "Pronto ✓ · desmarcar") : LOCTEXT("MarkReady", "Estou pronto"));
	ReadyButton->SetOn(bReady);
	ReadyButton->SetIsEnabled(bGathering);
	CloseButton->SetIsEnabled(bGathering);
	SteamButton->SetIsEnabled(bGathering);
	BackLink->SetIsEnabled(bGathering);
	CharacterLink->SetIsEnabled(bGathering);
	CharacterNameText->SetText(NoclipCharacters::Get(PS ? PS->GetCharacterIndex() : NoclipCharacters::GetLocalChoice()).Name);
	NavNote->SetText(bLeader
		? LOCTEXT("NoteHost", "Com todos prontos o elevador desce sozinho. Só o host desce antes, sem a sala cheia.")
		: LOCTEXT("NoteClient", "Só o host desce o elevador antes de todos estarem prontos."));

	if (GS && !bLeader)
	{
		const APlayerState* Leader = GS->GetLeader();
		int32 Ready = 0;
		for (const APlayerState* Other : GS->PlayerArray)
		{
			const ANoclipPlayerState* NPS = Cast<ANoclipPlayerState>(Other);
			Ready += (NPS && NPS->IsReady()) ? 1 : 0;
		}
		GuestPrivacyText->SetText(FText::Format(LOCTEXT("Access", "Acesso: {0}"), PrivacyName(GS->GetRoomPrivacy())));
		GuestHostText->SetText(Leader ? ShortNameText(Leader->GetPlayerName(), 20) : FText::FromString(TEXT("—")));
		GuestReadyText->SetText(FText::Format(LOCTEXT("ReadyCount", "{0} / {1}"), FText::AsNumber(Ready), FText::AsNumber(GS->PlayerArray.Num())));
		GuestYouText->SetText(bReady ? LOCTEXT("YouReady", "Pronto ✓") : LOCTEXT("YouWaiting", "Falta você marcar pronto"));
	}

	// Elevador descendo: cobre a tela ate a viagem.
	const bool bWantBusy = Phase == ENoclipWaitingPhase::Departing && Room && Room->GetPhaseElapsed() > 1.6f;
	if (bWantBusy != bBusyShown)
	{
		bBusyShown = bWantBusy;
		if (UNoclipPrimaryLayout* L = GetLayout())
		{
			L->SetBusy(bWantBusy ? LOCTEXT("Descending", "Descendo para o Level 0…") : FText::GetEmpty());
		}
	}
}

void UNoclipWaitingRoomScreen::RefreshCrew()
{
	const ANoclipGameState* GS = GetNoclipGameState();
	if (!GS)
	{
		return;
	}
	const APlayerState* LocalPS = GetOwningPlayer() ? GetOwningPlayer()->PlayerState : nullptr;
	const int32 Max = FMath::Max(GS->GetMaxPlayers(), GS->PlayerArray.Num());

	struct FEntry { FString Name; int32 State = 0; }; // 0 na sala, 1 pronto, 2 host, 3 host pronto
	TArray<FEntry> Entries;
	FString Signature = FString::Printf(TEXT("%d|"), Max);
	for (const APlayerState* PS : GS->PlayerArray)
	{
		if (!PS)
		{
			continue;
		}
		FEntry E;
		E.Name = ShortName(PS->GetPlayerName(), 14);
		if (PS == LocalPS)
		{
			E.Name = FString::Printf(TEXT("%s (você)"), *E.Name);
		}
		const ANoclipPlayerState* NPS = Cast<ANoclipPlayerState>(PS);
		E.State = (NPS && NPS->IsReady() ? 1 : 0) + (PS == GS->GetLeader() ? 2 : 0);
		Signature += FString::Printf(TEXT("%s%d|"), *E.Name, E.State);
		Entries.Add(E);
	}

	CountText->SetText(FText::Format(LOCTEXT("CountFmt", "{0} / {1}"), FText::AsNumber(Entries.Num()), FText::AsNumber(Max)));

	if (Signature == CrewSignature)
	{
		return;
	}
	CrewSignature = Signature;

	CrewGrid->ClearChildren();
	UHorizontalBox* Row = nullptr;
	for (int32 i = 0; i < Max; ++i)
	{
		if (i % 2 == 0)
		{
			Row = WidgetTree->ConstructWidget<UHorizontalBox>();
			AddV(CrewGrid, Row, i == 0 ? 0.f : 6.f);
		}

		UBorder* Cell;
		UHorizontalBox* H = WidgetTree->ConstructWidget<UHorizontalBox>();
		if (Entries.IsValidIndex(i))
		{
			const FEntry& E = Entries[i];
			const bool bReady = (E.State & 1) != 0;
			const bool bHost = (E.State & 2) != 0;
			Cell = Box(WidgetTree, Panel2(), FMargin(10.f, 0.f), Line2(), 1.f);
			AddH(H, Sized(WidgetTree, Box(WidgetTree, bReady ? Ok() : Amber(), FMargin(0.f)), 10.f, 10.f));
			UTextBlock* NameText = Txt(WidgetTree, FText::FromString(E.Name), Mono(14.f), Text());
			NameText->SetTextOverflowPolicy(ETextOverflowPolicy::Ellipsis);
			NameText->SetClipping(EWidgetClipping::ClipToBounds);
			AddH(H, NameText, 8.f, true);
			const FText BadgeLabel = bReady ? LOCTEXT("Ready", "Pronto") : (bHost ? LOCTEXT("Host", "Host") : LOCTEXT("InRoom", "Na sala"));
			const FLinearColor BadgeColor = bReady ? OkText() : (bHost ? Amber() : Muted());
			UBorder* Badge = Box(WidgetTree, FLinearColor::Transparent, FMargin(6.f, 2.f), bReady ? Ok() : (bHost ? Amber() : Line()), 1.f);
			Badge->SetContent(Txt(WidgetTree, bHost && bReady ? LOCTEXT("HostReady", "Host · pronto") : BadgeLabel, Mono(11.f), BadgeColor));
			AddH(H, Badge, 8.f);
		}
		else
		{
			Cell = Box(WidgetTree, FLinearColor::Transparent, FMargin(10.f, 0.f), Line(), 1.f);
			AddH(H, Sized(WidgetTree, Box(WidgetTree, FLinearColor::Transparent, FMargin(0.f), Line(), 1.f), 10.f, 10.f));
			AddH(H, Txt(WidgetTree, LOCTEXT("FreeSlot", "Vaga livre"), Mono(14.f), Muted()), 8.f, true);
		}
		Cell->SetVerticalAlignment(VAlign_Center);
		Cell->SetContent(H);
		AddH(Row, Sized(WidgetTree, Cell, 0.f, 36.f), (i % 2 == 0) ? 0.f : 6.f, true);
	}
	if (Max % 2 == 1 && Row)
	{
		AddH(Row, Gap(WidgetTree, 1.f, 36.f), 6.f, true);
	}
}

void UNoclipWaitingRoomScreen::RefreshCamera(float DeltaSeconds)
{
	ANoclipWaitingRoom* Room = GetRoom();

	// REC pisca (1.2 s, 55% aceso); com "reduzir flashes" fica aceso.
	BlinkTime = FMath::Fmod(BlinkTime + DeltaSeconds, 1.2f);
	const bool bRecOn = FNoclipUserSettings::Get().bReduceFlashing || BlinkTime < 0.66f;
	RecDot->SetRenderOpacity(bRecOn ? 1.f : 0.15f);

	const FDateTime Now = FDateTime::Now();
	StampText->SetText(FText::FromString(FString::Printf(TEXT("%02d/%02d/%04d  %02d:%02d:%02d"), Now.GetDay(), Now.GetMonth(), Now.GetYear(), Now.GetHour(), Now.GetMinute(), Now.GetSecond())));

	if (!Room)
	{
		DoorText->SetText(LOCTEXT("NoSignal", "SEM SINAL"));
		return;
	}

	if (!bBoundImage && Room->GetCameraTarget())
	{
		bBoundImage = true;
		// A captura grava alfa 0; o material da UI desenha so o RGB (opaco).
		UMaterialInterface* FeedMat = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/NOCLIP/UI/M_UI_CameraFeed.M_UI_CameraFeed"));
		if (UMaterialInstanceDynamic* MID = FeedMat ? UMaterialInstanceDynamic::Create(FeedMat, this) : nullptr)
		{
			MID->SetTextureParameterValue(TEXT("Feed"), Room->GetCameraTarget());
			CameraImage->SetBrushFromMaterial(MID);
		}
		else
		{
			CameraImage->SetBrushResourceObject(Room->GetCameraTarget());
		}
	}

	const ANoclipElevator* Elevator = Room->GetElevator();
	const float DoorAlpha = Elevator ? Elevator->GetDoorAlpha() : 0.f;
	switch (Room->GetPhase())
	{
	case ENoclipWaitingPhase::Gathering: DoorText->SetText(LOCTEXT("DoorGround", "ELEVADOR NO TÉRREO")); break;
	case ENoclipWaitingPhase::Boarding:  DoorText->SetText(DoorAlpha > 0.05f ? LOCTEXT("DoorOpen", "PORTAS ABERTAS") : LOCTEXT("DoorOpening", "ABRINDO PORTAS")); break;
	case ENoclipWaitingPhase::Departing: DoorText->SetText(DoorAlpha > 0.05f ? LOCTEXT("DoorClosing", "FECHANDO PORTAS") : LOCTEXT("DoorDown", "ELEVADOR DESCENDO ▼")); break;
	}

	// Caixas de rastreio sobre cada estagiario (projecao da camera de seguranca).
	const APlayerState* LocalPS = GetOwningPlayer() ? GetOwningPlayer()->PlayerState : nullptr;
	int32 Used = 0;
	for (TActorIterator<ANoclipWaitingAvatar> It(GetWorld()); It && Used < Tracks.Num(); ++It)
	{
		const ANoclipWaitingAvatar* Avatar = *It;
		// Dentro da cabine com as portas fechando: a camera nao ve mais.
		if (!Avatar || (Avatar->WasInCabinWhenCaptured() && DoorAlpha < 0.6f && Room->GetPhase() != ENoclipWaitingPhase::Gathering))
		{
			continue;
		}
		FVector2D Feet, Head;
		const FVector Base = Avatar->GetCapturedLocation();
		if (!Room->ProjectToCamera(Base, Feet) || !Room->ProjectToCamera(Base + FVector(0.f, 0.f, ANoclipWaitingAvatar::GetHeight()), Head))
		{
			continue;
		}
		const FVector2D Size(CamW - 2.f, CamH - 2.f);
		const FVector2D F = Feet * Size;
		const FVector2D H = Head * Size;
		const float Height = FMath::Max(F.Y - H.Y, 8.f);
		const float Width = Height * 0.36f;

		const bool bMe = Avatar->GetPlayerStateRef() == LocalPS;
		const FLinearColor Color = bMe ? Amber() : OkText();
		FTrack& T = Tracks[Used++];
		SetBox(T.Box, FLinearColor::Transparent, Color, 1.f);
		if (UCanvasPanelSlot* S = Cast<UCanvasPanelSlot>(T.Box->Slot))
		{
			S->SetPosition(FVector2D(F.X - Width * 0.5f - 8.f, H.Y - 8.f));
			S->SetSize(FVector2D(Width + 16.f, Height + 12.f));
		}
		const APlayerState* PS = Avatar->GetPlayerStateRef();
		T.TagText->SetText(ShortNameText(PS ? PS->GetPlayerName() : FString(), 14));
		T.TagText->SetColorAndOpacity(FSlateColor(Color));
		if (UCanvasPanelSlot* S = Cast<UCanvasPanelSlot>(T.Tag->Slot))
		{
			S->SetPosition(FVector2D(F.X - Width * 0.5f - 9.f, H.Y - 26.f));
		}
		T.Box->SetVisibility(ESlateVisibility::HitTestInvisible);
		T.Tag->SetVisibility(ESlateVisibility::HitTestInvisible);
	}
	for (int32 i = Used; i < Tracks.Num(); ++i)
	{
		Tracks[i].Box->SetVisibility(ESlateVisibility::Collapsed);
		Tracks[i].Tag->SetVisibility(ESlateVisibility::Collapsed);
	}
}

bool UNoclipWaitingRoomScreen::HandleBack()
{
	const ANoclipWaitingRoom* Room = GetRoom();
	if (!Room || Room->GetPhase() == ENoclipWaitingPhase::Gathering)
	{
		Leave(true);
	}
	return true;
}

void UNoclipWaitingRoomScreen::Leave(bool bReopenHostOptions)
{
	UNoclipPrimaryLayout* Layout = GetLayout();
	if (!Layout)
	{
		return;
	}
	const bool bLeader = IsHost();
	TWeakObjectPtr<ANoclipPlayerController> WeakPC = GetNoclipPC();
	TWeakObjectPtr<UNoclipSessionSubsystem> WeakSessions = GetSessions(this);
	Layout->PushConfirm(
		bLeader ? LOCTEXT("CloseTitle", "Fechar a sala?") : LOCTEXT("LeaveTitle", "Sair da sala?"),
		bLeader ? LOCTEXT("CloseBody", "A sala de espera fecha e quem já entrou volta ao menu com um aviso.") : LOCTEXT("LeaveBody", "Você volta ao menu. O grupo continua esperando sem você."),
		bLeader ? LOCTEXT("CloseConfirm", "Fechar sala") : LOCTEXT("LeaveConfirm", "Sair"),
		[WeakPC, WeakSessions, bLeader, bReopenHostOptions]()
		{
			if (bLeader && bReopenHostOptions)
			{
				if (UNoclipSessionSubsystem* S = WeakSessions.Get())
				{
					S->RequestReopenHostMenu();
				}
			}
			if (ANoclipPlayerController* P = WeakPC.Get())
			{
				P->LeaveToMainMenu();
			}
		},
		LOCTEXT("Eyebrow", "Sala de espera"));
}

#undef LOCTEXT_NAMESPACE
