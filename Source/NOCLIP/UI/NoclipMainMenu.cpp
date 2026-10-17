// Copyright Fuly Games. All Rights Reserved.

#include "UI/NoclipMainMenu.h"
#include "UI/NoclipButton.h"
#include "UI/NoclipSlider.h"
#include "UI/NoclipUIStyle.h"
#include "UI/NoclipInputGlyphs.h"
#include "UI/NoclipPrimaryLayout.h"
#include "UI/NoclipCharacterSelectScreen.h"
#include "Character/NoclipCharacterCatalog.h"
#include "World/NoclipPortraitStudio.h"
#include "Components/Image.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Game/NoclipPlayerController.h"
#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/Border.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/EditableTextBox.h"
#include "Components/ScrollBox.h"
#include "Engine/GameInstance.h"
#include "Voice/NoclipAudioDevices.h"
#include "Containers/Ticker.h"
#include "Engine/World.h"
#include "InputCoreTypes.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(NoclipMainMenu)

#define LOCTEXT_NAMESPACE "NoclipMenu"

using namespace NoclipUI;

namespace
{
	UNoclipSessionSubsystem* GetSessions(const UUserWidget* Widget)
	{
		const UGameInstance* GI = Widget ? Widget->GetGameInstance() : nullptr;
		return GI ? GI->GetSubsystem<UNoclipSessionSubsystem>() : nullptr;
	}

	UNoclipButton* NewButton(UWidgetTree* Tree, const FText& Label, ENoclipButtonKind Kind, ENoclipButtonStyle Style)
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

	/** Fundo opaco + area com padding (as telas "flex" do prototipo). */
	UVerticalBox* BuildPaddedScreen(UWidgetTree* Tree, const FMargin& Padding, const FLinearColor& BgColor)
	{
		UOverlay* Root = Tree->ConstructWidget<UOverlay>();
		UBorder* Back = Box(Tree, BgColor, FMargin(0.f));
		UOverlaySlot* BackSlot = Root->AddChildToOverlay(Back);
		BackSlot->SetHorizontalAlignment(HAlign_Fill);
		BackSlot->SetVerticalAlignment(VAlign_Fill);

		UVerticalBox* Col = Tree->ConstructWidget<UVerticalBox>();
		UOverlaySlot* ColSlot = Root->AddChildToOverlay(Col);
		ColSlot->SetHorizontalAlignment(HAlign_Fill);
		ColSlot->SetVerticalAlignment(VAlign_Fill);
		ColSlot->SetPadding(Padding);
		Tree->RootWidget = Root;
		return Col;
	}

	/** header: link "[Esc] Voltar..." + h1 56px */
	UVerticalBox* BuildHeader(UWidgetTree* Tree, const FText& BackLabel, const FText& Title, UNoclipButton*& OutLink)
	{
		UVerticalBox* Header = Tree->ConstructWidget<UVerticalBox>();
		OutLink = NewButton(Tree, BackLabel, ENoclipButtonKind::Normal, ENoclipButtonStyle::Link);
		AddV(Header, OutLink, 0.f, HAlign_Left);
		AddV(Header, H1(Tree, Title, 56.f), 6.f, HAlign_Left);
		return Header;
	}

	FText PingText(int32 Ms) { return FText::Format(LOCTEXT("PingFmt", "{0} ms"), FText::AsNumber(Ms)); }

	FLinearColor PingColor(int32 Ms)
	{
		return Ms < 60 ? Ok() : (Ms < 120 ? Amber() : Danger());
	}

	bool IsLeftKey(const FKey& Key) { return Key == EKeys::Left || Key == EKeys::Gamepad_DPad_Left || Key == EKeys::Gamepad_LeftStick_Left; }
	bool IsRightKey(const FKey& Key) { return Key == EKeys::Right || Key == EKeys::Gamepad_DPad_Right || Key == EKeys::Gamepad_LeftStick_Right; }
}

// =====================================================================  Tela inicial

void UNoclipMainMenu::BuildTree()
{
	bClosesOnBack = false;

	UCanvasPanel* Canvas = WidgetTree->ConstructWidget<UCanvasPanel>();
	WidgetTree->RootWidget = Canvas;

	// Lado esquerdo opaco; a direita fica o Level 0 ao vivo (no prototipo, a arte de fundo).
	Place(Canvas, Box(WidgetTree, Bg(), FMargin(0.f)), FAnchors(0.f, 0.f, 0.f, 1.f), FVector2D::ZeroVector, FMargin(0.f, 0.f, 640.f, 0.f), false);
	Place(Canvas, Box(WidgetTree, Hex(TEXT("12100C"), 0.85f), FMargin(0.f)), FAnchors(0.f, 0.f, 0.f, 1.f), FVector2D::ZeroVector, FMargin(560.f, 0.f, 200.f, 0.f), false);

	// main: left 80, top 72, width 480, gap 36
	UVerticalBox* Main = WidgetTree->ConstructWidget<UVerticalBox>();
	AddV(Main, Label(WidgetTree, LOCTEXT("Company", "Baseline Recuperações Ltda. · Programa de estágio")));
	UTextBlock* Title = Txt(WidgetTree, FText::FromString(TEXT("NOCLIP")), Oswald(128.f, true, 0.02f), Amber());
	Title->SetLineHeightPercentage(0.95f / 1.2f);
	AddV(Main, Title, 6.f);
	AddV(Main, Wrap(Txt(WidgetTree, LOCTEXT("Tagline", "Entre, pegue o que caiu do mundo real e saia antes das luzes apagarem."), Mono(15.f), Soft())), 6.f);

	UVerticalBox* Nav = WidgetTree->ConstructWidget<UVerticalBox>();
	HostButton = NewButton(WidgetTree, LOCTEXT("Host", "Hospedar expediente"), ENoclipButtonKind::Primary, ENoclipButtonStyle::Menu);
	HostButton->SetSuffix(FText::FromString(TEXT("→")));
	HostButton->OnClicked().AddWeakLambda(this, [this]() { if (UNoclipPrimaryLayout* L = GetLayout(); L && !L->IsBusy()) { L->PushMenu(UNoclipHostMenu::StaticClass()); } });
	AddV(Nav, HostButton);
	UNoclipButton* Join = NewButton(WidgetTree, LOCTEXT("Join", "Entrar em expediente"), ENoclipButtonKind::Normal, ENoclipButtonStyle::Menu);
	Join->SetSuffix(FText::FromString(TEXT("→")));
	Join->OnClicked().AddWeakLambda(this, [this]() { if (UNoclipPrimaryLayout* L = GetLayout(); L && !L->IsBusy()) { L->PushMenu(UNoclipJoinMenu::StaticClass()); } });
	AddV(Nav, Join, 12.f);
	UNoclipButton* Options = NewButton(WidgetTree, LOCTEXT("Options", "Opções"), ENoclipButtonKind::Normal, ENoclipButtonStyle::Menu);
	Options->SetSuffix(FText::FromString(TEXT("→")));
	Options->OnClicked().AddWeakLambda(this, [this]() { if (UNoclipPrimaryLayout* L = GetLayout(); L && !L->IsBusy()) { L->PushMenu(UNoclipOptionsMenu::StaticClass()); } });
	AddV(Nav, Options, 12.f);
	UNoclipButton* Quit = NewButton(WidgetTree, LOCTEXT("Quit", "Sair do jogo"), ENoclipButtonKind::Danger, ENoclipButtonStyle::Menu);
	Quit->OnClicked().AddUObject(this, &UNoclipMainMenu::HandleQuit);
	AddV(Nav, Quit, 12.f);
	AddV(Main, Nav, 36.f);

	Place(Canvas, Sized(WidgetTree, Main, 480.f), FAnchors(0.f, 0.f), FVector2D::ZeroVector, FMargin(80.f, 72.f, 0.f, 0.f), true);

	// aside Cracha: right 64, top 72, width 300, manila, padding 20, gap 12
	UBorder* Badge = Box(WidgetTree, Manila(), FMargin(20.f));
	UVerticalBox* BadgeCol = WidgetTree->ConstructWidget<UVerticalBox>();
	Badge->SetContent(BadgeCol);
	BadgeId = Txt(WidgetTree, FText::GetEmpty(), Mono(12.f), Ink());
	AddV(BadgeCol, SpaceBetween(WidgetTree, Txt(WidgetTree, Upper(LOCTEXT("BadgeTitle", "Crachá de estagiário")), Oswald(18.f, false, 0.08f), Ink()), BadgeId, VAlign_Bottom));
	UHorizontalBox* Who = WidgetTree->ConstructWidget<UHorizontalBox>();
	// Foto 3x4 do estagiario vestido: clicar (ou focar e confirmar) abre a ficha de admissao.
	{
		UOverlay* PhotoStack = WidgetTree->ConstructWidget<UOverlay>();
		// Render do rosto do estagiario (ANoclipPortraitStudio), ligado no NativeOnActivated.
		UBorder* PhotoBg = Box(WidgetTree, Hex(TEXT("9E9476")), FMargin(0.f));
		BadgePhotoImage = WidgetTree->ConstructWidget<UImage>();
		BadgePhotoImage->SetVisibility(ESlateVisibility::HitTestInvisible);
		PhotoBg->SetContent(BadgePhotoImage);
		PhotoBg->SetHorizontalAlignment(HAlign_Fill);
		PhotoBg->SetVerticalAlignment(VAlign_Fill);
		UOverlaySlot* BgSlot = PhotoStack->AddChildToOverlay(PhotoBg);
		BgSlot->SetHorizontalAlignment(HAlign_Fill);
		BgSlot->SetVerticalAlignment(VAlign_Fill);
		UBorder* Tag = Box(WidgetTree, Ink(), FMargin(0.f, 2.f));
		Tag->SetHorizontalAlignment(HAlign_Center);
		Tag->SetContent(Txt(WidgetTree, Upper(LOCTEXT("Change", "Trocar")), Mono(9.f, 400, 0.14f), Amber()));
		UOverlaySlot* TagSlot = PhotoStack->AddChildToOverlay(Tag);
		TagSlot->SetHorizontalAlignment(HAlign_Fill);
		TagSlot->SetVerticalAlignment(VAlign_Bottom);

		BadgePhoto = NewButton(WidgetTree, LOCTEXT("ChangeIntern", "Trocar estagiário"), ENoclipButtonKind::Normal, ENoclipButtonStyle::Photo);
		BadgePhoto->SetRowContent(Sized(WidgetTree, PhotoStack, 62.f, 78.f));
		BadgePhoto->SetToolTipText(LOCTEXT("ChangeTip", "Trocar estagiário (ficha de admissão)"));
		BadgePhoto->OnClicked().AddWeakLambda(this, [this]()
		{
			if (UNoclipPrimaryLayout* L = GetLayout(); L && !L->IsBusy())
			{
				L->PushMenu(UNoclipCharacterSelectScreen::StaticClass());
			}
		});
		AddH(Who, Sized(WidgetTree, BadgePhoto, 64.f, 80.f), 0.f, false, VAlign_Top);
	}
	UVerticalBox* WhoText = WidgetTree->ConstructWidget<UVerticalBox>();
	BadgeName = Txt(WidgetTree, FText::GetEmpty(), Mono(16.f, 600), Ink());
	BadgeName->SetTextOverflowPolicy(ETextOverflowPolicy::Ellipsis);
	AddV(WhoText, BadgeName);
	AddV(WhoText, Txt(WidgetTree, LOCTEXT("Sector", "Setor: Recuperação"), Mono(13.f), Ink()), 4.f);
	BadgeCharacter = Txt(WidgetTree, FText::GetEmpty(), Mono(13.f), Ink());
	BadgeCharacter->SetTextOverflowPolicy(ETextOverflowPolicy::Ellipsis);
	AddV(WhoText, BadgeCharacter, 4.f);
	AddH(Who, WhoText, 14.f, true);
	AddV(BadgeCol, Who, 12.f);
	AddV(BadgeCol, Dashed(WidgetTree, ManilaRule()), 12.f);
	BadgeLast = Wrap(Txt(WidgetTree, FText::GetEmpty(), Mono(13.f), Ink()));
	AddV(BadgeCol, BadgeLast, 10.f);
	Place(Canvas, Sized(WidgetTree, Badge, 300.f), FAnchors(1.f, 0.f), FVector2D(1.f, 0.f), FMargin(-64.f, 72.f, 0.f, 0.f), true);

	// footer: left 80, right 64, bottom 32
	UHorizontalBox* Footer = WidgetTree->ConstructWidget<UHorizontalBox>();
	auto Hint = [this](UHorizontalBox* Parent, UTextBlock*& KeyA, UTextBlock** KeyB, const FText& LabelText, float GapBefore)
	{
		UHorizontalBox* H = WidgetTree->ConstructWidget<UHorizontalBox>();
		AddH(H, Key(WidgetTree, KeyA));
		if (KeyB)
		{
			AddH(H, Key(WidgetTree, *KeyB), 8.f);
		}
		AddH(H, Txt(WidgetTree, LabelText, Mono(13.f), Muted()), 8.f);
		AddH(Parent, H, GapBefore);
	};
	UHorizontalBox* Hints = WidgetTree->ConstructWidget<UHorizontalBox>();
	Hint(Hints, KeyUp, &KeyDown, LOCTEXT("Navigate", "navegar"), 0.f);
	Hint(Hints, KeyConfirm, nullptr, LOCTEXT("Confirm", "confirmar"), 20.f);
	Hint(Hints, KeyBack, nullptr, LOCTEXT("Back", "voltar"), 20.f);
	AddH(Footer, Hints, 0.f, true);
	AddH(Footer, Txt(WidgetTree, LOCTEXT("Build", "NOCLIP_0.1 · build de desenvolvimento"), Mono(13.f), Muted()));
	Place(Canvas, Footer, FAnchors(0.f, 1.f, 1.f, 1.f), FVector2D(0.f, 1.f), FMargin(80.f, -32.f, 64.f, 28.f), false);

	DefaultFocusWidget = HostButton;
}

void UNoclipMainMenu::RefreshBadgeCharacter()
{
	const int32 Choice = NoclipCharacters::GetLocalChoice();
	BadgeShownChoice = Choice;
	const FNoclipCharacterDef& Def = NoclipCharacters::Get(Choice);
	BadgeCharacter->SetText(FText::Format(LOCTEXT("BadgeCharacter", "Ficha: {0}"), Def.Name));

	if (!PortraitStudio.IsValid())
	{
		PortraitStudio = ANoclipPortraitStudio::SpawnStudio(GetWorld(), Choice);
		if (UMaterialInstanceDynamic* MID = PortraitStudio.IsValid() ? PortraitStudio->MakeUIMaterial(this) : nullptr)
		{
			BadgePhotoImage->SetBrushFromMaterial(MID);
		}
	}
	else
	{
		PortraitStudio->SetCharacter(Choice);
	}
}

void UNoclipMainMenu::NativeOnDeactivated()
{
	if (PortraitStudio.IsValid())
	{
		PortraitStudio->Destroy();
	}
	PortraitStudio.Reset();
	Super::NativeOnDeactivated();
}

void UNoclipMainMenu::NativeOnActivated()
{
	Super::NativeOnActivated();

	if (UNoclipSessionSubsystem* Sessions = GetSessions(this))
	{
		const FString Name = Sessions->GetLocalPlayerName();
		BadgeName->SetText(ShortNameText(Name, 18));
		BadgeId->SetText(FText::Format(LOCTEXT("BadgeId", "Nº {0}"), FText::FromString(FString::Printf(TEXT("%04u"), GetTypeHash(Name) % 10000))));

		const FText Error = Sessions->ConsumePendingError();
		if (!Error.IsEmpty())
		{
			if (UNoclipPrimaryLayout* Layout = GetLayout())
			{
				Layout->ShowToast(Error);
			}
		}
	}
	RefreshBadgeCharacter();

	const FString& Last = FNoclipUserSettings::Get().LastShiftSummary;
	BadgeLast->SetText(FText::Format(LOCTEXT("LastShift", "Último expediente: {0}"), Last.IsEmpty() ? LOCTEXT("NoShift", "nenhum ainda") : FText::FromString(Last)));

	if (UNoclipPrimaryLayout* Layout = GetLayout())
	{
		Layout->SetBusy(FText::GetEmpty());
		// Host saiu da sala de espera por "Voltar as opcoes da sala": reabre Hospedar.
		UNoclipSessionSubsystem* Sessions = GetSessions(this);
		if (Sessions && Sessions->ConsumeReopenHostMenu())
		{
			bPendingHostReopen = true; // empilha no proximo tick, fora da ativacao
		}
	}
}

void UNoclipMainMenu::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	// Voltou da ficha de admissao (ou o estudio da foto sumiu): atualiza o cracha.
	if (!PortraitStudio.IsValid() || BadgeShownChoice != NoclipCharacters::GetLocalChoice())
	{
		RefreshBadgeCharacter();
	}
	if (bPendingHostReopen)
	{
		bPendingHostReopen = false;
		if (UNoclipPrimaryLayout* L = GetLayout())
		{
			L->PushMenu(UNoclipHostMenu::StaticClass());
		}
	}
	RefreshAccumulator += InDeltaTime;
	if (RefreshAccumulator >= 0.25f)
	{
		RefreshAccumulator = 0.f;
		const APlayerController* PC = GetOwningPlayer();
		KeyUp->SetText(FText::FromString(TEXT("↑")));
		KeyDown->SetText(FText::FromString(TEXT("↓")));
		KeyConfirm->SetText(NoclipGlyphs::GetKeyText(ENoclipPrompt::Confirm, PC));
		KeyBack->SetText(NoclipGlyphs::GetKeyText(ENoclipPrompt::Back, PC));
	}
}

bool UNoclipMainMenu::HandleBack()
{
	HandleQuit();
	return true;
}

void UNoclipMainMenu::HandleQuit()
{
	UNoclipPrimaryLayout* Layout = GetLayout();
	if (!Layout || Layout->IsBusy())
	{
		return;
	}
	TWeakObjectPtr<ANoclipPlayerController> WeakPC = GetNoclipPC();
	Layout->PushConfirm(LOCTEXT("QuitTitle", "Sair do jogo?"),
		LOCTEXT("QuitBody", "Você vai fechar o NOCLIP. Corridas em andamento não ficam salvas fora de uma sessão."),
		LOCTEXT("QuitConfirm", "Sair"),
		[WeakPC]()
		{
			if (ANoclipPlayerController* PC = WeakPC.Get())
			{
				PC->QuitToDesktop();
			}
		});
}

// =====================================================================  Hospedar

void UNoclipHostMenu::BuildTree()
{
	UVerticalBox* Screen = BuildPaddedScreen(WidgetTree, FMargin(80.f, 56.f), Bg());
	AddV(Screen, BuildHeader(WidgetTree, LOCTEXT("BackHome", "Voltar ao início"), LOCTEXT("HostTitle", "Hospedar expediente"), BackLink));
	BackLink->OnClicked().AddWeakLambda(this, [this]() { HandleBack(); });

	UHorizontalBox* Body = WidgetTree->ConstructWidget<UHorizontalBox>();
	UVerticalBoxSlot* BodySlot = AddV(Screen, Body, 32.f);
	BodySlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));

	// ---------- form: flex 1, gap 26 ----------
	UVerticalBox* Form = WidgetTree->ConstructWidget<UVerticalBox>();
	AddH(Body, Form, 0.f, true, VAlign_Top);

	UVerticalBox* NameGroup = WidgetTree->ConstructWidget<UVerticalBox>();
	AddV(NameGroup, Label(WidgetTree, LOCTEXT("RoomName", "Nome da sala")));
	NameInput = Input(WidgetTree, LOCTEXT("RoomNameHint", "Expediente de [seu nome]"));
	AddV(NameGroup, Sized(WidgetTree, NameInput, 0.f, 48.f), 8.f);
	AddV(Form, NameGroup);

	UVerticalBox* Privacy = WidgetTree->ConstructWidget<UVerticalBox>();
	AddV(Privacy, Label(WidgetTree, LOCTEXT("WhoCanJoin", "Quem pode entrar")));
	UHorizontalBox* Segs = WidgetTree->ConstructWidget<UHorizontalBox>();
	const FText SegTitles[] = { LOCTEXT("Friends", "Só amigos"), LOCTEXT("Invite", "Só convite"), LOCTEXT("Public", "Pública") };
	const FText SegHints[] = { LOCTEXT("FriendsHint", "Amigos da Steam"), LOCTEXT("InviteHint", "Quem você chamar"), LOCTEXT("PublicHint", "Aparece na busca") };
	for (int32 i = 0; i < 3; ++i)
	{
		UNoclipButton* Seg = NewButton(WidgetTree, SegTitles[i], ENoclipButtonKind::Normal, ENoclipButtonStyle::Seg);
		Seg->SetSubtext(SegHints[i]);
		Seg->OnClicked().AddWeakLambda(this, [this, i]() { Settings.Privacy = static_cast<ENoclipRoomPrivacy>(i); Refresh(); });
		AddH(Segs, Seg, i > 0 ? 10.f : 0.f, true, VAlign_Fill);
		PrivacyButtons.Add(Seg);
	}
	AddV(Privacy, Segs, 16.f);
	AddV(Form, Privacy, 26.f);

	UVerticalBox* Slots = WidgetTree->ConstructWidget<UVerticalBox>();
	AddV(Slots, Label(WidgetTree, LOCTEXT("CrewSize", "Estagiários na equipe")));
	UHorizontalBox* Stepper = WidgetTree->ConstructWidget<UHorizontalBox>();
	UNoclipButton* Minus = NewButton(WidgetTree, FText::FromString(TEXT("−")), ENoclipButtonKind::Normal, ENoclipButtonStyle::Step);
	Minus->OnClicked().AddWeakLambda(this, [this]() { Settings.MaxPlayers = FMath::Max(1, Settings.MaxPlayers - 1); Refresh(); });
	AddH(Stepper, Minus);
	PlayersValue = Txt(WidgetTree, FText::GetEmpty(), Oswald(34.f, false), TextBright());
	PlayersValue->SetJustification(ETextJustify::Center);
	USizeBox* ValueBox = Sized(WidgetTree, PlayersValue, 0.f, 0.f);
	ValueBox->SetMinDesiredWidth(72.f);
	AddH(Stepper, ValueBox, 12.f);
	UNoclipButton* Plus = NewButton(WidgetTree, FText::FromString(TEXT("+")), ENoclipButtonKind::Normal, ENoclipButtonStyle::Step);
	Plus->OnClicked().AddWeakLambda(this, [this]() { Settings.MaxPlayers = FMath::Min(6, Settings.MaxPlayers + 1); Refresh(); });
	AddH(Stepper, Plus, 12.f);
	AddH(Stepper, Txt(WidgetTree, LOCTEXT("OneToSix", "de 1 a 6"), Mono(13.f), Muted()), 12.f);
	AddV(Slots, Stepper, 8.f, HAlign_Left);
	AddV(Form, Slots, 26.f);

	JoinCheck = NewButton(WidgetTree, LOCTEXT("JoinInProgress", "Permitir entrar no meio do turno"), ENoclipButtonKind::Normal, ENoclipButtonStyle::Check);
	JoinCheck->OnClicked().AddWeakLambda(this, [this]() { Settings.bJoinInProgress = !Settings.bJoinInProgress; Refresh(); });
	AddV(Form, JoinCheck, 26.f, HAlign_Left);

	// ---------- aside: 380, gap 16 ----------
	UVerticalBox* Aside = WidgetTree->ConstructWidget<UVerticalBox>();
	UBorder* Order = Box(WidgetTree, Manila(), FMargin(22.f));
	UVerticalBox* OrderCol = WidgetTree->ConstructWidget<UVerticalBox>();
	Order->SetContent(OrderCol);
	AddV(OrderCol, Txt(WidgetTree, Upper(LOCTEXT("WorkOrder", "Ordem de serviço")), Oswald(20.f, false, 0.06f), Ink()));
	auto SummaryRow = [this, OrderCol](const FText& Name, UTextBlock*& OutValue)
	{
		OutValue = Txt(WidgetTree, FText::GetEmpty(), Mono(14.f, 600), Ink());
		AddV(OrderCol, SpaceBetween(WidgetTree, Txt(WidgetTree, Name, Mono(14.f), Ink()), OutValue), 12.f);
	};
	SummaryRow(LOCTEXT("Access", "Acesso"), SummaryAccess);
	SummaryRow(LOCTEXT("SlotsRow", "Vagas"), SummarySlots);
	SummaryRow(LOCTEXT("JoinRow", "Entrada no meio"), SummaryJoin);
	AddV(OrderCol, Dashed(WidgetTree, ManilaRule()), 12.f);
	AddV(OrderCol, Wrap(Txt(WidgetTree, LOCTEXT("HostNote", "Você será o host: a partida roda no seu PC. Convide amigos pelo overlay da Steam depois de criar."), Mono(13.f), Ink())), 10.f);
	AddV(Aside, Order);

	CreateButton = NewButton(WidgetTree, LOCTEXT("Create", "Criar sala"), ENoclipButtonKind::Primary, ENoclipButtonStyle::Menu);
	CreateButton->SetSuffix(FText::FromString(TEXT("→")));
	CreateButton->SetMinHeight(60.f);
	CreateButton->OnClicked().AddUObject(this, &UNoclipHostMenu::HandleCreate);
	AddV(Aside, CreateButton, 16.f);

	AddH(Body, Sized(WidgetTree, Aside, 380.f), 56.f, false, VAlign_Top);

	DefaultFocusWidget = CreateButton;
}

void UNoclipHostMenu::NativeOnActivated()
{
	bWaiting = false;
	if (UNoclipSessionSubsystem* Sessions = GetSessions(this))
	{
		Sessions->OnHostFinished.AddUniqueDynamic(this, &UNoclipHostMenu::HandleHostFinished);
		if (NameInput->GetText().IsEmpty())
		{
			NameInput->SetText(FText::FromString(Sessions->GetDefaultRoomName()));
		}
	}
	Refresh();
	Super::NativeOnActivated();
}

void UNoclipHostMenu::NativeOnDeactivated()
{
	if (UNoclipSessionSubsystem* Sessions = GetSessions(this))
	{
		Sessions->OnHostFinished.RemoveDynamic(this, &UNoclipHostMenu::HandleHostFinished);
	}
	Super::NativeOnDeactivated();
}

void UNoclipHostMenu::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	BackLink->SetSuffix(NoclipGlyphs::GetKeyText(ENoclipPrompt::Back, GetOwningPlayer()), true);
}

void UNoclipHostMenu::Refresh()
{
	for (int32 i = 0; i < PrivacyButtons.Num(); ++i)
	{
		PrivacyButtons[i]->SetOn(static_cast<int32>(Settings.Privacy) == i);
	}
	PlayersValue->SetText(FText::AsNumber(Settings.MaxPlayers));
	JoinCheck->SetOn(Settings.bJoinInProgress);

	const FText Titles[] = { LOCTEXT("Friends", "Só amigos"), LOCTEXT("Invite", "Só convite"), LOCTEXT("Public", "Pública") };
	SummaryAccess->SetText(Titles[FMath::Clamp(static_cast<int32>(Settings.Privacy), 0, 2)]);
	SummarySlots->SetText(FText::Format(LOCTEXT("SlotsFmt", "{0} estagiários"), FText::AsNumber(Settings.MaxPlayers)));
	SummaryJoin->SetText(Settings.bJoinInProgress ? LOCTEXT("Allowed", "Permitida") : LOCTEXT("Blocked", "Bloqueada"));
}

void UNoclipHostMenu::HandleCreate()
{
	UNoclipPrimaryLayout* Layout = GetLayout();
	UNoclipSessionSubsystem* Sessions = GetSessions(this);
	if (!Layout || !Sessions || Layout->IsBusy())
	{
		return;
	}
	Settings.RoomName = NameInput->GetText().ToString().TrimStartAndEnd().Left(32);
	bWaiting = true;
	Layout->SetBusy(LOCTEXT("Creating", "Criando sala…"));
	Sessions->HostRoom(Settings);
}

void UNoclipHostMenu::HandleHostFinished(bool bSuccess)
{
	if (!bWaiting)
	{
		return;
	}
	bWaiting = false;
	if (UNoclipPrimaryLayout* Layout = GetLayout())
	{
		if (bSuccess)
		{
			Layout->SetBusy(LOCTEXT("Traveling", "Abrindo a sala de espera…"));
		}
		else
		{
			Layout->SetBusy(FText::GetEmpty());
			Layout->ShowToast(LOCTEXT("HostFail", "Não foi possível criar a sala. Confira se a Steam está aberta e tente de novo."));
		}
	}
}

// =====================================================================  Entrar

void UNoclipJoinMenu::BuildTree()
{
	UVerticalBox* Screen = BuildPaddedScreen(WidgetTree, FMargin(80.f, 56.f), Bg());

	// header: titulo a esquerda, codigo de convite a direita (align-items: flex-end)
	UHorizontalBox* Header = WidgetTree->ConstructWidget<UHorizontalBox>();
	AddH(Header, BuildHeader(WidgetTree, LOCTEXT("BackHome", "Voltar ao início"), LOCTEXT("JoinTitle", "Entrar em expediente"), BackLink), 0.f, true, VAlign_Bottom);
	BackLink->OnClicked().AddWeakLambda(this, [this]() { HandleBack(); });
	UVerticalBox* CodeCol = WidgetTree->ConstructWidget<UVerticalBox>();
	AddV(CodeCol, Label(WidgetTree, LOCTEXT("InviteCode", "Código de convite")));
	CodeInput = Input(WidgetTree, LOCTEXT("CodeHint", "ex.: K7Q-2MX"));
	CodeInput->OnTextCommitted.AddDynamic(this, &UNoclipJoinMenu::HandleCodeCommitted);
	AddV(CodeCol, Sized(WidgetTree, CodeInput, 200.f, 48.f), 6.f);
	AddH(Header, CodeCol, 0.f, false, VAlign_Bottom);
	UNoclipButton* CodeJoin = NewButton(WidgetTree, LOCTEXT("Enter", "Entrar"), ENoclipButtonKind::Normal, ENoclipButtonStyle::Compact);
	CodeJoin->OnClicked().AddUObject(this, &UNoclipJoinMenu::HandleJoinCode);
	AddH(Header, CodeJoin, 10.f, false, VAlign_Bottom);
	AddV(Screen, Header);

	// busca + ocultar cheias + atualizar (gap 12)
	UHorizontalBox* SearchRow = WidgetTree->ConstructWidget<UHorizontalBox>();
	SearchInput = Input(WidgetTree, LOCTEXT("SearchHint", "Buscar por sala ou host"));
	SearchInput->OnTextChanged.AddDynamic(this, &UNoclipJoinMenu::HandleFilterChanged);
	AddH(SearchRow, Sized(WidgetTree, SearchInput, 0.f, 48.f), 0.f, true);
	HideFullCheck = NewButton(WidgetTree, LOCTEXT("HideFull", "Ocultar cheias"), ENoclipButtonKind::Normal, ENoclipButtonStyle::Check);
	HideFullCheck->SetFontPx(14.f);
	HideFullCheck->SetOn(true);
	HideFullCheck->OnClicked().AddWeakLambda(this, [this]() { HideFullCheck->SetOn(!HideFullCheck->IsOn()); RebuildRows(); });
	AddH(SearchRow, HideFullCheck, 20.f);
	RefreshButton = NewButton(WidgetTree, LOCTEXT("Refresh", "↻  Atualizar"), ENoclipButtonKind::Normal, ENoclipButtonStyle::Compact);
	RefreshButton->OnClicked().AddUObject(this, &UNoclipJoinMenu::Search);
	AddH(SearchRow, RefreshButton, 12.f);
	AddV(Screen, SearchRow, 24.f);

	// regiao da lista (flex 1)
	UOverlay* Region = WidgetTree->ConstructWidget<UOverlay>();
	UVerticalBoxSlot* RegionSlot = AddV(Screen, Region, 24.f);
	RegionSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	auto FillRegion = [Region](UWidget* W)
	{
		UOverlaySlot* S = Region->AddChildToOverlay(W);
		S->SetHorizontalAlignment(HAlign_Fill);
		S->SetVerticalAlignment(VAlign_Fill);
	};

	// Colunas: Sala (1fr) | Host 200 | Equipe 120 | Ping 110 | 40
	UVerticalBox* List = WidgetTree->ConstructWidget<UVerticalBox>();
	UHorizontalBox* Columns = WidgetTree->ConstructWidget<UHorizontalBox>();
	AddH(Columns, Label(WidgetTree, LOCTEXT("ColRoom", "Sala")), 0.f, true);
	AddH(Columns, Sized(WidgetTree, Label(WidgetTree, LOCTEXT("ColHost", "Host")), 200.f), 16.f);
	AddH(Columns, Sized(WidgetTree, Label(WidgetTree, LOCTEXT("ColCrew", "Equipe")), 120.f), 16.f);
	AddH(Columns, Sized(WidgetTree, Label(WidgetTree, LOCTEXT("ColPing", "Ping")), 110.f), 16.f);
	AddH(Columns, Gap(WidgetTree, 40.f, 1.f), 16.f);
	UVerticalBoxSlot* ColSlot = AddV(List, Columns);
	ColSlot->SetPadding(FMargin(18.f, 0.f));
	UScrollBox* Scroll = WidgetTree->ConstructWidget<UScrollBox>();
	Rows = WidgetTree->ConstructWidget<UVerticalBox>();
	Scroll->AddChild(Rows);
	UVerticalBoxSlot* ScrollSlot = AddV(List, Scroll, 8.f);
	ScrollSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	ListPanel = List;
	FillRegion(List);

	auto StatePanel = [this](const FLinearColor& Fill, const FLinearColor& Outline, UVerticalBox*& OutCol) -> UBorder*
	{
		UBorder* Panel = Box(WidgetTree, Fill, FMargin(24.f), Outline, 1.f);
		Panel->SetHorizontalAlignment(HAlign_Center);
		Panel->SetVerticalAlignment(VAlign_Center);
		OutCol = WidgetTree->ConstructWidget<UVerticalBox>();
		Panel->SetContent(OutCol);
		return Panel;
	};

	// Carregando
	UVerticalBox* LoadCol = nullptr;
	UBorder* Loading = StatePanel(FLinearColor::Transparent, Line(), LoadCol);
	UHorizontalBox* Dots = WidgetTree->ConstructWidget<UHorizontalBox>();
	for (int32 i = 0; i < 3; ++i)
	{
		UBorder* Dot = Box(WidgetTree, Amber(), FMargin(0.f));
		AddH(Dots, Sized(WidgetTree, Dot, 14.f, 14.f), i > 0 ? 8.f : 0.f);
		LoadingDots.Add(Dot);
	}
	AddV(LoadCol, Dots, 0.f, HAlign_Center);
	AddV(LoadCol, Txt(WidgetTree, LOCTEXT("Searching", "Procurando expedientes…"), Mono(16.f), Text()), 14.f, HAlign_Center);
	AddV(LoadCol, Txt(WidgetTree, LOCTEXT("MayTake", "Pode levar alguns segundos."), Mono(13.f), Muted()), 14.f, HAlign_Center);
	LoadingPanel = Loading;
	FillRegion(Loading);

	// Vazio
	UVerticalBox* EmptyCol = nullptr;
	UBorder* Empty = StatePanel(FLinearColor::Transparent, Line(), EmptyCol);
	AddV(EmptyCol, Txt(WidgetTree, Upper(LOCTEXT("NoRooms", "Nenhum expediente aberto")), Oswald(28.f, false), Amber()), 0.f, HAlign_Center);
	UTextBlock* EmptyBody = Wrap(Txt(WidgetTree, LOCTEXT("NoRoomsBody", "Peça um código de convite para quem vai hospedar, ou crie a sua própria sala."), Mono(14.f), Soft()));
	EmptyBody->SetJustification(ETextJustify::Center);
	AddV(EmptyCol, Sized(WidgetTree, EmptyBody, 520.f), 14.f, HAlign_Center);
	UHorizontalBox* EmptyButtons = WidgetTree->ConstructWidget<UHorizontalBox>();
	EmptyHost = NewButton(WidgetTree, LOCTEXT("HostIt", "Hospedar expediente"), ENoclipButtonKind::Primary, ENoclipButtonStyle::Compact);
	EmptyHost->OnClicked().AddWeakLambda(this, [this]()
	{
		if (UNoclipPrimaryLayout* L = GetLayout())
		{
			DeactivateWidget();
			L->PushMenu(UNoclipHostMenu::StaticClass());
		}
	});
	AddH(EmptyButtons, EmptyHost);
	UNoclipButton* EmptyRetry = NewButton(WidgetTree, LOCTEXT("SearchAgain", "Buscar de novo"), ENoclipButtonKind::Normal, ENoclipButtonStyle::Compact);
	EmptyRetry->OnClicked().AddUObject(this, &UNoclipJoinMenu::Search);
	AddH(EmptyButtons, EmptyRetry, 10.f);
	AddV(EmptyCol, EmptyButtons, 14.f, HAlign_Center);
	EmptyPanel = Empty;
	FillRegion(Empty);

	// Erro (role=alert)
	UVerticalBox* ErrCol = nullptr;
	UBorder* Error = StatePanel(Hex(TEXT("1E1310")), DangerLine(), ErrCol);
	ErrorTitle = Txt(WidgetTree, FText::GetEmpty(), Oswald(28.f, false), DangerText());
	AddV(ErrCol, ErrorTitle, 0.f, HAlign_Center);
	ErrorBody = Wrap(Txt(WidgetTree, FText::GetEmpty(), Mono(14.f), Hex(TEXT("E7C9C0"))));
	ErrorBody->SetJustification(ETextJustify::Center);
	AddV(ErrCol, Sized(WidgetTree, ErrorBody, 560.f), 14.f, HAlign_Center);
	ErrorRetry = NewButton(WidgetTree, LOCTEXT("TryAgain", "Tentar de novo"), ENoclipButtonKind::Normal, ENoclipButtonStyle::Compact);
	ErrorRetry->OnClicked().AddUObject(this, &UNoclipJoinMenu::Search);
	AddV(ErrCol, ErrorRetry, 14.f, HAlign_Center);
	ErrorPanel = Error;
	FillRegion(Error);

	DefaultFocusWidget = RefreshButton;
	SetView(EView::Loading);
}

void UNoclipJoinMenu::NativeOnActivated()
{
	if (UNoclipSessionSubsystem* Sessions = GetSessions(this))
	{
		Sessions->OnSessionsFound.AddUniqueDynamic(this, &UNoclipJoinMenu::HandleSessionsFound);
		Sessions->OnJoinFinished.AddUniqueDynamic(this, &UNoclipJoinMenu::HandleJoinFinished);
		const bool bSteam = Sessions->GetOnlineSubsystemName().Equals(TEXT("STEAM"), ESearchCase::IgnoreCase);
		ErrorTitle->SetText(Upper(bSteam ? LOCTEXT("NoSteam", "Sem conexão com a Steam") : LOCTEXT("NoNet", "Sem conexão com a rede")));
		ErrorBody->SetText(bSteam ? LOCTEXT("NoSteamBody", "Confira se o cliente da Steam está aberto e com login feito, e tente de novo.")
			: LOCTEXT("NoNetBody", "Confira a conexão de rede e tente de novo."));
	}
	Super::NativeOnActivated();
	Search();
}

void UNoclipJoinMenu::NativeOnDeactivated()
{
	if (UNoclipSessionSubsystem* Sessions = GetSessions(this))
	{
		Sessions->OnSessionsFound.RemoveDynamic(this, &UNoclipJoinMenu::HandleSessionsFound);
		Sessions->OnJoinFinished.RemoveDynamic(this, &UNoclipJoinMenu::HandleJoinFinished);
	}
	Super::NativeOnDeactivated();
}

void UNoclipJoinMenu::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	BackLink->SetSuffix(NoclipGlyphs::GetKeyText(ENoclipPrompt::Back, GetOwningPlayer()), true);
	if (bSearching)
	{
		// Os tres quadrados acendem em sequencia (ambar, ambar escuro, apagado).
		SearchTime += InDeltaTime;
		const int32 Lead = FMath::FloorToInt(SearchTime * 4.f) % 3;
		const FLinearColor Shades[] = { Amber(), Hex(TEXT("8C7A3C")), Line() };
		for (int32 i = 0; i < LoadingDots.Num(); ++i)
		{
			SetBox(Cast<UBorder>(LoadingDots[i]), Shades[(i - Lead + 3) % 3]);
		}
	}
}

void UNoclipJoinMenu::SetView(EView View)
{
	CurrentView = View;
	ListPanel->SetVisibility(View == EView::Loaded ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
	LoadingPanel->SetVisibility(View == EView::Loading ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
	EmptyPanel->SetVisibility(View == EView::Empty ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
	ErrorPanel->SetVisibility(View == EView::Error ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
}

void UNoclipJoinMenu::Search()
{
	UNoclipSessionSubsystem* Sessions = GetSessions(this);
	if (!Sessions || bSearching)
	{
		return;
	}
	bSearching = true;
	SearchTime = 0.f;
	SetView(EView::Loading);
	RefreshButton->SetFocus();
	Sessions->FindSessions();
}

void UNoclipJoinMenu::HandleSessionsFound(const TArray<FNoclipSessionInfo>& InFound)
{
	bSearching = false;
	Found = InFound;
	const UNoclipSessionSubsystem* Sessions = GetSessions(this);
	if (Sessions && Sessions->DidLastFindFail())
	{
		SetView(EView::Error);
		ErrorRetry->SetFocus();
		return;
	}
	RebuildRows();
}

void UNoclipJoinMenu::HandleFilterChanged(const FText& Text)
{
	if (!bSearching)
	{
		RebuildRows();
	}
}

void UNoclipJoinMenu::RebuildRows()
{
	Rows->ClearChildren();
	const FString Filter = SearchInput->GetText().ToString().TrimStartAndEnd();
	const bool bHideFull = HideFullCheck->IsOn();

	UNoclipButton* First = nullptr;
	for (const FNoclipSessionInfo& Info : Found)
	{
		const bool bFull = Info.MaxPlayers > 0 && Info.CurrentPlayers >= Info.MaxPlayers;
		if (bHideFull && bFull)
		{
			continue;
		}
		if (!Filter.IsEmpty() && !Info.RoomName.Contains(Filter) && !Info.OwnerName.Contains(Filter))
		{
			continue;
		}

		UNoclipButton* RowButton = NewButton(WidgetTree, FText::GetEmpty(), ENoclipButtonKind::Normal, ENoclipButtonStyle::Row);
		UHorizontalBox* Cells = WidgetTree->ConstructWidget<UHorizontalBox>();
		UTextBlock* Name = Txt(WidgetTree, ShortNameText(Info.RoomName, 32), Mono(15.f, 600), Text());
		Name->SetTextOverflowPolicy(ETextOverflowPolicy::Ellipsis);
		AddH(Cells, Name, 0.f, true);
		AddH(Cells, Sized(WidgetTree, Txt(WidgetTree, ShortNameText(Info.OwnerName, 18), Mono(15.f), Soft()), 200.f), 16.f);
		AddH(Cells, Sized(WidgetTree, Txt(WidgetTree, FText::Format(LOCTEXT("CrewFmt", "{0} / {1}"), FText::AsNumber(Info.CurrentPlayers), FText::AsNumber(Info.MaxPlayers)), Mono(15.f), Text()), 120.f), 16.f);
		UHorizontalBox* Ping = WidgetTree->ConstructWidget<UHorizontalBox>();
		AddH(Ping, Sized(WidgetTree, Box(WidgetTree, PingColor(Info.PingMs), FMargin(0.f)), 10.f, 10.f));
		AddH(Ping, Txt(WidgetTree, PingText(Info.PingMs), Mono(15.f), Text()), 8.f);
		AddH(Cells, Sized(WidgetTree, Ping, 110.f), 16.f);
		UTextBlock* Arrow = Txt(WidgetTree, FText::FromString(TEXT("→")), Mono(15.f), Text());
		Arrow->SetJustification(ETextJustify::Right);
		AddH(Cells, Sized(WidgetTree, Arrow, 40.f), 16.f);
		RowButton->SetRowContent(Cells);
		RowButton->SetIsEnabled(!bFull);
		const int32 Index = Info.Index;
		RowButton->OnClicked().AddWeakLambda(this, [this, Index]() { HandleJoinIndex(Index); });
		AddV(Rows, RowButton, 8.f);
		if (!First && !bFull)
		{
			First = RowButton;
		}
	}

	if (Rows->GetChildrenCount() == 0)
	{
		SetView(EView::Empty);
		if (EmptyHost)
		{
			EmptyHost->SetFocus();
		}
		return;
	}
	SetView(EView::Loaded);
	if (First && NoclipGlyphs::IsUsingGamepad(GetOwningPlayer()))
	{
		First->SetFocus();
	}
}

void UNoclipJoinMenu::HandleJoinIndex(int32 Index)
{
	UNoclipPrimaryLayout* Layout = GetLayout();
	UNoclipSessionSubsystem* Sessions = GetSessions(this);
	if (!Layout || !Sessions || Layout->IsBusy())
	{
		return;
	}
	Layout->SetBusy(LOCTEXT("Joining", "Entrando no expediente…"));
	Sessions->JoinSessionByIndex(Index);
}

void UNoclipJoinMenu::HandleCodeCommitted(const FText& Text, ETextCommit::Type CommitMethod)
{
	if (CommitMethod == ETextCommit::OnEnter)
	{
		HandleJoinCode();
	}
}

void UNoclipJoinMenu::HandleJoinCode()
{
	UNoclipPrimaryLayout* Layout = GetLayout();
	UNoclipSessionSubsystem* Sessions = GetSessions(this);
	const FString Code = CodeInput->GetText().ToString().TrimStartAndEnd();
	if (!Layout || !Sessions || Layout->IsBusy())
	{
		return;
	}
	if (Code.Len() < 6)
	{
		Layout->ShowToast(LOCTEXT("CodeShort", "O código tem 6 caracteres, como K7Q-2MX."));
		CodeInput->SetKeyboardFocus();
		return;
	}
	Layout->SetBusy(LOCTEXT("JoiningCode", "Procurando a sala do código…"));
	Sessions->JoinByCode(Code);
}

void UNoclipJoinMenu::HandleJoinFinished(bool bSuccess)
{
	UNoclipPrimaryLayout* Layout = GetLayout();
	if (!Layout)
	{
		return;
	}
	if (bSuccess)
	{
		Layout->SetBusy(LOCTEXT("JoinTravel", "Descendo para o Level 0…"));
		return;
	}
	Layout->SetBusy(FText::GetEmpty());
	FText Error;
	if (UNoclipSessionSubsystem* Sessions = GetSessions(this))
	{
		Error = Sessions->ConsumePendingError();
	}
	Layout->ShowToast(Error.IsEmpty() ? LOCTEXT("JoinFail", "Não foi possível entrar. A sala pode ter fechado ou enchido.") : Error);
}

// =====================================================================  Opcoes

namespace
{
	const FText& TabName(int32 Index)
	{
		static const FText Names[] = { LOCTEXT("TabVideo", "Vídeo"), LOCTEXT("TabAudio", "Áudio"), LOCTEXT("TabControls", "Controles"), LOCTEXT("TabAccess", "Acessibilidade") };
		return Names[FMath::Clamp(Index, 0, 3)];
	}
}

void UNoclipOptionsMenu::BuildTree()
{
	UVerticalBox* Screen = BuildPaddedScreen(WidgetTree, FMargin(80.f, 56.f, 80.f, 40.f), Bg());
	AddV(Screen, BuildHeader(WidgetTree, LOCTEXT("Back", "Voltar"), LOCTEXT("OptTitle", "Opções"), BackLink));
	BackLink->OnClicked().AddWeakLambda(this, [this]() { HandleBack(); });

	UHorizontalBox* Body = WidgetTree->ConstructWidget<UHorizontalBox>();
	UVerticalBoxSlot* BodySlot = AddV(Screen, Body, 24.f);
	BodySlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));

	// nav 240, gap 4
	UVerticalBox* Nav = WidgetTree->ConstructWidget<UVerticalBox>();
	for (int32 i = 0; i < 4; ++i)
	{
		UNoclipButton* Tab = NewButton(WidgetTree, TabName(i), ENoclipButtonKind::Normal, ENoclipButtonStyle::Tab);
		Tab->OnClicked().AddWeakLambda(this, [this, i]() { SelectTab(i, false); });
		AddV(Nav, Tab, 4.f);
		Tabs.Add(Tab);
	}
	UHorizontalBox* TabHint = WidgetTree->ConstructWidget<UHorizontalBox>();
	AddH(TabHint, Txt(WidgetTree, LOCTEXT("ChangesNow", "Mudanças valem na hora."), Mono(12.f), Muted()));
	AddV(Nav, TabHint, 16.f);
	UHorizontalBox* TabKeys = WidgetTree->ConstructWidget<UHorizontalBox>();
	AddH(TabKeys, Key(WidgetTree, KeyPrev));
	AddH(TabKeys, Key(WidgetTree, KeyNext), 6.f);
	AddH(TabKeys, Txt(WidgetTree, LOCTEXT("SwitchTabs", "trocam de seção."), Mono(12.f), Muted()), 6.f);
	AddV(Nav, TabKeys, 6.f);
	AddH(Body, Sized(WidgetTree, Nav, 240.f), 0.f, false, VAlign_Top);

	Section = WidgetTree->ConstructWidget<UScrollBox>();
	AddH(Body, Section, 40.f, true, VAlign_Fill);

	// footer: restaurar + pronto (justify-content: flex-end, gap 12)
	UHorizontalBox* Footer = WidgetTree->ConstructWidget<UHorizontalBox>();
	AddH(Footer, Gap(WidgetTree, 1.f, 1.f), 0.f, true);
	UNoclipButton* Reset = NewButton(WidgetTree, LOCTEXT("ResetSection", "Restaurar padrão da seção"), ENoclipButtonKind::Normal, ENoclipButtonStyle::Compact);
	Reset->OnClicked().AddWeakLambda(this, [this]()
	{
		FNoclipUserSettings::Get().ResetSection(static_cast<ENoclipSettingsSection>(TabIndex));
		Changed();
		BuildSection();
	});
	AddH(Footer, Reset);
	UNoclipButton* Done = NewButton(WidgetTree, LOCTEXT("Done", "Pronto"), ENoclipButtonKind::Primary, ENoclipButtonStyle::Compact);
	Done->OnClicked().AddWeakLambda(this, [this]() { HandleBack(); });
	AddH(Footer, Done, 12.f);
	AddV(Screen, Footer, 24.f);

	SelectTab(0, false);
	DefaultFocusWidget = Tabs[0];
}

void UNoclipOptionsMenu::NativeOnActivated()
{
	Super::NativeOnActivated();
	const bool bPad = NoclipGlyphs::IsUsingGamepad(GetOwningPlayer());
	KeyPrev->SetText(FText::FromString(bPad ? (NoclipGlyphs::GetPadStyle(GetOwningPlayer()) == ENoclipPadStyle::PlayStation ? TEXT("L1") : TEXT("LB")) : TEXT("Q")));
	KeyNext->SetText(FText::FromString(bPad ? (NoclipGlyphs::GetPadStyle(GetOwningPlayer()) == ENoclipPadStyle::PlayStation ? TEXT("R1") : TEXT("RB")) : TEXT("E")));
	BackLink->SetSuffix(NoclipGlyphs::GetKeyText(ENoclipPrompt::Back, GetOwningPlayer()), true);
}

FReply UNoclipOptionsMenu::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	const FKey Key = InKeyEvent.GetKey();
	if (Key == EKeys::Q || Key == EKeys::Gamepad_LeftShoulder)
	{
		SelectTab((TabIndex + 3) % 4, true);
		return FReply::Handled();
	}
	if (Key == EKeys::E || Key == EKeys::Gamepad_RightShoulder)
	{
		SelectTab((TabIndex + 1) % 4, true);
		return FReply::Handled();
	}
	// Selecoes: ‹ › trocam o valor sem precisar abrir lista.
	if (IsLeftKey(Key) || IsRightKey(Key))
	{
		for (UNoclipButton* Select : Selects)
		{
			if (Select && Select->IsShowingFocus())
			{
				if (TFunction<void(int32)>* Step = SelectStep.Find(Select))
				{
					(*Step)(IsRightKey(Key) ? +1 : -1);
					return FReply::Handled();
				}
			}
		}
	}
	return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

void UNoclipOptionsMenu::SelectTab(int32 Index, bool bFocusTab)
{
	TabIndex = FMath::Clamp(Index, 0, 3);
	for (int32 i = 0; i < Tabs.Num(); ++i)
	{
		Tabs[i]->SetOn(i == TabIndex);
	}
	BuildSection();
	if (bFocusTab)
	{
		Tabs[TabIndex]->SetFocus();
	}
}

void UNoclipOptionsMenu::Changed()
{
	FNoclipUserSettings& S = FNoclipUserSettings::Get();
	S.Save();
	S.ApplyToWorld(GetWorld());
	if (UNoclipAudioDeviceSubsystem* Devices = UNoclipAudioDeviceSubsystem::Get(this))
	{
		Devices->ApplySaved();
	}
}

void UNoclipOptionsMenu::AddAudioDeviceRows()
{
	UNoclipAudioDeviceSubsystem* Devices = UNoclipAudioDeviceSubsystem::Get(this);
	if (!Devices)
	{
		return;
	}
	// Liga uma vez: se a lista chegar (ou um fone for plugado) com a aba de audio aberta, refaz a secao.
	if (!AudioDevicesHandle.IsValid())
	{
		AudioDevicesHandle = Devices->OnDevicesChanged.AddWeakLambda(this, [this]()
		{
			// No proximo quadro (pode chegar no meio do BuildSection) e mesmo com o jogo pausado.
			FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateWeakLambda(this, [this](float)
			{
				if (TabIndex == 1 && IsActivated())
				{
					BuildSection();
				}
				return false;
			}));
		});
	}

	auto Short = [](const FString& Name) { return FText::FromString(Name.Len() > 34 ? Name.Left(33) + TEXT("…") : Name); };
	auto MakeDeviceSelect = [this, &Short](const TArray<FNoclipAudioDevice>& List, const FText& DefaultLabel, FString FNoclipUserSettings::* Field, bool bOutput)
	{
		TArray<FText> Options = { DefaultLabel };
		TArray<FString> Ids = { FString() };
		for (const FNoclipAudioDevice& D : List)
		{
			Options.Add(Short(D.Name));
			Ids.Add(D.Id);
		}
		return MakeSelect(Options,
			[Ids, Field]()
			{
				const int32 Index = Ids.IndexOfByKey(FNoclipUserSettings::Get().*Field);
				return Index == INDEX_NONE ? 0 : Index;
			},
			[this, Ids, bOutput](int32 V)
			{
				UNoclipAudioDeviceSubsystem* Sub = UNoclipAudioDeviceSubsystem::Get(this);
				if (Sub && Ids.IsValidIndex(V))
				{
					bOutput ? Sub->SetOutputDevice(Ids[V]) : Sub->SetInputDevice(Ids[V]);
				}
			});
	};

	const FNoclipAudioDevice* DefaultOut = Devices->GetOutputDevices().FindByPredicate([](const FNoclipAudioDevice& D) { return D.bSystemDefault; });
	const FText DefaultOutLabel = DefaultOut
		? FText::Format(LOCTEXT("DefaultDeviceFmt", "Padrão ({0})"), Short(DefaultOut->Name))
		: LOCTEXT("DefaultDevice", "Padrão do sistema");
	AddRow(LOCTEXT("OutputDevice", "Saída de áudio"), LOCTEXT("OutputDeviceDesc", "Fone ou caixa de som. Se o escolhido for desconectado, volta para o padrão."),
		MakeDeviceSelect(Devices->GetOutputDevices(), DefaultOutLabel, &FNoclipUserSettings::OutputDevice, true));
	AddRow(LOCTEXT("InputDevice", "Microfone"), LOCTEXT("InputDeviceDesc", "Usado no chat de voz. Troca na hora, até no meio do turno."),
		MakeDeviceSelect(Devices->GetInputDevices(), LOCTEXT("DefaultDevice", "Padrão do sistema"), &FNoclipUserSettings::InputDevice, false));
}

UWidget* UNoclipOptionsMenu::AddRow(const FText& Title, const FText& Desc, UWidget* Control)
{
	// .set { grid: 1fr 300px; gap 24; padding 14px 0; border-bottom 1px #2A2519 }
	UVerticalBox* Wrapper = WidgetTree->ConstructWidget<UVerticalBox>();
	UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>();
	UVerticalBox* Text = WidgetTree->ConstructWidget<UVerticalBox>();
	AddV(Text, Txt(WidgetTree, Title, Mono(16.f), NoclipUI::Text()));
	if (!Desc.IsEmpty())
	{
		AddV(Text, Wrap(Txt(WidgetTree, Desc, Mono(13.f), Muted()), 1.45f), 4.f);
	}
	AddH(Row, Text, 0.f, true);
	AddH(Row, Sized(WidgetTree, Control, 300.f), 24.f);
	UVerticalBoxSlot* RowSlot = AddV(Wrapper, Row);
	RowSlot->SetPadding(FMargin(0.f, 14.f));
	AddV(Wrapper, Rule(WidgetTree, Line2()));
	Section->AddChild(Wrapper);
	return Wrapper;
}

UNoclipButton* UNoclipOptionsMenu::MakeSelect(TArray<FText> Options, TFunction<int32()> Get, TFunction<void(int32)> Set)
{
	UNoclipButton* Select = NewButton(WidgetTree, Options.IsValidIndex(Get()) ? Options[Get()] : FText::GetEmpty(), ENoclipButtonKind::Normal, ENoclipButtonStyle::Select);
	TFunction<void(int32)> Step = [this, Select, Options, Get, Set](int32 Delta)
	{
		const int32 Count = Options.Num();
		const int32 Next = (Get() + Delta + Count) % Count;
		Set(Next);
		Select->SetValueText(Options[Next]);
		Changed();
	};
	Select->OnClicked().AddWeakLambda(this, [Step]() { Step(+1); });
	Selects.Add(Select);
	SelectStep.Add(Select, Step);
	return Select;
}

UNoclipButton* UNoclipOptionsMenu::MakeCheck(TFunction<bool()> Get, TFunction<void(bool)> Set)
{
	UNoclipButton* Check = NewButton(WidgetTree, LOCTEXT("On", "Ligado"), ENoclipButtonKind::Normal, ENoclipButtonStyle::Check);
	Check->SetOn(Get());
	Check->OnClicked().AddWeakLambda(this, [this, Check, Get, Set]()
	{
		Set(!Get());
		Check->SetOn(Get());
		Changed();
	});
	return Check;
}

UNoclipSlider* UNoclipOptionsMenu::MakeSlider(float Min, float Max, float Step, float Value, TFunction<void(float)> Set)
{
	UNoclipSlider* Slider = WidgetTree->ConstructWidget<UNoclipSlider>(UNoclipSlider::StaticClass());
	Slider->Setup(Min, Max, Step, Value, [this, Set](float V) { Set(V); Changed(); });
	return Slider;
}

void UNoclipOptionsMenu::BuildSection()
{
	Section->ClearChildren();
	Selects.Reset();
	SelectStep.Reset();
	FNoclipUserSettings& S = FNoclipUserSettings::Get();

	switch (TabIndex)
	{
	case 0: // Video
		AddRow(LOCTEXT("PSXRes", "Resolução interna PSX"), LOCTEXT("PSXResDesc", "Linhas verticais da imagem. Menos linhas = mais \"1998\"."),
			MakeSelect({ LOCTEXT("Lines240", "240 linhas (padrão)"), LOCTEXT("Lines360", "360 linhas"), LOCTEXT("Lines480", "480 linhas"), LOCTEXT("Native", "Nativa (sem filtro)") },
				[&S]() { return S.PSXLines; }, [&S](int32 V) { S.PSXLines = V; }));
		AddRow(LOCTEXT("Dither", "Força do dithering"), LOCTEXT("DitherDesc", "Padrão de pontinhos nas transições de cor."),
			MakeSlider(0.f, 100.f, 1.f, S.Dither, [&S](float V) { S.Dither = FMath::RoundToInt(V); }));
		AddRow(LOCTEXT("Wobble", "Tremido dos vértices"), LOCTEXT("WobbleDesc", "Geometria \"pulando\" como no PS1."),
			MakeCheck([&S]() { return S.bVertexWobble; }, [&S](bool B) { S.bVertexWobble = B; }));
		AddRow(LOCTEXT("FOV", "Campo de visão"), LOCTEXT("FOVDesc", "De 70° a 110°. Correr abre +8°."),
			MakeSlider(70.f, 110.f, 1.f, S.FieldOfView, [&S](float V) { S.FieldOfView = FMath::RoundToInt(V); }));
		AddRow(LOCTEXT("FPS", "Limite de FPS"), FText::GetEmpty(),
			MakeSelect({ FText::FromString(TEXT("60")), FText::FromString(TEXT("120")), FText::FromString(TEXT("144")), LOCTEXT("NoLimit", "Sem limite") },
				[&S]() { return S.FPSLimit; }, [&S](int32 V) { S.FPSLimit = V; }));
		AddRow(LOCTEXT("Scan", "Linhas de varredura"), LOCTEXT("ScanDesc", "Efeito de TV de tubo por cima da interface."),
			MakeCheck([&S]() { return S.bScanlines; }, [&S](bool B) { S.bScanlines = B; }));
		break;

	case 1: // Audio
		if (UNoclipAudioDeviceSubsystem* Devices = UNoclipAudioDeviceSubsystem::Get(this))
		{
			Devices->RefreshDevices(); // fone plugado agora aparece (a lista de saida chega em seguida)
		}
		AddAudioDeviceRows();
		AddRow(LOCTEXT("Master", "Volume geral"), FText::GetEmpty(),
			MakeSlider(0.f, 100.f, 1.f, S.MasterVolume, [&S](float V) { S.MasterVolume = FMath::RoundToInt(V); }));
		AddRow(LOCTEXT("Music", "Música"), LOCTEXT("MusicDesc", "Trilha que acompanha o expediente: some e volta conforme o perigo."),
			MakeSlider(0.f, 100.f, 1.f, S.MusicVolume, [&S](float V) { S.MusicVolume = FMath::RoundToInt(V); }));
		AddRow(LOCTEXT("Voice", "Voz dos colegas"), LOCTEXT("VoiceDesc", "Voz por proximidade (some a ~20 m). Longe? Levantem o celular juntos."),
			MakeSlider(0.f, 100.f, 1.f, S.VoiceVolume, [&S](float V) { S.VoiceVolume = FMath::RoundToInt(V); }));
		AddRow(LOCTEXT("MicMode", "Modo do microfone"), LOCTEXT("MicModeDesc", "Lembre: as entidades ouvem acima de certo volume."),
			MakeSelect({ LOCTEXT("VAD", "Detecção de voz"), LOCTEXT("PTT", "Apertar para falar (T / RB)") },
				[&S]() { return S.MicMode; }, [&S](int32 V) { S.MicMode = V; }));
		AddRow(LOCTEXT("MicSens", "Sensibilidade do microfone"), LOCTEXT("MicSensDesc", "Fale normalmente: a barra deve passar da marca."),
			MakeSlider(0.f, 100.f, 1.f, S.MicSensitivity, [&S](float V) { S.MicSensitivity = FMath::RoundToInt(V); }));
		break;

	case 2: // Controles
	{
		AddRow(LOCTEXT("MouseSens", "Sensibilidade do mouse"), FText::GetEmpty(),
			MakeSlider(FNoclipUserSettings::MinSensitivity, FNoclipUserSettings::MaxSensitivity, 0.05f, S.MouseSensitivity, [&S](float V) { S.MouseSensitivity = V; }));
		AddRow(LOCTEXT("StickSens", "Sensibilidade do analógico"), FText::GetEmpty(),
			MakeSlider(FNoclipUserSettings::MinSensitivity, FNoclipUserSettings::MaxSensitivity, 0.05f, S.StickSensitivity, [&S](float V) { S.StickSensitivity = V; }));
		AddRow(LOCTEXT("InvertY", "Inverter eixo vertical"), FText::GetEmpty(),
			MakeCheck([&S]() { return S.bInvertY; }, [&S](bool B) { S.bInvertY = B; }));
		AddRow(LOCTEXT("Rumble", "Vibração do controle"), FText::GetEmpty(),
			MakeCheck([&S]() { return S.bRumble; }, [&S](bool B) { S.bRumble = B; }));
		AddRow(LOCTEXT("Glyphs", "Ícones do controle"), LOCTEXT("GlyphsDesc", "No PC, o controle de PlayStation costuma aparecer como Xbox."),
			MakeSelect({ LOCTEXT("Auto", "Automático"), FText::FromString(TEXT("Xbox")), FText::FromString(TEXT("PlayStation")) },
				[&S]() { return S.PadGlyphs; }, [&S](int32 V) { S.PadGlyphs = V; }));

		const APlayerController* PC = GetOwningPlayer();
		const bool bPad = NoclipGlyphs::IsUsingGamepad(PC);
		struct FBinding { FText Action; FText Key; };
		const TArray<FBinding> Bindings = {
			{ LOCTEXT("BWalk", "Andar"), bPad ? LOCTEXT("LS", "Analógico esq.") : FText::FromString(TEXT("WASD")) },
			{ LOCTEXT("BSprint", "Correr"), NoclipGlyphs::GetKeyText(ENoclipPrompt::Sprint, PC) },
			{ LOCTEXT("BJump", "Pular"), NoclipGlyphs::GetKeyText(ENoclipPrompt::Jump, PC) },
			{ LOCTEXT("BGrab", "Agarrar / soltar"), NoclipGlyphs::GetKeyText(ENoclipPrompt::Grab, PC) },
			{ LOCTEXT("BThrow", "Arremessar (segurar)"), NoclipGlyphs::GetKeyText(ENoclipPrompt::Throw, PC) },
			{ LOCTEXT("BRadio", "Falar no rádio"), bPad ? FText::FromString(TEXT("—")) : FText::FromString(TEXT("T")) },
			{ LOCTEXT("BMark", "Marcar local"), bPad ? FText::FromString(TEXT("—")) : FText::FromString(TEXT("Mouse 3")) },
		};
		for (const FBinding& B : Bindings)
		{
			UNoclipButton* Rebind = NewButton(WidgetTree, LOCTEXT("Rebind", "Trocar"), ENoclipButtonKind::Normal, ENoclipButtonStyle::Row);
			UHorizontalBox* Cells = WidgetTree->ConstructWidget<UHorizontalBox>();
			UTextBlock* KeyText = nullptr;
			AddH(Cells, Key(WidgetTree, KeyText));
			KeyText->SetText(B.Key);
			AddH(Cells, Gap(WidgetTree, 1.f, 1.f), 0.f, true);
			AddH(Cells, Txt(WidgetTree, LOCTEXT("Rebind", "Trocar"), Mono(15.f), Text()));
			Rebind->SetRowContent(Cells);
			Rebind->SetMinHeight(48.f);
			Rebind->OnClicked().AddWeakLambda(this, [this]()
			{
				if (UNoclipPrimaryLayout* L = GetLayout())
				{
					L->ShowToast(LOCTEXT("RebindSoon", "Trocar teclas chega numa próxima versão."), false);
				}
			});
			AddRow(B.Action, FText::GetEmpty(), Rebind);
		}
		break;
	}

	default: // Acessibilidade
		AddRow(LOCTEXT("Flash", "Reduzir luzes piscando"), LOCTEXT("FlashDesc", "Apagões e pisca-pisca viram escurecimento suave. Recomendado para fotossensibilidade."),
			MakeCheck([&S]() { return S.bReduceFlashing; }, [&S](bool B) { S.bReduceFlashing = B; }));
		AddRow(LOCTEXT("Bob", "Balanço da câmera"), LOCTEXT("BobDesc", "Passos, pouso e inclinação lateral."),
			MakeSlider(0.f, 100.f, 1.f, S.CameraBob, [&S](float V) { S.CameraBob = FMath::RoundToInt(V); }));
		AddRow(LOCTEXT("TextSize", "Tamanho do texto da interface"), FText::GetEmpty(),
			MakeSelect({ FText::FromString(TEXT("100%")), FText::FromString(TEXT("125%")), FText::FromString(TEXT("150%")) },
				[&S]() { return S.TextScale; }, [&S](int32 V) { S.TextScale = V; }));
		AddRow(LOCTEXT("Colorblind", "Cores para daltonismo"), LOCTEXT("ColorblindDesc", "Troca verde/vermelho por azul/laranja no HUD."),
			MakeSelect({ LOCTEXT("Off", "Desligado"), LOCTEXT("Protan", "Protanopia"), LOCTEXT("Deutan", "Deuteranopia"), LOCTEXT("Tritan", "Tritanopia") },
				[&S]() { return S.Colorblind; }, [&S](int32 V) { S.Colorblind = V; }));
		AddRow(LOCTEXT("ShowValue", "Mostrar valor ao mirar"), LOCTEXT("ShowValueDesc", "Exibe o preço do achado sem precisar pegar."),
			MakeCheck([&S]() { return S.bShowValueOnAim; }, [&S](bool B) { S.bShowValueOnAim = B; }));
		break;
	}
}

#undef LOCTEXT_NAMESPACE
