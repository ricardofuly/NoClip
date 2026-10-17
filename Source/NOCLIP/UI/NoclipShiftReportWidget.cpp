// Copyright Fuly Games. All Rights Reserved.

#include "UI/NoclipShiftReportWidget.h"
#include "UI/NoclipButton.h"
#include "UI/NoclipUIStyle.h"
#include "UI/NoclipPrimaryLayout.h"
#include "Game/NoclipGameState.h"
#include "Game/NoclipPlayerState.h"
#include "Game/NoclipPlayerController.h"
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
#include "Components/Spacer.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(NoclipShiftReportWidget)

#define LOCTEXT_NAMESPACE "NoclipReport"

using namespace NoclipUI;

namespace
{
	constexpr float CountUpSeconds = 1.2f;

	UVerticalBox* StatCol(UWidgetTree* Tree, const FText& Name, UTextBlock*& OutValue, float ValuePx, const FLinearColor& ValueColor)
	{
		UVerticalBox* Col = Tree->ConstructWidget<UVerticalBox>();
		AddV(Col, Txt(Tree, Upper(Name), Mono(12.f, 400, 0.14f), Ink()));
		OutValue = Txt(Tree, FText::GetEmpty(), Oswald(ValuePx, false), ValueColor);
		AddV(Col, OutValue);
		return Col;
	}

	/** Carimbo girado (APROVADO / DEMITIDO). */
	UWidget* Stamp(UWidgetTree* Tree, const FText& InText, const FLinearColor& Color, float FontPx, float Angle)
	{
		UBorder* B = Box(Tree, FLinearColor::Transparent, FMargin(14.f, 6.f), Color, 4.f);
		B->SetContent(Txt(Tree, InText, Oswald(FontPx, false, 0.1f), Color));
		B->SetRenderTransformAngle(Angle);
		return B;
	}

	FText TimeAway(float Seconds)
	{
		const int32 S = FMath::RoundToInt(Seconds);
		return S >= 60
			? FText::Format(LOCTEXT("MinAway", "{0} min longe da equipe"), FText::AsNumber(S / 60))
			: FText::Format(LOCTEXT("SecAway", "{0} s longe da equipe"), FText::AsNumber(S));
	}

	UNoclipButton* PrimaryButton(UWidgetTree* Tree, const FText& Label)
	{
		// .btn de Report/Fired: 56px, padding 0 24, Oswald 22
		UNoclipButton* Button = Tree->ConstructWidget<UNoclipButton>(UNoclipButton::StaticClass());
		Button->Configure(Label, ENoclipButtonKind::Primary, ENoclipButtonStyle::Compact);
		Button->SetFontPx(22.f);
		Button->SetMinHeight(56.f);
		return Button;
	}
}

// =====================================================================  Relatorio (quota batida)

UNoclipShiftReportWidget::UNoclipShiftReportWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bClosesOnBack = false;
}

UTextBlock* UNoclipShiftReportWidget::AddLine(UVerticalBox* Col, const FText& LabelText, const FSlateFontInfo& ValueFont, bool bStrong)
{
	// .line { space-between; padding 10px 0; border-bottom: 1px dashed #6B5A2E; font-size 16px }
	UTextBlock* Value = Txt(WidgetTree, FText::GetEmpty(), ValueFont, Ink());
	UHorizontalBox* Row = SpaceBetween(WidgetTree, Txt(WidgetTree, LabelText, bStrong ? Mono(20.f, 600) : Mono(16.f), Ink()), Value, VAlign_Bottom);
	UVerticalBoxSlot* RowSlot = AddV(Col, Row, 10.f);
	RowSlot->SetPadding(FMargin(0.f, 10.f, 0.f, 10.f));
	AddV(Col, bStrong ? Rule(WidgetTree, Ink(), 2.f) : Dashed(WidgetTree, ManilaRule()));
	return Value;
}

void UNoclipShiftReportWidget::BuildTree()
{
	UOverlay* Root = WidgetTree->ConstructWidget<UOverlay>();
	WidgetTree->RootWidget = Root;
	UOverlaySlot* BackSlot = Root->AddChildToOverlay(Box(WidgetTree, Bg(), FMargin(0.f)));
	BackSlot->SetHorizontalAlignment(HAlign_Fill);
	BackSlot->SetVerticalAlignment(VAlign_Fill);

	// padding 48 80, display flex, gap 48
	UHorizontalBox* Screen = WidgetTree->ConstructWidget<UHorizontalBox>();
	UOverlaySlot* ScreenSlot = Root->AddChildToOverlay(Screen);
	ScreenSlot->SetHorizontalAlignment(HAlign_Fill);
	ScreenSlot->SetVerticalAlignment(VAlign_Fill);
	ScreenSlot->SetPadding(FMargin(80.f, 48.f));

	// ---------- article: 560, manila, padding 32, gap 10 ----------
	UOverlay* ArticleStack = WidgetTree->ConstructWidget<UOverlay>();
	UBorder* Article = Box(WidgetTree, Manila(), FMargin(32.f));
	UOverlaySlot* ArticleSlot = ArticleStack->AddChildToOverlay(Article);
	ArticleSlot->SetHorizontalAlignment(HAlign_Fill);
	ArticleSlot->SetVerticalAlignment(VAlign_Fill);
	UVerticalBox* Col = WidgetTree->ConstructWidget<UVerticalBox>();
	Article->SetContent(Col);
	Eyebrow = Txt(WidgetTree, FText::GetEmpty(), Mono(12.f, 400, 0.14f), Ink());
	AddV(Col, Eyebrow);
	AddV(Col, Txt(WidgetTree, Upper(LOCTEXT("Met", "Quota batida")), Oswald(52.f), Ink()), 10.f);
	Extracted = AddLine(Col, LOCTEXT("Extracted", "Valor extraído"), Mono(16.f, 600));
	QuotaValue = AddLine(Col, LOCTEXT("Quota", "Quota do turno"), Mono(16.f));
	Surplus = AddLine(Col, LOCTEXT("Surplus", "Excedente"), Mono(16.f));
	Bonus = AddLine(Col, LOCTEXT("Bonus", "Bônus por bater a quota"), Mono(16.f));
	Earned = AddLine(Col, LOCTEXT("ToCash", "Para o caixa da equipe"), Oswald(34.f, false), true);

	UHorizontalBox* Stats = WidgetTree->ConstructWidget<UHorizontalBox>();
	AddH(Stats, StatCol(WidgetTree, LOCTEXT("Items", "Achados"), Items, 28.f, Ink()), 0.f, true, VAlign_Top);
	AddH(Stats, StatCol(WidgetTree, LOCTEXT("InRoomEnd", "Na sala no fim"), InRoom, 28.f, Ink()), 12.f, true, VAlign_Top);
	AddH(Stats, StatCol(WidgetTree, LOCTEXT("Exit", "Saída"), ExitText, 28.f, Ink()), 12.f, true, VAlign_Top);
	AddV(Col, Stats, 22.f);
	LeftBehindNote = Wrap(Txt(WidgetTree, FText::GetEmpty(), Mono(13.f, 600), DangerDeep()));
	LeftBehindNote->SetVisibility(ESlateVisibility::Collapsed);
	AddV(Col, LeftBehindNote, 12.f);

	UOverlaySlot* StampSlot = ArticleStack->AddChildToOverlay(Stamp(WidgetTree, LOCTEXT("Approved", "APROVADO"), OkDeep(), 26.f, -8.f));
	StampSlot->SetHorizontalAlignment(HAlign_Right);
	StampSlot->SetVerticalAlignment(VAlign_Top);
	StampSlot->SetPadding(FMargin(0.f, 28.f, 28.f, 0.f));
	AddH(Screen, Sized(WidgetTree, ArticleStack, 560.f), 0.f, false, VAlign_Fill);

	// ---------- section: flex 1, gap 16 ----------
	UVerticalBox* Right = WidgetTree->ConstructWidget<UVerticalBox>();
	AddV(Right, Txt(WidgetTree, Upper(LOCTEXT("Awards", "Menções honrosas")), Oswald(28.f, false), Amber()));
	UUniformGridPanel* Awards = WidgetTree->ConstructWidget<UUniformGridPanel>();
	Awards->SetSlotPadding(FMargin(5.f));
	const FText AwardTitles[] = { LOCTEXT("AwBroke", "Mais quebrou achados"), LOCTEXT("AwCarried", "Mais carregou"), LOCTEXT("AwLost", "Mais tempo perdido"), LOCTEXT("AwThrow", "Maior arremesso") };
	for (int32 i = 0; i < 4; ++i)
	{
		UBorder* Card = Box(WidgetTree, Panel(), FMargin(14.f), Line(), 1.f);
		UVerticalBox* CardCol = WidgetTree->ConstructWidget<UVerticalBox>();
		Card->SetContent(CardCol);
		AddV(CardCol, Label(WidgetTree, AwardTitles[i]));
		UTextBlock* Name = Txt(WidgetTree, FText::GetEmpty(), Mono(17.f, 600), Text());
		Name->SetTextOverflowPolicy(ETextOverflowPolicy::Ellipsis);
		Name->SetClipping(EWidgetClipping::ClipToBounds);
		AddV(CardCol, Name, 4.f);
		UTextBlock* Detail = Wrap(Txt(WidgetTree, FText::GetEmpty(), Mono(13.f), Soft()), 1.4f);
		AddV(CardCol, Detail, 4.f);
		AddCell(Awards, Card, i / 2, i % 2);
		AwardNames.Add(Name);
		AwardDetails.Add(Detail);
	}
	AddV(Right, Awards, 16.f);
	UVerticalBoxSlot* Push = AddV(Right, Gap(WidgetTree, 1.f, 1.f));
	Push->SetSize(FSlateChildSize(ESlateSizeRule::Fill));

	Countdown = Txt(WidgetTree, FText::GetEmpty(), Mono(14.f), Soft());
	AddV(Right, Countdown);
	{
		NoclipUI::FBar Bar = MakeBar(WidgetTree, 6.f, Line2(), Amber());
		ProgressRoot = Bar.Root;
		ProgressFill = Bar.Fill;
		ProgressFillSlot = Bar.FillSlot;
		ProgressRestSlot = Bar.RestSlot;
		AddV(Right, Bar.Root, 10.f);
	}
	LobbyButton = PrimaryButton(WidgetTree, LOCTEXT("GoLobby", "Ir para o saguão agora"));
	LobbyButton->SetSuffix(FText::FromString(TEXT("→")));
	LobbyButton->OnClicked().AddWeakLambda(this, [this]()
	{
		if (ANoclipPlayerController* PC = GetNoclipPC(); PC && IsLeader())
		{
			PC->RequestSkipToLobby();
		}
	});
	AddV(Right, LobbyButton, 10.f);
	AddH(Screen, Right, 48.f, true, VAlign_Fill);

	DefaultFocusWidget = LobbyButton;
}

void UNoclipShiftReportWidget::NativeOnActivated()
{
	if (const ANoclipGameState* GS = GetNoclipGameState())
	{
		ApplyResult(GS->GetLastResult());
		TotalSeconds = FMath::Max(1.f, GS->GetPhaseTimeRemaining());
	}
	CountUpTime = 0.f;
	Super::NativeOnActivated();
}

void UNoclipShiftReportWidget::ApplyResult(const FNoclipShiftResult& R)
{
	Shown = R;
	Eyebrow->SetText(Upper(FText::Format(LOCTEXT("Eyebrow", "Relatório de expediente · Turno {0}"), FText::AsNumber(R.ShiftIndex))));
	Extracted->SetText(FormatMoney(R.Extracted));
	QuotaValue->SetText(FText::Format(LOCTEXT("Minus", "− {0}"), FormatMoney(R.Quota)));
	Surplus->SetText(FormatMoney(FMath::Max(0, R.Extracted - R.Quota)));
	Bonus->SetText(FText::Format(LOCTEXT("Plus", "+ {0}"), FormatMoney(R.BonusPaid)));
	Earned->SetText(FText::Format(LOCTEXT("PlusTight", "+{0}"), FormatMoney(0)));
	Items->SetText(FText::AsNumber(R.ItemsExtracted));
	InRoom->SetText(FText::Format(LOCTEXT("OfFmt", "{0} de {1}"), FText::AsNumber(R.PlayersExtracted), FText::AsNumber(R.PlayersTotal)));
	ExitText->SetText(R.bExtractedEarly ? LOCTEXT("Elevator", "Elevador") : LOCTEXT("TimeUp", "Fim do tempo"));
	if (R.bTeamWiped)
	{
		ExitText->SetText(LOCTEXT("Wiped", "Equipe perdida"));
	}
	TArray<FText> Notes;
	if (R.bTeamWiped)
	{
		Notes.Add(LOCTEXT("WipedNote", "A equipe inteira morreu. A Baseline recolheu só o que já estava na Sala Manila."));
	}
	else if (R.PlayersDied > 0)
	{
		Notes.Add(R.PlayersDied == 1
			? LOCTEXT("DiedOne", "1 estagiário morreu no turno. Volta no próximo sem itens.")
			: FText::Format(LOCTEXT("DiedMany", "{0} estagiários morreram no turno. Voltam no próximo sem itens."), FText::AsNumber(R.PlayersDied)));
	}
	if (R.PlayersLeftBehind > 0)
	{
		const bool bOne = R.PlayersLeftBehind == 1;
		const FText Lost = R.ValueLostCarried > 0
			? FText::Format(bOne ? LOCTEXT("LostOne", " e perdeu {0} que carregava") : LOCTEXT("LostMany", " e perderam {0} que carregavam"), FormatMoney(R.ValueLostCarried))
			: FText::GetEmpty();
		Notes.Add(bOne
			? FText::Format(LOCTEXT("LeftOne", "1 estagiário ficou para trás no colapso{0}. Volta no próximo turno sem itens."), Lost)
			: FText::Format(LOCTEXT("LeftMany", "{0} estagiários ficaram para trás no colapso{1}. Voltam no próximo turno sem itens."), FText::AsNumber(R.PlayersLeftBehind), Lost));
	}
	LeftBehindNote->SetText(FText::Join(FText::FromString(TEXT("\n")), Notes));
	LeftBehindNote->SetVisibility(Notes.Num() > 0 ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	RefreshAwards();
}

void UNoclipShiftReportWidget::RefreshAwards()
{
	const ANoclipGameState* GS = GetNoclipGameState();
	if (!GS)
	{
		return;
	}
	const ANoclipPlayerState* Best[4] = { nullptr, nullptr, nullptr, nullptr };
	for (const APlayerState* PS : GS->PlayerArray)
	{
		const ANoclipPlayerState* N = Cast<ANoclipPlayerState>(PS);
		if (!N) { continue; }
		if (N->DamageCaused > 0 && (!Best[0] || N->DamageCaused > Best[0]->DamageCaused)) { Best[0] = N; }
		if (N->ValueCarried > 0 && (!Best[1] || N->ValueCarried > Best[1]->ValueCarried)) { Best[1] = N; }
		if (N->TimeAlone > 1.f && (!Best[2] || N->TimeAlone > Best[2]->TimeAlone)) { Best[2] = N; }
		if (N->LongestThrowCm > 50.f && (!Best[3] || N->LongestThrowCm > Best[3]->LongestThrowCm)) { Best[3] = N; }
	}
	auto NameOf = [](const ANoclipPlayerState* N) { return N ? ShortNameText(N->GetPlayerName(), 18) : LOCTEXT("Nobody", "Ninguém"); };

	AwardNames[0]->SetText(NameOf(Best[0]));
	AwardDetails[0]->SetText(Best[0] ? FText::Format(LOCTEXT("AwBrokeFmt", "−{0} em batidas"), FormatMoney(Best[0]->DamageCaused)) : LOCTEXT("AwBrokeNone", "nenhum achado quebrado"));
	AwardNames[1]->SetText(NameOf(Best[1]));
	AwardDetails[1]->SetText(Best[1] ? FText::Format(LOCTEXT("AwCarriedFmt", "{0} levados à sala"), FormatMoney(Best[1]->ValueCarried)) : LOCTEXT("AwCarriedNone", "nada na sala"));
	AwardNames[2]->SetText(NameOf(Best[2]));
	AwardDetails[2]->SetText(Best[2] ? TimeAway(Best[2]->TimeAlone) : LOCTEXT("AwLostNone", "equipe sempre junta"));
	AwardNames[3]->SetText(NameOf(Best[3]));
	FNumberFormattingOptions Meters;
	Meters.MaximumFractionalDigits = 0;
	AwardDetails[3]->SetText(Best[3]
		? FText::Format(LOCTEXT("AwThrowFmt", "{0}, {1} m"), Best[3]->LongestThrowItem.IsEmpty() ? LOCTEXT("Something", "um achado") : Best[3]->LongestThrowItem, FText::AsNumber(Best[3]->LongestThrowCm / 100.f, &Meters))
		: LOCTEXT("AwThrowNone", "ninguém arremessou nada"));
}

void UNoclipShiftReportWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	const ANoclipGameState* GS = GetNoclipGameState();
	if (!GS)
	{
		return;
	}
	if (GS->GetLastResult().ShiftIndex != Shown.ShiftIndex)
	{
		ApplyResult(GS->GetLastResult());
		CountUpTime = 0.f;
	}

	if (CountUpTime < CountUpSeconds)
	{
		CountUpTime = FMath::Min(CountUpTime + InDeltaTime, CountUpSeconds);
		const float Alpha = FMath::InterpEaseOut(0.f, 1.f, CountUpTime / CountUpSeconds, 3.f);
		Earned->SetText(FText::Format(LOCTEXT("PlusTight", "+{0}"), FormatMoney(FMath::RoundToInt(Shown.MoneyEarned * Alpha))));
	}

	const float Left = GS->GetPhaseTimeRemaining();
	const float Pct = 1.f - FMath::Clamp(Left / TotalSeconds, 0.f, 1.f);
	if (ProgressFillSlot && ProgressRestSlot)
	{
		FSlateChildSize A(ESlateSizeRule::Fill); A.Value = Pct; ProgressFillSlot->SetSize(A);
		FSlateChildSize B(ESlateSizeRule::Fill); B.Value = 1.f - Pct; ProgressRestSlot->SetSize(B);
	}

	RefreshAccumulator += InDeltaTime;
	if (RefreshAccumulator >= 0.25f)
	{
		RefreshAccumulator = 0.f;
		Countdown->SetText(FText::Format(LOCTEXT("Countdown", "Voltando ao saguão em {0} s. Próxima quota: {1}."), FText::AsNumber(FMath::Max(0, FMath::CeilToInt(Left))), FormatMoney(Shown.NextQuota)));
		const bool bLeader = IsLeader();
		LobbyButton->SetIsEnabled(bLeader);
		LobbyButton->SetLabel(bLeader ? LOCTEXT("GoLobby", "Ir para o saguão agora") : LOCTEXT("OnlyHost", "Só o host adianta"));
		LobbyButton->SetSuffix(bLeader ? FText::FromString(TEXT("→")) : FText::GetEmpty());
		RefreshAwards();
	}
}

// =====================================================================  Demissao

UNoclipFiredScreen::UNoclipFiredScreen(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bClosesOnBack = false;
}

void UNoclipFiredScreen::BuildTree()
{
	UOverlay* Root = WidgetTree->ConstructWidget<UOverlay>();
	WidgetTree->RootWidget = Root;
	UOverlaySlot* BackSlot = Root->AddChildToOverlay(Box(WidgetTree, BgDeep(), FMargin(0.f)));
	BackSlot->SetHorizontalAlignment(HAlign_Fill);
	BackSlot->SetVerticalAlignment(VAlign_Fill);

	// display flex, centralizado, gap 56
	UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>();
	UOverlaySlot* RowSlot = Root->AddChildToOverlay(Row);
	RowSlot->SetHorizontalAlignment(HAlign_Center);
	RowSlot->SetVerticalAlignment(VAlign_Center);

	// ---------- article: 600, manila, padding 36, gap 16, rotate(-1deg) ----------
	UOverlay* ArticleStack = WidgetTree->ConstructWidget<UOverlay>();
	UBorder* Article = Box(WidgetTree, Manila(), FMargin(36.f));
	UOverlaySlot* ArticleSlot = ArticleStack->AddChildToOverlay(Article);
	ArticleSlot->SetHorizontalAlignment(HAlign_Fill);
	ArticleSlot->SetVerticalAlignment(VAlign_Fill);
	UVerticalBox* Col = WidgetTree->ConstructWidget<UVerticalBox>();
	Article->SetContent(Col);

	ShiftLabel = Txt(WidgetTree, FText::GetEmpty(), Mono(12.f, 400, 0.14f), Ink());
	AddV(Col, SpaceBetween(WidgetTree, Txt(WidgetTree, Upper(LOCTEXT("HR", "Baseline Recuperações Ltda. · RH")), Mono(12.f, 400, 0.14f), Ink()), ShiftLabel));
	UTextBlock* Title = Wrap(Txt(WidgetTree, Upper(LOCTEXT("FiredTitle", "Vocês foram demitidos")), Oswald(58.f), Ink()), 1.f);
	AddV(Col, Title, 16.f);
	AddV(Col, Wrap(Txt(WidgetTree, LOCTEXT("FiredBody", "Prezados estagiários, a quota do turno não foi atingida. Agradecemos os serviços prestados. Seus crachás foram desativados."), Mono(16.f), Ink()), 1.55f), 16.f);

	UVerticalBox* StatsBlock = WidgetTree->ConstructWidget<UVerticalBox>();
	AddV(StatsBlock, Dashed(WidgetTree, ManilaRule()));
	UHorizontalBox* Stats = WidgetTree->ConstructWidget<UHorizontalBox>();
	AddH(Stats, StatCol(WidgetTree, LOCTEXT("Extracted", "Extraído"), Extracted, 30.f, Ink()), 0.f, true, VAlign_Top);
	AddH(Stats, StatCol(WidgetTree, LOCTEXT("QuotaShort", "Quota"), QuotaValue, 30.f, Ink()), 12.f, true, VAlign_Top);
	AddH(Stats, StatCol(WidgetTree, LOCTEXT("Missing", "Faltaram"), Missing, 30.f, DangerDeep()), 12.f, true, VAlign_Top);
	UVerticalBoxSlot* StatsSlot = AddV(StatsBlock, Stats);
	StatsSlot->SetPadding(FMargin(0.f, 14.f));
	AddV(StatsBlock, Dashed(WidgetTree, ManilaRule()));
	AddV(Col, StatsBlock, 16.f);

	RunInfo = Txt(WidgetTree, FText::GetEmpty(), Mono(14.f), Ink());
	AddV(Col, RunInfo, 16.f);
	AddV(Col, Gap(WidgetTree, 1.f, 40.f));

	UOverlaySlot* StampSlot = ArticleStack->AddChildToOverlay(Stamp(WidgetTree, LOCTEXT("FiredStamp", "DEMITIDO"), DangerDeep(), 30.f, -12.f));
	StampSlot->SetHorizontalAlignment(HAlign_Right);
	StampSlot->SetVerticalAlignment(VAlign_Bottom);
	StampSlot->SetPadding(FMargin(0.f, 0.f, 30.f, 26.f));
	ArticleStack->SetRenderTransformAngle(-1.f);
	AddH(Row, Sized(WidgetTree, ArticleStack, 600.f), 0.f, false, VAlign_Center);

	// ---------- nav: 360, gap 12 ----------
	UVerticalBox* Nav = WidgetTree->ConstructWidget<UVerticalBox>();
	AddV(Nav, Wrap(Txt(WidgetTree, LOCTEXT("RunRestarts", "A corrida recomeça do turno 1, com caixa zerado. A equipe continua na mesma sala."), Mono(15.f), Soft())));
	NewRunButton = PrimaryButton(WidgetTree, LOCTEXT("NewRun", "Novo estágio"));
	NewRunButton->SetSuffix(FText::FromString(TEXT("→")));
	NewRunButton->OnClicked().AddWeakLambda(this, [this]()
	{
		if (ANoclipPlayerController* PC = GetNoclipPC(); PC && IsLeader())
		{
			PC->RequestSkipToLobby();
		}
	});
	AddV(Nav, NewRunButton, 18.f);
	UNoclipButton* Leave = WidgetTree->ConstructWidget<UNoclipButton>(UNoclipButton::StaticClass());
	Leave->Configure(LOCTEXT("ToMenu", "Sair para o menu"), ENoclipButtonKind::Normal, ENoclipButtonStyle::Compact);
	Leave->SetFontPx(22.f);
	Leave->SetMinHeight(56.f);
	Leave->OnClicked().AddWeakLambda(this, [this]()
	{
		UNoclipPrimaryLayout* Layout = GetLayout();
		if (!Layout)
		{
			return;
		}
		TWeakObjectPtr<ANoclipPlayerController> WeakPC = GetNoclipPC();
		Layout->PushConfirm(LOCTEXT("LeaveTitle", "Sair para o menu?"),
			IsHost() ? LOCTEXT("LeaveBodyHost", "Você é o host: a sessão termina e os colegas voltam ao menu com um aviso.") : LOCTEXT("LeaveBodyClient", "Você sai da sala. O grupo continua sem você."),
			LOCTEXT("LeaveConfirm", "Sair"),
			[WeakPC]() { if (ANoclipPlayerController* P = WeakPC.Get()) { P->LeaveToMainMenu(); } });
	});
	AddV(Nav, Leave, 12.f);
	AddH(Row, Sized(WidgetTree, Nav, 360.f), 56.f, false, VAlign_Center);

	DefaultFocusWidget = NewRunButton;
}

void UNoclipFiredScreen::NativeOnActivated()
{
	if (const ANoclipGameState* GS = GetNoclipGameState())
	{
		ApplyResult(GS->GetLastResult());
	}
	Super::NativeOnActivated();
}

void UNoclipFiredScreen::ApplyResult(const FNoclipShiftResult& R)
{
	Shown = R;
	ShiftLabel->SetText(Upper(FText::Format(LOCTEXT("ShiftN", "Turno {0}"), FText::AsNumber(R.ShiftIndex))));
	Extracted->SetText(FormatMoney(R.Extracted));
	QuotaValue->SetText(FormatMoney(R.Quota));
	Missing->SetText(FormatMoney(FMath::Max(0, R.Quota - R.Extracted)));
	RunInfo->SetText(FText::Format(LOCTEXT("RunInfo", "Turnos completados nesta corrida: {0} · Caixa perdido: {1}"), FText::AsNumber(R.ShiftsCompleted), FormatMoney(R.MoneyLost)));
}

void UNoclipFiredScreen::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	RefreshAccumulator += InDeltaTime;
	if (RefreshAccumulator < 0.25f)
	{
		return;
	}
	RefreshAccumulator = 0.f;
	if (const ANoclipGameState* GS = GetNoclipGameState(); GS && GS->GetLastResult().ShiftIndex != Shown.ShiftIndex)
	{
		ApplyResult(GS->GetLastResult());
	}
	const bool bLeader = IsLeader();
	NewRunButton->SetIsEnabled(bLeader);
	NewRunButton->SetLabel(bLeader ? LOCTEXT("NewRun", "Novo estágio") : LOCTEXT("WaitHost", "Aguardando o host"));
	NewRunButton->SetSuffix(bLeader ? FText::FromString(TEXT("→")) : FText::GetEmpty());
}

#undef LOCTEXT_NAMESPACE
