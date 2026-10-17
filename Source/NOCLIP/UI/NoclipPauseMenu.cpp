// Copyright Fuly Games. All Rights Reserved.

#include "UI/NoclipPauseMenu.h"
#include "UI/NoclipButton.h"
#include "UI/NoclipUIStyle.h"
#include "UI/NoclipInputGlyphs.h"
#include "UI/NoclipPrimaryLayout.h"
#include "UI/NoclipMainMenu.h"
#include "Game/NoclipGameState.h"
#include "Game/NoclipPlayerController.h"
#include "Character/NoclipCharacter.h"
#include "Character/NoclipGrabComponent.h"
#include "World/NoclipLootActor.h"
#include "World/NoclipExtractionZone.h"
#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Border.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "GameFramework/PlayerState.h"
#include "HAL/PlatformApplicationMisc.h"
#include "EngineUtils.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(NoclipPauseMenu)

#define LOCTEXT_NAMESPACE "NoclipPause"

using namespace NoclipUI;

void UNoclipPauseMenu::BuildTree()
{
	UCanvasPanel* Canvas = WidgetTree->ConstructWidget<UCanvasPanel>();
	WidgetTree->RootWidget = Canvas;

	// rgba(10,9,6,.8) por cima do jogo
	UCanvasPanelSlot* DimSlot = Canvas->AddChildToCanvas(Box(WidgetTree, Hex(TEXT("0A0906"), 0.8f), FMargin(0.f)));
	DimSlot->SetAnchors(FAnchors(0.f, 0.f, 1.f, 1.f));
	DimSlot->SetOffsets(FMargin(0.f));

	// ---------- role=dialog: left 80, top 64, 460, gap 22 ----------
	UVerticalBox* Left = WidgetTree->ConstructWidget<UVerticalBox>();
	AddV(Left, H1(WidgetTree, LOCTEXT("Title", "Pausa"), 64.f, Amber(), 0.f));
	UBorder* Note = Box(WidgetTree, NoclipUI::Note(), FMargin(12.f, 10.f), NoteLine(), 1.f);
	Note->SetContent(Wrap(Txt(WidgetTree, LOCTEXT("Note", "O expediente continua rodando. Seu personagem fica parado e vulnerável."), Mono(14.f), NoteText())));
	AddV(Left, Note, 8.f);

	UVerticalBox* Nav = WidgetTree->ConstructWidget<UVerticalBox>();
	auto AddButton = [this, Nav](const FText& Label, ENoclipButtonKind Kind) -> UNoclipButton*
	{
		UNoclipButton* Button = WidgetTree->ConstructWidget<UNoclipButton>(UNoclipButton::StaticClass());
		Button->Configure(Label, Kind, ENoclipButtonStyle::Menu);
		AddV(Nav, Button, 10.f);
		return Button;
	};
	ContinueButton = AddButton(LOCTEXT("Continue", "Continuar"), ENoclipButtonKind::Primary);
	ContinueButton->OnClicked().AddWeakLambda(this, [this]() { DeactivateWidget(); });

	UNoclipButton* Options = AddButton(LOCTEXT("Options", "Opções"), ENoclipButtonKind::Normal);
	Options->SetSuffix(FText::FromString(TEXT("→")));
	Options->OnClicked().AddWeakLambda(this, [this]() { if (UNoclipPrimaryLayout* L = GetLayout()) { L->PushMenu(UNoclipOptionsMenu::StaticClass()); } });

	UNoclipButton* Invite = AddButton(LOCTEXT("Invite", "Convidar amigos"), ENoclipButtonKind::Normal);
	Invite->SetSuffixNote(LOCTEXT("SteamOverlay", "overlay da Steam"));
	Invite->OnClicked().AddWeakLambda(this, [this]()
	{
		ANoclipPlayerController* PC = GetNoclipPC();
		if (!PC || !PC->ShowInviteOverlay())
		{
			if (UNoclipPrimaryLayout* L = GetLayout())
			{
				L->ShowToast(LOCTEXT("NoOverlay", "Overlay da Steam indisponível. Passe o código de convite ao lado."), false);
			}
		}
	});

	UNoclipButton* Leave = AddButton(LOCTEXT("Leave", "Sair para o menu"), ENoclipButtonKind::Danger);
	Leave->OnClicked().AddUObject(this, &UNoclipPauseMenu::HandleLeave);
	AddV(Left, Nav, 22.f);

	AddV(Left, Wrap(Txt(WidgetTree, LOCTEXT("HostNote", "Se você é o host, sair encerra a sessão para todos. Os colegas voltam ao menu com um aviso."), Mono(13.f), Muted())), 22.f);

	UCanvasPanelSlot* LeftSlot = Canvas->AddChildToCanvas(Sized(WidgetTree, Left, 460.f));
	LeftSlot->SetPosition(FVector2D(80.f, 64.f));
	LeftSlot->SetAutoSize(true);

	// ---------- aside: right 80, top 64, 380, panel, padding 20, gap 14 ----------
	UBorder* Panel = Box(WidgetTree, NoclipUI::Panel(), FMargin(20.f), Line(), 1.f);
	UVerticalBox* Col = WidgetTree->ConstructWidget<UVerticalBox>();
	Panel->SetContent(Col);
	CrewTitle = Label(WidgetTree, FText::GetEmpty());
	AddV(Col, CrewTitle);
	CrewList = WidgetTree->ConstructWidget<UVerticalBox>();
	AddV(Col, CrewList, 14.f);
	AddV(Col, Dashed(WidgetTree, Line()), 14.f);
	QuotaLine = Txt(WidgetTree, FText::GetEmpty(), Oswald(22.f, false), Amber());
	AddV(Col, SpaceBetween(WidgetTree, Txt(WidgetTree, LOCTEXT("InRoomQuota", "Na sala / quota"), Mono(14.f), Text()), QuotaLine), 10.f);

	UHorizontalBox* CodeRow = WidgetTree->ConstructWidget<UHorizontalBox>();
	CodeText = Txt(WidgetTree, FText::GetEmpty(), Mono(14.f, 600, 0.1f), Text());
	AddH(CodeRow, CodeText);
	UNoclipButton* Copy = WidgetTree->ConstructWidget<UNoclipButton>(UNoclipButton::StaticClass());
	Copy->Configure(FText::FromString(TEXT("❐")), ENoclipButtonKind::Normal, ENoclipButtonStyle::Step);
	Copy->SetFontPx(16.f);
	Copy->OnClicked().AddWeakLambda(this, [this]()
	{
		if (const ANoclipGameState* GS = GetNoclipGameState())
		{
			FPlatformApplicationMisc::ClipboardCopy(*GS->GetInviteCode());
			if (UNoclipPrimaryLayout* L = GetLayout())
			{
				L->ShowToast(FText::Format(LOCTEXT("Copied", "Código {0} copiado."), FText::FromString(GS->GetInviteCode())), false);
			}
		}
	});
	AddH(CodeRow, Sized(WidgetTree, Copy, 44.f, 44.f), 8.f);
	AddV(Col, SpaceBetween(WidgetTree, Txt(WidgetTree, LOCTEXT("InviteCode", "Código de convite"), Mono(14.f), Text()), CodeRow), 14.f);

	UCanvasPanelSlot* PanelSlot = Canvas->AddChildToCanvas(Sized(WidgetTree, Panel, 380.f));
	PanelSlot->SetAnchors(FAnchors(1.f, 0.f));
	PanelSlot->SetAlignment(FVector2D(1.f, 0.f));
	PanelSlot->SetPosition(FVector2D(-80.f, 64.f));
	PanelSlot->SetAutoSize(true);

	DefaultFocusWidget = ContinueButton;
}

void UNoclipPauseMenu::NativeOnActivated()
{
	RefreshDynamic();
	Super::NativeOnActivated();
}

void UNoclipPauseMenu::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	RefreshAccumulator += InDeltaTime;
	if (RefreshAccumulator >= 0.25f)
	{
		RefreshAccumulator = 0.f;
		RefreshDynamic();
	}
}

void UNoclipPauseMenu::RefreshDynamic()
{
	const APlayerController* PC = GetOwningPlayer();
	const ANoclipGameState* GS = GetNoclipGameState();
	ContinueButton->SetSuffix(NoclipGlyphs::GetKeyText(ENoclipPrompt::Back, PC), true);
	if (!GS)
	{
		return;
	}

	CrewTitle->SetText(Upper(FText::Format(LOCTEXT("CrewTitle", "Turno {0} · equipe"), FText::AsNumber(GS->GetShiftIndex()))));
	QuotaLine->SetText(FText::Format(LOCTEXT("QuotaLine", "{0} / {1}"), FormatMoney(GS->GetValueInExtraction()), FormatMoney(GS->GetQuota())));
	CodeText->SetText(FText::FromString(GS->GetInviteCode().IsEmpty() ? TEXT("—") : GS->GetInviteCode()));

	const ANoclipExtractionZone* Zone = nullptr;
	for (TActorIterator<ANoclipExtractionZone> It(GetWorld()); It; ++It)
	{
		Zone = *It;
		break;
	}
	const bool bInShift = GS->GetPhase() == ENoclipShiftPhase::Active || GS->GetPhase() == ENoclipShiftPhase::Collapse;

	struct FEntry { FString Name; FText State; bool bGood = false; };
	TArray<FEntry> Entries;
	FString Signature;
	const APlayerState* LocalPS = PC ? PC->PlayerState : nullptr;
	for (const APlayerState* PS : GS->PlayerArray)
	{
		if (!PS)
		{
			continue;
		}
		FEntry E;
		E.Name = ShortName(PS->GetPlayerName(), 16);
		if (PS == LocalPS) { E.Name = FString::Printf(TEXT("%s (você)"), *E.Name); }
		if (PS == GS->GetLeader()) { E.Name += TEXT(" · host"); }

		const ANoclipCharacter* Pawn = Cast<ANoclipCharacter>(PS->GetPawn());
		const UNoclipGrabComponent* Grab = Pawn ? Pawn->GetGrabComponent() : nullptr;
		const ANoclipLootActor* Held = Grab ? Cast<ANoclipLootActor>(Grab->GetHeldActor()) : nullptr;
		if (Held)
		{
			E.State = FText::Format(LOCTEXT("Holding", "segurando {0}"), Held->GetDisplayName());
		}
		else if (bInShift && Zone && Pawn && Zone->IsInside(Pawn->GetActorLocation()))
		{
			E.State = LOCTEXT("InRoom", "na Sala Manila");
			E.bGood = true;
		}
		else
		{
			E.State = bInShift ? LOCTEXT("Exploring", "explorando") : LOCTEXT("InLobby", "no saguão");
		}
		Signature += E.Name + E.State.ToString() + TEXT("|");
		Entries.Add(E);
	}
	if (Signature == CrewSignature)
	{
		return;
	}
	CrewSignature = Signature;

	CrewList->ClearChildren();
	for (int32 i = 0; i < Entries.Num(); ++i)
	{
		UVerticalBox* Item = WidgetTree->ConstructWidget<UVerticalBox>();
		UHorizontalBox* Row = SpaceBetween(WidgetTree,
			Txt(WidgetTree, FText::FromString(Entries[i].Name), Mono(15.f), Text()),
			Txt(WidgetTree, Entries[i].State, Mono(15.f), Entries[i].bGood ? OkText() : Soft()));
		AddV(Item, Sized(WidgetTree, Row, 0.f, 40.f));
		if (i < Entries.Num() - 1)
		{
			AddV(Item, Rule(WidgetTree, Line2()));
		}
		AddV(CrewList, Item, 8.f);
	}
}

void UNoclipPauseMenu::HandleLeave()
{
	UNoclipPrimaryLayout* Layout = GetLayout();
	if (!Layout)
	{
		return;
	}
	TWeakObjectPtr<ANoclipPlayerController> WeakPC = GetNoclipPC();
	Layout->PushConfirm(
		LOCTEXT("LeaveTitle", "Sair para o menu?"),
		IsHost() ? LOCTEXT("LeaveBodyHost", "Você é o host: a sessão termina e os colegas voltam ao menu com um aviso.") : LOCTEXT("LeaveBodyClient", "Você sai da sala. O grupo continua sem você."),
		LOCTEXT("LeaveConfirm", "Sair"),
		[WeakPC]()
		{
			if (ANoclipPlayerController* P = WeakPC.Get())
			{
				P->LeaveToMainMenu();
			}
		});
}

#undef LOCTEXT_NAMESPACE
