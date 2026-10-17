// Copyright Fuly Games. All Rights Reserved.

#include "UI/NoclipCharacterSelectScreen.h"
#include "UI/NoclipButton.h"
#include "UI/NoclipUIStyle.h"
#include "UI/NoclipInputGlyphs.h"
#include "UI/NoclipPrimaryLayout.h"
#include "Character/NoclipCharacterCatalog.h"
#include "Game/NoclipGameState.h"
#include "Game/NoclipPlayerController.h"
#include "Game/NoclipPlayerState.h"
#include "World/NoclipCharacterStage.h"
#include "World/NoclipPortraitStudio.h"
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
#include "Components/UniformGridPanel.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/WrapBox.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/World.h"
#include "GameFramework/PlayerState.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(NoclipCharacterSelectScreen)

#define LOCTEXT_NAMESPACE "NoclipCharacterSelect"

using namespace NoclipUI;

namespace
{
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

	bool IsPrevKey(const FKey& K) { return K == EKeys::Q || K == EKeys::Left || K == EKeys::Gamepad_LeftShoulder || K == EKeys::Gamepad_DPad_Left || K == EKeys::Gamepad_LeftStick_Left; }
	bool IsNextKey(const FKey& K) { return K == EKeys::E || K == EKeys::Right || K == EKeys::Gamepad_RightShoulder || K == EKeys::Gamepad_DPad_Right || K == EKeys::Gamepad_LeftStick_Right; }

	/** Mesmo FVector do mundo do carrossel: longe de tudo para a captura nao pegar mais nada por acidente. */
	const FVector StageLocation(150000.f, 150000.f, -60000.f);
}

void UNoclipCharacterSelectScreen::BuildTree()
{
	UCanvasPanel* Canvas = WidgetTree->ConstructWidget<UCanvasPanel>();
	WidgetTree->RootWidget = Canvas;
	Place(Canvas, Box(WidgetTree, Bg(), FMargin(0.f)), FAnchors(0.f, 0.f, 1.f, 1.f), FVector2D::ZeroVector, FMargin(0.f), false);

	// ---------- header: left 80, top 56, width 680 ----------
	{
		UVerticalBox* Header = WidgetTree->ConstructWidget<UVerticalBox>();
		BackLink = NewButton(WidgetTree, LOCTEXT("Back", "Voltar à sala de espera"), ENoclipButtonKind::Normal, ENoclipButtonStyle::Link);
		BackLink->OnClicked().AddWeakLambda(this, [this]() { DeactivateWidget(); });
		AddV(Header, BackLink, 0.f, HAlign_Left);
		AddV(Header, Label(WidgetTree, LOCTEXT("Eyebrow", "Recursos humanos · ficha de admissão")), 6.f);
		AddV(Header, H1(WidgetTree, LOCTEXT("Title", "Escolha o estagiário"), 52.f), 6.f);
		Place(Canvas, Sized(WidgetTree, Header, 680.f), FAnchors(0.f, 0.f), FVector2D::ZeroVector, FMargin(80.f, 56.f, 0.f, 0.f), true);
	}

	// ---------- carrossel: left 40, top 176, 760 x 420 (captura 3D) ----------
	{
		UOverlay* Stack = WidgetTree->ConstructWidget<UOverlay>();
		CarouselImage = WidgetTree->ConstructWidget<UImage>();
		CarouselImage->SetColorAndOpacity(FLinearColor::White);
		CarouselImage->SetVisibility(ESlateVisibility::HitTestInvisible);
		AddOverlay(Stack, CarouselImage, HAlign_Fill, VAlign_Fill);
		// Bordas somem no fundo da tela (vinheta).
		UBorder* Fade = Box(WidgetTree, FLinearColor::Transparent, FMargin(0.f), Hex(TEXT("12100C"), 0.85f), 18.f);
		Fade->SetVisibility(ESlateVisibility::HitTestInvisible);
		AddOverlay(Stack, Fade, HAlign_Fill, VAlign_Fill);
		Place(Canvas, Sized(WidgetTree, Stack, ANoclipCharacterStage::CaptureWidth, ANoclipCharacterStage::CaptureHeight), FAnchors(0.f, 0.f), FVector2D::ZeroVector, FMargin(40.f, 176.f, 0.f, 0.f), true);
	}

	// ---------- nav: left 80, top 608, width 680 ----------
	{
		UHorizontalBox* Nav = WidgetTree->ConstructWidget<UHorizontalBox>();
		UNoclipButton* Prev = NewButton(WidgetTree, FText::FromString(TEXT("‹")), ENoclipButtonKind::Normal, ENoclipButtonStyle::Step);
		Prev->OnClicked().AddWeakLambda(this, [this]() { Rotate(-1); });
		AddH(Nav, Sized(WidgetTree, Prev, 48.f, 48.f));

		UVerticalBox* Mid = WidgetTree->ConstructWidget<UVerticalBox>();
		CurrentName = Txt(WidgetTree, FText::GetEmpty(), Oswald(22.f, false, 0.06f), TextBright());
		CurrentName->SetJustification(ETextJustify::Center);
		AddV(Mid, CurrentName, 0.f, HAlign_Center);
		Dots = WidgetTree->ConstructWidget<UHorizontalBox>();
		AddV(Mid, Dots, 8.f, HAlign_Center);
		AddH(Nav, Mid, 0.f, true);

		UNoclipButton* Next = NewButton(WidgetTree, FText::FromString(TEXT("›")), ENoclipButtonKind::Normal, ENoclipButtonStyle::Step);
		Next->OnClicked().AddWeakLambda(this, [this]() { Rotate(+1); });
		AddH(Nav, Sized(WidgetTree, Next, 48.f, 48.f));
		Place(Canvas, Sized(WidgetTree, Nav, 680.f), FAnchors(0.f, 0.f), FVector2D::ZeroVector, FMargin(80.f, 608.f, 0.f, 0.f), true);
	}

	// ---------- ficha: right 64, top 56, width 420 ----------
	{
		UVerticalBox* Aside = WidgetTree->ConstructWidget<UVerticalBox>();

		UBorder* Card = Box(WidgetTree, Manila(), FMargin(20.f, 18.f));
		UOverlay* CardStack = WidgetTree->ConstructWidget<UOverlay>();
		Card->SetContent(CardStack);
		UVerticalBox* Col = WidgetTree->ConstructWidget<UVerticalBox>();
		AddOverlay(CardStack, Col, HAlign_Fill, VAlign_Top);

		FileText = Txt(WidgetTree, FText::GetEmpty(), Mono(12.f), Ink());
		AddV(Col, SpaceBetween(WidgetTree, Txt(WidgetTree, Upper(LOCTEXT("Sheet", "Ficha de admissão")), Oswald(18.f, false, 0.08f), Ink()), FileText, VAlign_Bottom));

		// Foto 3x4 + nome
		{
			UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>();
			PhotoFrame = Box(WidgetTree, Text(), FMargin(0.f), ManilaRule(), 1.f);
			// Render do rosto (ANoclipPortraitStudio), segue o escolhido no carrossel.
			PhotoImage = WidgetTree->ConstructWidget<UImage>();
			PhotoImage->SetVisibility(ESlateVisibility::HitTestInvisible);
			PhotoFrame->SetPadding(FMargin(1.f));
			PhotoFrame->SetHorizontalAlignment(HAlign_Fill);
			PhotoFrame->SetVerticalAlignment(VAlign_Fill);
			PhotoFrame->SetContent(PhotoImage);
			PhotoFrame->SetRenderTransformAngle(-2.f);
			UBorder* Shadow = Box(WidgetTree, Hex(TEXT("1A1508"), 0.25f), FMargin(0.f, 0.f, 2.f, 3.f));
			Shadow->SetContent(Sized(WidgetTree, PhotoFrame, 84.f, 104.f));
			AddH(Row, Shadow, 0.f, false, VAlign_Top);

			UVerticalBox* Who = WidgetTree->ConstructWidget<UVerticalBox>();
			NameText = Wrap(Txt(WidgetTree, FText::GetEmpty(), Oswald(28.f, true, 0.02f), Ink()), 1.05f);
			AddV(Who, NameText);
			NickText = Txt(WidgetTree, FText::GetEmpty(), Mono(14.f, 600), Ink());
			AddV(Who, NickText, 4.f);
			RoleText = Wrap(Txt(WidgetTree, FText::GetEmpty(), Mono(13.f), Ink()), 1.4f);
			AddV(Who, RoleText, 4.f);
			UHorizontalBoxSlot* WhoSlot = AddH(Row, Who, 14.f, true, VAlign_Top);
			WhoSlot->SetPadding(FMargin(14.f, 0.f, 64.f, 0.f));
			AddV(Col, Row, 10.f);
		}

		// Dados (3 colunas)
		{
			UUniformGridPanel* Grid = WidgetTree->ConstructWidget<UUniformGridPanel>();
			Grid->SetSlotPadding(FMargin(0.f, 0.f, 12.f, 8.f));
			for (int32 i = 0; i < 6; ++i)
			{
				UVerticalBox* Cell = WidgetTree->ConstructWidget<UVerticalBox>();
				UTextBlock* K = Txt(WidgetTree, FText::GetEmpty(), Mono(10.f, 400, 0.14f), Ink());
				UTextBlock* V = Txt(WidgetTree, FText::GetEmpty(), Mono(15.f, 600), Ink());
				V->SetTextOverflowPolicy(ETextOverflowPolicy::Ellipsis);
				AddV(Cell, K);
				AddV(Cell, V, 2.f);
				AddV(Cell, Dashed(WidgetTree, ManilaRule()), 5.f);
				FactKeys.Add(K);
				FactValues.Add(V);
				AddCell(Grid, Cell, i / 3, i % 3);
			}
			AddV(Col, Grid, 10.f);
		}

		AddV(Col, Txt(WidgetTree, Upper(LOCTEXT("History", "Histórico")), Mono(11.f, 400, 0.14f), Ink()), 2.f);
		StoryText = Wrap(Txt(WidgetTree, FText::GetEmpty(), Mono(13.f), Ink()), 1.5f);
		AddV(Col, StoryText, 6.f);

		Traits = WidgetTree->ConstructWidget<UWrapBox>();
		Traits->SetInnerSlotPadding(FVector2D(6.f, 6.f));
		AddV(Col, Traits, 10.f);

		AddV(Col, Wrap(Txt(WidgetTree, LOCTEXT("OnlyLooks", "Só visual: todo estagiário tem os mesmos verbos, fôlego e força."), Mono(11.f), Hex(TEXT("4A4230"))), 1.4f), 10.f);

		// Carimbo (absoluto: direita 18, topo 46 do cartao)
		StampText = Txt(WidgetTree, FText::GetEmpty(), Oswald(13.f, false, 0.12f), Hex(TEXT("2F5A1E")));
		StampBox = Box(WidgetTree, FLinearColor::Transparent, FMargin(8.f, 3.f), Hex(TEXT("2F5A1E")), 2.f);
		StampBox->SetContent(StampText);
		StampBox->SetRenderTransformAngle(8.f);
		StampBox->SetVisibility(ESlateVisibility::HitTestInvisible);
		AddOverlay(CardStack, StampBox, HAlign_Right, VAlign_Top, FMargin(0.f, 28.f, -2.f, 0.f));

		AddV(Aside, Card);

		UHorizontalBox* Buttons = WidgetTree->ConstructWidget<UHorizontalBox>();
		RandomButton = NewButton(WidgetTree, LOCTEXT("Random", "Aleatório"), ENoclipButtonKind::Normal, ENoclipButtonStyle::Compact);
		RandomButton->SetFontPx(18.f);
		RandomButton->SetMinHeight(52.f);
		RandomButton->OnClicked().AddWeakLambda(this, [this]() { PickRandom(); });
		UHorizontalBoxSlot* RS = AddH(Buttons, RandomButton, 0.f, true, VAlign_Fill);
		RS->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		WearButton = NewButton(WidgetTree, LOCTEXT("Wear", "Vestir crachá"), ENoclipButtonKind::Primary);
		WearButton->SetMinHeight(52.f);
		WearButton->OnClicked().AddWeakLambda(this, [this]() { Wear(); });
		UHorizontalBoxSlot* WS = AddH(Buttons, WearButton, 12.f, true, VAlign_Fill);
		FSlateChildSize Two(ESlateSizeRule::Fill);
		Two.Value = 2.f;
		WS->SetSize(Two);
		AddV(Aside, Buttons, 12.f);

		AlsoNote = Wrap(Txt(WidgetTree, FText::GetEmpty(), Mono(12.f), Muted()));
		AddV(Aside, AlsoNote, 12.f);

		Place(Canvas, Sized(WidgetTree, Aside, 420.f), FAnchors(1.f, 0.f), FVector2D(1.f, 0.f), FMargin(-64.f, 56.f, 0.f, 0.f), true);
	}

	// ---------- footer: left 80, width 680, bottom 28 ----------
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
		Hint(KeyPrev, &KeyNext, LOCTEXT("Turn", "girar"), 0.f);
		Hint(KeyRandom, nullptr, LOCTEXT("RandomHint", "aleatório"), 20.f);
		Hint(KeyWear, nullptr, LOCTEXT("WearHint", "vestir crachá"), 20.f);
		Hint(KeyBack, nullptr, LOCTEXT("BackHint", "voltar"), 20.f);
		AddH(Footer, Hints, 0.f, true);
		AddH(Footer, Txt(WidgetTree, FText::FromString(TEXT("NOCLIP_0.1")), Mono(13.f), Muted()));
		Place(Canvas, Sized(WidgetTree, Footer, 680.f), FAnchors(0.f, 1.f), FVector2D(0.f, 1.f), FMargin(80.f, -28.f, 0.f, 0.f), true);
	}

	DefaultFocusWidget = WearButton;
}

void UNoclipCharacterSelectScreen::NativeOnActivated()
{
	// Abre no cracha que ja esta vestido.
	Position = NoclipCharacters::GetLocalChoice();
	if (const APlayerController* PC = GetOwningPlayer())
	{
		if (const ANoclipPlayerState* PS = PC->GetPlayerState<ANoclipPlayerState>())
		{
			Position = PS->GetCharacterIndex();
		}
	}
	// Aberta pelo cracha do menu inicial ou pela sala de espera.
	const ANoclipPlayerController* NPC = GetNoclipPC();
	bFromWaitingRoom = NPC && NPC->IsWaitingRoom();
	BackLink->SetLabel(bFromWaitingRoom ? LOCTEXT("Back", "Voltar à sala de espera") : LOCTEXT("BackMenu", "Voltar ao início"));
	ShownSlot = -1;
	AlsoSignature.Reset();
	SpawnStage();
	RefreshSheet();
	RefreshHints();
	Super::NativeOnActivated();
}

void UNoclipCharacterSelectScreen::NativeOnDeactivated()
{
	DestroyStage();
	Super::NativeOnDeactivated();
}

void UNoclipCharacterSelectScreen::NativeDestruct()
{
	DestroyStage();
	Super::NativeDestruct();
}

void UNoclipCharacterSelectScreen::SpawnStage()
{
	UWorld* World = GetWorld();
	if (!World || IsValid(Stage))
	{
		return;
	}
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	Stage = World->SpawnActor<ANoclipCharacterStage>(ANoclipCharacterStage::StaticClass(), FTransform(StageLocation), Params);
	bImageBound = false;
	if (!IsValid(Portrait))
	{
		Portrait = ANoclipPortraitStudio::SpawnStudio(World, GetSelected());
		if (UMaterialInstanceDynamic* MID = Portrait ? Portrait->MakeUIMaterial(this) : nullptr)
		{
			PhotoImage->SetBrushFromMaterial(MID);
		}
	}
	if (Stage)
	{
		Stage->SetPosition(Position);
	}
}

void UNoclipCharacterSelectScreen::DestroyStage()
{
	if (IsValid(Stage))
	{
		Stage->Destroy();
	}
	Stage = nullptr;
	if (IsValid(Portrait))
	{
		Portrait->Destroy();
	}
	Portrait = nullptr;
	bImageBound = false;
}

int32 UNoclipCharacterSelectScreen::GetSelected() const
{
	const int32 N = FMath::Max(NoclipCharacters::NumSlots(), 1);
	return ((Position % N) + N) % N;
}

void UNoclipCharacterSelectScreen::Rotate(int32 Delta)
{
	Position += Delta;
	if (Stage)
	{
		Stage->SetPosition(Position);
	}
	RefreshSheet();
}

void UNoclipCharacterSelectScreen::PickRandom()
{
	// Sorteia um jogavel e da uma volta inteira no carrossel ate ele (como no prototipo).
	const int32 N = NoclipCharacters::NumSlots();
	const int32 Playable = FMath::Max(NoclipCharacters::NumPlayable(), 1);
	const int32 Target = FMath::RandRange(0, Playable - 1);
	int32 Delta = Target - GetSelected();
	if (Delta <= 0)
	{
		Delta += N;
	}
	Rotate(Delta + N);
}

void UNoclipCharacterSelectScreen::Wear()
{
	const int32 Sel = GetSelected();
	if (NoclipCharacters::Get(Sel).bLocked)
	{
		return;
	}
	if (ANoclipPlayerController* PC = GetNoclipPC())
	{
		PC->RequestSetCharacter(Sel);
	}
	else
	{
		NoclipCharacters::SetLocalChoice(Sel);
	}
	if (UNoclipPrimaryLayout* L = GetLayout())
	{
		L->ShowToast(FText::Format(LOCTEXT("Wearing", "Crachá vestido: {0}."), NoclipCharacters::Get(Sel).Name), false);
	}
	DeactivateWidget();
}

FReply UNoclipCharacterSelectScreen::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	const FKey Key = InKeyEvent.GetKey();
	if (IsPrevKey(Key))
	{
		Rotate(-1);
		return FReply::Handled();
	}
	if (IsNextKey(Key))
	{
		Rotate(+1);
		return FReply::Handled();
	}
	if (Key == EKeys::R || Key == EKeys::Gamepad_FaceButton_Left)
	{
		PickRandom();
		return FReply::Handled();
	}
	return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

void UNoclipCharacterSelectScreen::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	if (!bImageBound && Stage && Stage->GetTarget())
	{
		bImageBound = true;
		UMaterialInterface* FeedMat = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/NOCLIP/UI/M_UI_CameraFeed.M_UI_CameraFeed"));
		if (UMaterialInstanceDynamic* MID = FeedMat ? UMaterialInstanceDynamic::Create(FeedMat, this) : nullptr)
		{
			MID->SetTextureParameterValue(TEXT("Feed"), Stage->GetTarget());
			CarouselImage->SetBrushFromMaterial(MID);
		}
		else
		{
			CarouselImage->SetBrushResourceObject(Stage->GetTarget());
		}
	}

	RefreshAccumulator += InDeltaTime;
	if (RefreshAccumulator >= 0.25f)
	{
		RefreshAccumulator = 0.f;
		RefreshSheet();
		RefreshHints();
	}
}

void UNoclipCharacterSelectScreen::RefreshHints()
{
	const APlayerController* PC = GetOwningPlayer();
	const bool bPad = NoclipGlyphs::IsUsingGamepad(PC);
	const bool bPS = bPad && NoclipGlyphs::GetPadStyle(PC) == ENoclipPadStyle::PlayStation;
	KeyPrev->SetText(FText::FromString(bPad ? (bPS ? TEXT("L1") : TEXT("LB")) : TEXT("Q")));
	KeyNext->SetText(FText::FromString(bPad ? (bPS ? TEXT("R1") : TEXT("RB")) : TEXT("E")));
	KeyRandom->SetText(FText::FromString(bPad ? (bPS ? TEXT("□") : TEXT("X")) : TEXT("R")));
	KeyWear->SetText(NoclipGlyphs::GetKeyText(ENoclipPrompt::Confirm, PC));
	KeyBack->SetText(NoclipGlyphs::GetKeyText(ENoclipPrompt::Back, PC));
	BackLink->SetSuffix(NoclipGlyphs::GetKeyText(ENoclipPrompt::Back, PC), true);
	WearButton->SetSuffix(NoclipGlyphs::GetKeyText(ENoclipPrompt::Confirm, PC), true);
}

void UNoclipCharacterSelectScreen::RefreshSheet()
{
	const int32 Sel = GetSelected();
	const FNoclipCharacterDef& Def = NoclipCharacters::Get(Sel);

	if (Sel != ShownSlot)
	{
		ShownSlot = Sel;
		AlsoSignature.Reset();

		CurrentName->SetText(Upper(Def.Short));
		FileText->SetText(Def.File);
		NameText->SetText(Upper(Def.Name));
		NickText->SetText(Def.Nick);
		RoleText->SetText(Def.Role);
		StoryText->SetText(Def.Story);
		for (int32 i = 0; i < FactKeys.Num(); ++i)
		{
			const bool bHas = Def.Facts.IsValidIndex(i);
			FactKeys[i]->SetText(bHas ? Upper(Def.Facts[i].Key) : FText::GetEmpty());
			FactValues[i]->SetText(bHas ? Def.Facts[i].Value : FText::GetEmpty());
		}

		if (Portrait)
		{
			Portrait->SetCharacter(Sel);
		}
		PhotoFrame->SetRenderOpacity(Def.bLocked ? 0.6f : 1.f);

		const FLinearColor StampColor = Hex(Def.bLocked ? TEXT("7A3A2E") : TEXT("2F5A1E"));
		StampText->SetText(Upper(Def.Stamp));
		StampText->SetColorAndOpacity(FSlateColor(StampColor));
		SetBox(StampBox, FLinearColor::Transparent, StampColor, 2.f);

		Traits->ClearChildren();
		for (const FText& T : Def.Traits)
		{
			UBorder* Chip = Box(WidgetTree, FLinearColor::Transparent, FMargin(7.f, 2.f), ManilaRule(), 1.f);
			Chip->SetContent(Txt(WidgetTree, T, Mono(11.f), Ink()));
			Traits->AddChildToWrapBox(Chip);
		}

		Dots->ClearChildren();
		for (int32 i = 0; i < NoclipCharacters::NumSlots(); ++i)
		{
			const bool bOn = i == Sel;
			const FLinearColor C = bOn ? Amber() : (NoclipCharacters::Get(i).bLocked ? Line() : Muted());
			AddH(Dots, Sized(WidgetTree, Box(WidgetTree, C, FMargin(0.f)), bOn ? 22.f : 8.f, 8.f), i == 0 ? 0.f : 8.f);
		}

		WearButton->SetIsEnabled(!Def.bLocked);
	}

	// Botao: ja vestido? / quem mais esta com este cracha.
	const ANoclipPlayerState* LocalPS = GetOwningPlayer() ? GetOwningPlayer()->GetPlayerState<ANoclipPlayerState>() : nullptr;
	const int32 Worn = LocalPS ? LocalPS->GetCharacterIndex() : NoclipCharacters::GetLocalChoice();
	const bool bCurrent = !Def.bLocked && Worn == Sel;
	WearButton->SetLabel(Def.bLocked ? LOCTEXT("Closed", "Vaga fechada") : (bCurrent ? LOCTEXT("Current", "Crachá atual ✓") : LOCTEXT("Wear", "Vestir crachá")));

	FString Others;
	if (!Def.bLocked)
	{
		if (const ANoclipGameState* GS = GetNoclipGameState())
		{
			for (const APlayerState* PS : GS->PlayerArray)
			{
				const ANoclipPlayerState* NPS = Cast<ANoclipPlayerState>(PS);
				if (NPS && NPS != LocalPS && NPS->GetCharacterIndex() == Sel)
				{
					Others += (Others.IsEmpty() ? TEXT("") : TEXT(", ")) + ShortName(NPS->GetPlayerName(), 16);
				}
			}
		}
	}
	const FString Signature = FString::Printf(TEXT("%d|%d|%s"), Sel, bCurrent ? 1 : 0, *Others);
	if (Signature == AlsoSignature)
	{
		return;
	}
	AlsoSignature = Signature;
	if (Def.bLocked)
	{
		AlsoNote->SetText(LOCTEXT("LockedNote", "Escolha a Regina ou o Davi para descer neste turno."));
	}
	else if (!bFromWaitingRoom)
	{
		AlsoNote->SetText(LOCTEXT("MenuNote", "Vale para os próximos expedientes. Dá para trocar de novo na sala de espera."));
	}
	else if (!Others.IsEmpty())
	{
		AlsoNote->SetText(FText::Format(LOCTEXT("AlsoWearing", "Também vestindo este crachá: {0}. Dá para repetir estagiário."), FText::FromString(Others)));
	}
	else
	{
		AlsoNote->SetText(LOCTEXT("NobodyElse", "Ninguém da equipe está com este crachá ainda."));
	}
}

#undef LOCTEXT_NAMESPACE
