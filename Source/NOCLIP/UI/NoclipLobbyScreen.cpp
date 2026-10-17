// Copyright Fuly Games. All Rights Reserved.

#include "UI/NoclipLobbyScreen.h"
#include "UI/NoclipButton.h"
#include "UI/NoclipUIStyle.h"
#include "UI/NoclipInputGlyphs.h"
#include "UI/NoclipPrimaryLayout.h"
#include "UI/NoclipPauseMenu.h"
#include "Game/NoclipGameState.h"
#include "Game/NoclipGameMode.h"
#include "Game/NoclipPlayerState.h"
#include "Game/NoclipPlayerController.h"
#include "Game/NoclipShop.h"
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
#include "Components/UniformGridPanel.h"
#include "Components/UniformGridSlot.h"
#include "Components/ScrollBox.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(NoclipLobbyScreen)

#define LOCTEXT_NAMESPACE "NoclipLobby"

using namespace NoclipUI;

namespace
{
	UNoclipButton* NewButton(UWidgetTree* Tree, const FText& Label, ENoclipButtonKind Kind)
	{
		UNoclipButton* Button = Tree->ConstructWidget<UNoclipButton>(UNoclipButton::StaticClass());
		Button->Configure(Label, Kind, ENoclipButtonStyle::Compact);
		return Button;
	}

	UBorder* PanelBox(UWidgetTree* Tree)
	{
		return Box(Tree, Panel(), FMargin(18.f), Line(), 1.f);
	}

	UVerticalBox* Stat(UWidgetTree* Tree, const FText& Name, UTextBlock*& OutValue, const FSlateFontInfo& Font)
	{
		UVerticalBox* Col = Tree->ConstructWidget<UVerticalBox>();
		AddV(Col, Txt(Tree, Upper(Name), Mono(12.f, 400, 0.1f), Ink()));
		OutValue = Txt(Tree, FText::GetEmpty(), Font, Ink());
		AddV(Col, OutValue);
		return Col;
	}
}

void UNoclipLobbyScreen::BuildTree()
{
	bClosesOnBack = false;

	UOverlay* Root = WidgetTree->ConstructWidget<UOverlay>();
	WidgetTree->RootWidget = Root;
	UOverlaySlot* BackSlot = Root->AddChildToOverlay(Box(WidgetTree, Bg(), FMargin(0.f)));
	BackSlot->SetHorizontalAlignment(HAlign_Fill);
	BackSlot->SetVerticalAlignment(VAlign_Fill);

	// padding 40 56, gap 20
	UVerticalBox* Screen = WidgetTree->ConstructWidget<UVerticalBox>();
	UOverlaySlot* ScreenSlot = Root->AddChildToOverlay(Screen);
	ScreenSlot->SetHorizontalAlignment(HAlign_Fill);
	ScreenSlot->SetVerticalAlignment(VAlign_Fill);
	ScreenSlot->SetPadding(FMargin(56.f, 40.f));

	// ---------- header ----------
	UHorizontalBox* Header = WidgetTree->ConstructWidget<UHorizontalBox>();
	UVerticalBox* TitleCol = WidgetTree->ConstructWidget<UVerticalBox>();
	RoomLabel = Label(WidgetTree, FText::GetEmpty());
	AddV(TitleCol, RoomLabel);
	Title = H1(WidgetTree, FText::GetEmpty(), 48.f, Amber(), 0.f);
	AddV(TitleCol, Title, 4.f);
	AddH(Header, TitleCol, 0.f, true, VAlign_Bottom);

	UNoclipButton* Invite = NewButton(WidgetTree, LOCTEXT("Invite", "Convidar pela Steam"), ENoclipButtonKind::Normal);
	Invite->OnClicked().AddWeakLambda(this, [this]()
	{
		ANoclipPlayerController* PC = GetNoclipPC();
		if (!PC || !PC->ShowInviteOverlay())
		{
			if (UNoclipPrimaryLayout* L = GetLayout())
			{
				const ANoclipGameState* GS = GetNoclipGameState();
				L->ShowToast(FText::Format(LOCTEXT("NoOverlay", "Overlay da Steam indisponível. Passe o código {0} para os colegas."), FText::FromString(GS ? GS->GetInviteCode() : FString())), false);
			}
		}
	});
	AddH(Header, Invite, 0.f, false, VAlign_Bottom);
	MenuButton = NewButton(WidgetTree, LOCTEXT("Menu", "Menu"), ENoclipButtonKind::Normal);
	MenuButton->OnClicked().AddWeakLambda(this, [this]() { HandleBack(); });
	AddH(Header, MenuButton, 10.f, false, VAlign_Bottom);
	AddV(Screen, Header);

	// ---------- grid: 300 | 1fr | 420, gap 20 ----------
	UHorizontalBox* Grid = WidgetTree->ConstructWidget<UHorizontalBox>();
	UVerticalBoxSlot* GridSlot = AddV(Screen, Grid, 20.f);
	GridSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));

	// Equipe
	UBorder* CrewPanel = PanelBox(WidgetTree);
	UVerticalBox* CrewCol = WidgetTree->ConstructWidget<UVerticalBox>();
	CrewPanel->SetContent(CrewCol);
	CrewTitle = Label(WidgetTree, FText::GetEmpty());
	AddV(CrewCol, CrewTitle);
	CrewList = WidgetTree->ConstructWidget<UVerticalBox>();
	AddV(CrewCol, CrewList, 12.f);
	UVerticalBoxSlot* SpacerSlot = AddV(CrewCol, Gap(WidgetTree, 1.f, 1.f));
	SpacerSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	// Duas linhas: cabe na coluna de 300 (a tecla nao quebra junto com o texto).
	AddV(CrewCol, Wrap(Txt(WidgetTree, LOCTEXT("MicHint", "Quadrado verde = falando."), Mono(12.f), Muted()), 1.4f));
	UHorizontalBox* RadioHint = WidgetTree->ConstructWidget<UHorizontalBox>();
	AddH(RadioHint, Key(WidgetTree, RadioKey));
	RadioKey->SetText(FText::FromString(TEXT("T")));
	AddH(RadioHint, Txt(WidgetTree, LOCTEXT("Radio", "fala no rádio."), Mono(12.f), Muted()), 8.f);
	AddV(CrewCol, RadioHint, 6.f);
	AddH(Grid, Sized(WidgetTree, CrewPanel, 300.f), 0.f, false, VAlign_Fill);

	// Ordem de servico + iniciar
	UVerticalBox* Center = WidgetTree->ConstructWidget<UVerticalBox>();
	UBorder* Order = Box(WidgetTree, Manila(), FMargin(22.f));
	UVerticalBox* OrderCol = WidgetTree->ConstructWidget<UVerticalBox>();
	Order->SetContent(OrderCol);
	OrderTitle = Txt(WidgetTree, FText::GetEmpty(), Oswald(22.f, false, 0.06f), Ink());
	AddV(OrderCol, OrderTitle);
	UUniformGridPanel* Stats = WidgetTree->ConstructWidget<UUniformGridPanel>();
	Stats->SetSlotPadding(FMargin(0.f, 0.f, 14.f, 14.f));
	UTextBlock* Dest = nullptr;
	UTextBlock* Exit = nullptr;
	AddCell(Stats, Stat(WidgetTree, LOCTEXT("Quota", "Quota"), QuotaValue, Oswald(44.f, false)), 0, 0);
	AddCell(Stats, Stat(WidgetTree, LOCTEXT("Duration", "Duração"), DurationValue, Oswald(44.f, false)), 0, 1);
	AddCell(Stats, Stat(WidgetTree, LOCTEXT("Destination", "Destino"), Dest, Mono(16.f, 600)), 1, 0);
	AddCell(Stats, Stat(WidgetTree, LOCTEXT("Exit", "Saída"), Exit, Mono(16.f, 600)), 1, 1);
	Dest->SetText(LOCTEXT("DestValue", "Level 0 · planta sorteada"));
	Wrap(Dest, 1.3f);
	Wrap(Exit, 1.3f);
	Exit->SetText(LOCTEXT("ExitValue", "Sala Manila"));
	AddV(OrderCol, Stats, 14.f);
	AddV(OrderCol, Dashed(WidgetTree, ManilaRule()));
	AddV(OrderCol, Wrap(Txt(WidgetTree, LOCTEXT("OrderNote", "Leve os achados para a Sala Manila. Fiquem todos lá dentro por 5 s para o elevador subir antes do fim."), Mono(13.f), Ink())), 12.f);
	AddV(Center, Order);
	UVerticalBoxSlot* PushDown = AddV(Center, Gap(WidgetTree, 1.f, 1.f));
	PushDown->SetSize(FSlateChildSize(ESlateSizeRule::Fill));

	ReadyButton = NewButton(WidgetTree, LOCTEXT("MarkReady", "Marcar como pronto"), ENoclipButtonKind::Normal);
	ReadyButton->OnClicked().AddWeakLambda(this, [this]()
	{
		const ANoclipPlayerState* PS = GetOwningPlayerState<ANoclipPlayerState>();
		if (ANoclipPlayerController* PC = GetNoclipPC())
		{
			PC->RequestSetReady(!(PS && PS->IsReady()));
		}
	});
	AddV(Center, ReadyButton, 10.f);
	StartButton = NewButton(WidgetTree, LOCTEXT("Start", "Iniciar expediente"), ENoclipButtonKind::Primary);
	StartButton->Configure(LOCTEXT("Start", "Iniciar expediente"), ENoclipButtonKind::Primary, ENoclipButtonStyle::Compact);
	StartButton->SetFontPx(24.f);
	StartButton->SetSuffix(FText::FromString(TEXT("→")));
	StartButton->SetMinHeight(60.f);
	StartButton->OnClicked().AddWeakLambda(this, [this]()
	{
		if (ANoclipPlayerController* PC = GetNoclipPC(); PC && IsLeader())
		{
			PC->NoclipStart();
		}
	});
	AddV(Center, StartButton, 10.f);
	StartNote = Txt(WidgetTree, LOCTEXT("StartNote", "Só o host inicia. Pode começar mesmo sem todos prontos."), Mono(12.f), Muted());
	StartNote->SetJustification(ETextJustify::Center);
	AddV(Center, StartNote, 10.f);
	AddH(Grid, Center, 20.f, true, VAlign_Fill);

	// Almoxarifado
	UBorder* ShopPanel = PanelBox(WidgetTree);
	UVerticalBox* ShopCol = WidgetTree->ConstructWidget<UVerticalBox>();
	ShopPanel->SetContent(ShopCol);
	MoneyText = Txt(WidgetTree, FText::GetEmpty(), Oswald(26.f, false), Ok());
	AddV(ShopCol, SpaceBetween(WidgetTree, Label(WidgetTree, LOCTEXT("Shop", "Almoxarifado")), MoneyText, VAlign_Bottom));
	UScrollBox* ShopScroll = WidgetTree->ConstructWidget<UScrollBox>();
	// Linhas de 2 cartoes: larguras iguais e, em cada linha, a altura do cartao mais alto
	// (uma grade uniforme deixaria todas as linhas com a altura do maior cartao da loja).
	UVerticalBox* Items = WidgetTree->ConstructWidget<UVerticalBox>();
	ShopScroll->AddChild(Items);
	UHorizontalBox* ItemRow = nullptr;
	const TArray<NoclipShop::FItem>& Catalog = NoclipShop::Items();
	for (int32 i = 0; i < Catalog.Num(); ++i)
	{
		const NoclipShop::FItem& Item = Catalog[i];
		UBorder* Card = Box(WidgetTree, Panel2(), FMargin(10.f), Line(), 1.f);
		UVerticalBox* CardCol = WidgetTree->ConstructWidget<UVerticalBox>();
		Card->SetContent(CardCol);
		// Nome quebra linha se precisar; o preco fica sempre a direita, sem invadir o nome.
		UTextBlock* Price = Txt(WidgetTree, FormatMoney(Item.Price), Mono(14.f), Amber());
		UHorizontalBox* NameRow = WidgetTree->ConstructWidget<UHorizontalBox>();
		AddH(NameRow, Wrap(Txt(WidgetTree, Item.Name, Mono(14.f, 600), Text()), 1.25f), 0.f, true, VAlign_Top);
		AddH(NameRow, Price, 8.f, false, VAlign_Top);
		AddV(CardCol, NameRow);
		AddV(CardCol, Wrap(Txt(WidgetTree, Item.Desc, Mono(12.f), Muted()), 1.4f), 6.f);
		// Empurra o botao para o pe do cartao (todos os cartoes da linha ficam alinhados).
		UVerticalBoxSlot* CardPush = AddV(CardCol, Gap(WidgetTree, 1.f, 1.f));
		CardPush->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		UNoclipButton* Buy = NewButton(WidgetTree, LOCTEXT("Buy", "Comprar"), ENoclipButtonKind::Normal);
		Buy->SetFontPx(16.f);
		Buy->SetMinHeight(36.f);
		Buy->OnClicked().AddWeakLambda(this, [this, i]()
		{
			if (ANoclipPlayerController* PC = GetNoclipPC())
			{
				PC->RequestBuyItem(i);
			}
		});
		AddV(CardCol, Buy, 6.f);
		if (i % 2 == 0)
		{
			ItemRow = WidgetTree->ConstructWidget<UHorizontalBox>();
			AddV(Items, ItemRow, i == 0 ? 0.f : 6.f);
		}
		AddH(ItemRow, Card, i % 2 == 0 ? 0.f : 6.f, true, VAlign_Fill);
		BuyButtons.Add(Buy);
		PriceTexts.Add(Price);
	}
	if (Catalog.Num() % 2 == 1 && ItemRow)
	{
		AddH(ItemRow, Gap(WidgetTree, 1.f, 1.f), 8.f, true, VAlign_Fill);
	}
	UVerticalBoxSlot* ShopSlot = AddV(ShopCol, ShopScroll, 12.f);
	ShopSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	AddH(Grid, Sized(WidgetTree, ShopPanel, 420.f), 20.f, false, VAlign_Fill);

	DefaultFocusWidget = StartButton;
}

void UNoclipLobbyScreen::NativeOnActivated()
{
	Refresh();
	// Quem nao lidera comeca no "pronto" (o iniciar fica desabilitado para ele).
	DefaultFocusWidget = IsLeader() ? StartButton : ReadyButton;
	Super::NativeOnActivated();
}

bool UNoclipLobbyScreen::HandleBack()
{
	if (UNoclipPrimaryLayout* Layout = GetLayout())
	{
		Layout->PushMenu(UNoclipPauseMenu::StaticClass());
	}
	return true;
}

void UNoclipLobbyScreen::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	RefreshAccumulator += InDeltaTime;
	if (RefreshAccumulator >= 0.2f)
	{
		RefreshAccumulator = 0.f;
		Refresh();
	}
}

void UNoclipLobbyScreen::Refresh()
{
	const ANoclipGameState* GS = GetNoclipGameState();
	if (!GS)
	{
		return;
	}
	const FString Room = GS->GetRoomName().IsEmpty() ? TEXT("Expediente") : ShortName(GS->GetRoomName(), 28);
	RoomLabel->SetText(Upper(FText::Format(LOCTEXT("RoomLabel", "Saguão · Sala \"{0}\""), FText::FromString(Room))));
	Title->SetText(Upper(FText::Format(LOCTEXT("BeforeShift", "Antes do turno {0}"), FText::AsNumber(GS->GetShiftIndex()))));
	OrderTitle->SetText(Upper(FText::Format(LOCTEXT("OrderTitle", "Ordem de serviço · Turno {0}"), FText::AsNumber(GS->GetShiftIndex()))));
	QuotaValue->SetText(FormatMoney(GS->GetQuota()));
	DurationValue->SetText(FormatClock(GetDefault<ANoclipGameMode>()->ShiftDurationSeconds));

	MenuButton->SetSuffix(NoclipGlyphs::GetKeyText(ENoclipPrompt::Back, GetOwningPlayer()), true);

	const bool bLeader = IsLeader();
	StartButton->SetIsEnabled(bLeader);
	StartButton->SetLabel(bLeader ? LOCTEXT("Start", "Iniciar expediente") : LOCTEXT("WaitHost", "Aguardando o host"));
	StartButton->SetSuffix(bLeader ? FText::FromString(TEXT("→")) : FText::GetEmpty());

	const ANoclipPlayerState* PS = GetOwningPlayerState<ANoclipPlayerState>();
	ReadyButton->SetLabel(PS && PS->IsReady() ? LOCTEXT("ReadyOn", "Pronto ✓ (clique para desmarcar)") : LOCTEXT("MarkReady", "Marcar como pronto"));

	RefreshCrew();
	RefreshShop();
}

void UNoclipLobbyScreen::RefreshCrew()
{
	const ANoclipGameState* GS = GetNoclipGameState();
	const APlayerState* LocalPS = GetOwningPlayerState();

	int32 Ready = 0;
	FString Signature;
	for (const APlayerState* PS : GS->PlayerArray)
	{
		const ANoclipPlayerState* NPS = Cast<ANoclipPlayerState>(PS);
		const bool bReady = NPS && NPS->IsReady();
		Ready += bReady ? 1 : 0;
		Signature += FString::Printf(TEXT("%s%d%d%d|"), PS ? *PS->GetPlayerName() : TEXT(""), bReady ? 1 : 0, PS == GS->GetLeader() ? 1 : 0, (NPS && NPS->IsLeftBehind()) ? 1 : 0);
	}
	CrewTitle->SetText(Upper(FText::Format(LOCTEXT("CrewTitle", "Equipe · {0}/{1} prontos"), FText::AsNumber(Ready), FText::AsNumber(GS->PlayerArray.Num()))));
	if (Signature == CrewSignature)
	{
		return;
	}
	CrewSignature = Signature;
	CrewList->ClearChildren();

	for (const APlayerState* PS : GS->PlayerArray)
	{
		if (!PS)
		{
			continue;
		}
		const ANoclipPlayerState* NPS = Cast<ANoclipPlayerState>(PS);
		const bool bReady = NPS && NPS->IsReady();
		const bool bLeftBehind = NPS && NPS->IsLeftBehind();
		// Coluna estreita: nome curto; voce em ambar, o host marcado pelo quadrado ambar (o selo mostra o estado).
		const bool bHost = PS == GS->GetLeader();
		const bool bMe = PS == LocalPS;
		const FString Name = ShortName(PS->GetPlayerName(), 13);

		UBorder* Item = Box(WidgetTree, Panel2(), FMargin(10.f, 0.f), Line2(), 1.f);
		Item->SetClipping(EWidgetClipping::ClipToBounds);
		UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>();
		Item->SetContent(Row);
		AddH(Row, Sized(WidgetTree, Box(WidgetTree, bHost ? Amber() : Line(), FMargin(0.f)), 10.f, 10.f));
		UTextBlock* NameText = Txt(WidgetTree, FText::FromString(Name), Mono(15.f, bMe ? 600 : 400), bMe ? Amber() : Text());
		NameText->SetTextOverflowPolicy(ETextOverflowPolicy::Ellipsis);
		NameText->SetClipping(EWidgetClipping::ClipToBounds);
		AddH(Row, NameText, 10.f, true);
		// Um selo so: pronto > sem itens (ficou para tras no colapso, GDD) > na loja.
		const FLinearColor BadgeColor = bReady ? Ok() : (bLeftBehind ? Warn() : Line());
		const FLinearColor BadgeText = bReady ? OkText() : (bLeftBehind ? Warn() : Muted());
		const FText BadgeLabel = bReady ? LOCTEXT("Ready", "Pronto") : (bLeftBehind ? LOCTEXT("NoItems", "Sem itens") : LOCTEXT("InShop", "Na loja"));
		UBorder* Badge = Box(WidgetTree, FLinearColor::Transparent, FMargin(8.f, 3.f), BadgeColor, 1.f);
		Badge->SetContent(Txt(WidgetTree, BadgeLabel, Mono(12.f), BadgeText));
		AddH(Row, Badge, 10.f);
		AddV(CrewList, Sized(WidgetTree, Item, 0.f, 44.f), 8.f);
	}

	// Vagas livres (borda tracejada no prototipo).
	UBorder* Free = Box(WidgetTree, FLinearColor::Transparent, FMargin(10.f, 0.f), Line(), 1.f);
	Free->SetVerticalAlignment(VAlign_Center);
	Free->SetContent(Txt(WidgetTree, LOCTEXT("FreeSlot", "Vaga livre"), Mono(14.f), Muted()));
	AddV(CrewList, Sized(WidgetTree, Free, 0.f, 44.f), 8.f);
}

void UNoclipLobbyScreen::RefreshShop()
{
	const ANoclipGameState* GS = GetNoclipGameState();
	const int32 Money = GS->GetMoney();
	MoneyText->SetText(FText::Format(LOCTEXT("Cash", "Caixa {0}"), FormatMoney(Money)));
	MoneyText->SetColorAndOpacity(FSlateColor(Ok()));

	const TArray<NoclipShop::FItem>& Catalog = NoclipShop::Items();
	for (int32 i = 0; i < BuyButtons.Num() && i < Catalog.Num(); ++i)
	{
		const int32 Owned = GS->GetOwnedCount(i);
		const bool bCanBuy = Money >= Catalog[i].Price;
		BuyButtons[i]->SetIsEnabled(bCanBuy);
		BuyButtons[i]->SetLabel(bCanBuy
			? (Owned > 0 ? FText::Format(LOCTEXT("BuyMore", "Comprar mais ({0})"), FText::AsNumber(Owned)) : LOCTEXT("Buy", "Comprar"))
			: FText::Format(LOCTEXT("Missing", "Falta {0}"), FormatMoney(Catalog[i].Price - Money)));
	}
}

#undef LOCTEXT_NAMESPACE
