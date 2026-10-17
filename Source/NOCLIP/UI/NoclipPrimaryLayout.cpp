// Copyright Fuly Games. All Rights Reserved.

#include "UI/NoclipPrimaryLayout.h"
#include "UI/NoclipHUDWidget.h"
#include "UI/NoclipPauseMenu.h"
#include "UI/NoclipConfirmDialog.h"
#include "UI/NoclipShiftReportWidget.h"
#include "UI/NoclipLobbyScreen.h"
#include "UI/NoclipMainMenu.h"
#include "UI/NoclipWaitingRoomScreen.h"
#include "UI/NoclipInputGlyphs.h"
#include "UI/NoclipUIStyle.h"
#include "Game/NoclipGameState.h"
#include "Game/NoclipUserSettings.h"
#include "Widgets/CommonActivatableWidgetContainer.h"
#include "Blueprint/WidgetTree.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/Border.h"
#include "Components/TextBlock.h"
#include "Components/Image.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "GameFramework/PlayerController.h"
#include "Engine/World.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(NoclipPrimaryLayout)

using namespace NoclipUI;

void UNoclipPrimaryLayout::InitializeNativeClassData()
{
	Super::InitializeNativeClassData();

	if (!WidgetTree)
	{
		WidgetTree = NewObject<UWidgetTree>(this, TEXT("WidgetTree"), RF_Transient);
	}
	if (WidgetTree->RootWidget)
	{
		return;
	}

	UOverlay* Root = WidgetTree->ConstructWidget<UOverlay>();
	auto Fill = [Root](UWidget* W) -> UOverlaySlot*
	{
		UOverlaySlot* S = Root->AddChildToOverlay(W);
		S->SetHorizontalAlignment(HAlign_Fill);
		S->SetVerticalAlignment(VAlign_Fill);
		return S;
	};

	GameStack = WidgetTree->ConstructWidget<UCommonActivatableWidgetStack>(UCommonActivatableWidgetStack::StaticClass(), TEXT("GameLayer"));
	GameStack->SetTransitionDuration(0.f);
	Fill(GameStack);

	MenuStack = WidgetTree->ConstructWidget<UCommonActivatableWidgetStack>(UCommonActivatableWidgetStack::StaticClass(), TEXT("MenuLayer"));
	MenuStack->SetTransitionDuration(0.12f);
	Fill(MenuStack);

	ModalStack = WidgetTree->ConstructWidget<UCommonActivatableWidgetStack>(UCommonActivatableWidgetStack::StaticClass(), TEXT("ModalLayer"));
	ModalStack->SetTransitionDuration(0.08f);
	Fill(ModalStack);

	// Aguarde (bloqueia o mouse; as telas tambem checam IsBusy antes de agir).
	BusyPanel = Box(WidgetTree, Hex(TEXT("080705"), 0.82f), FMargin(0.f));
	BusyPanel->SetHorizontalAlignment(HAlign_Center);
	BusyPanel->SetVerticalAlignment(VAlign_Center);
	BusyText = Txt(WidgetTree, FText::GetEmpty(), Oswald(40.f, true, 0.03f), Amber());
	BusyText->SetJustification(ETextJustify::Center);
	BusyPanel->SetContent(BusyText);
	BusyPanel->SetVisibility(ESlateVisibility::Collapsed);
	Fill(BusyPanel);

	// Aviso no rodape (role=alert no prototipo).
	ToastPanel = Box(WidgetTree, Hex(TEXT("1E1310")), FMargin(18.f, 12.f), DangerLine(), 1.f);
	ToastText = Txt(WidgetTree, FText::GetEmpty(), Mono(15.f, 600), Hex(TEXT("E7C9C0")));
	ToastPanel->SetContent(ToastText);
	ToastPanel->SetVisibility(ESlateVisibility::Collapsed);
	UOverlaySlot* ToastSlot = Root->AddChildToOverlay(ToastPanel);
	ToastSlot->SetHorizontalAlignment(HAlign_Center);
	ToastSlot->SetVerticalAlignment(VAlign_Bottom);
	ToastSlot->SetPadding(FMargin(0.f, 0.f, 0.f, 72.f));

	// Linhas de varredura (repeating-linear-gradient do prototipo), por cima de tudo e sem receber clique.
	Scanlines = WidgetTree->ConstructWidget<UImage>();
	if (UMaterialInterface* Mat = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/NOCLIP/UI/M_UI_Scanlines.M_UI_Scanlines")))
	{
		Scanlines->SetBrushFromMaterial(Mat);
		ScanlinesMID = Scanlines->GetDynamicMaterial();
	}
	Scanlines->SetVisibility(ESlateVisibility::HitTestInvisible);
	Fill(Scanlines);

	WidgetTree->RootWidget = Root;
}

void UNoclipPrimaryLayout::ShowHUD()
{
	if (!HUD && GameStack)
	{
		HUD = GameStack->AddWidget<UNoclipHUDWidget>(UNoclipHUDWidget::StaticClass());
	}
}

void UNoclipPrimaryLayout::ShowMainMenu()
{
	bFrontend = true;
	if (MenuStack && !MenuStack->GetActiveWidget())
	{
		MenuStack->AddWidget<UNoclipMainMenu>(UNoclipMainMenu::StaticClass());
	}
}

void UNoclipPrimaryLayout::ShowWaitingRoom()
{
	bWaitingRoom = true;
	if (MenuStack && !MenuStack->GetActiveWidget())
	{
		MenuStack->AddWidget<UNoclipWaitingRoomScreen>(UNoclipWaitingRoomScreen::StaticClass());
	}
}

bool UNoclipPrimaryLayout::IsMenuOpen() const
{
	return GetTopMenu() != nullptr;
}

UCommonActivatableWidget* UNoclipPrimaryLayout::GetTopMenu() const
{
	if (ModalStack && ModalStack->GetActiveWidget())
	{
		return ModalStack->GetActiveWidget();
	}
	return MenuStack ? MenuStack->GetActiveWidget() : nullptr;
}

UCommonActivatableWidget* UNoclipPrimaryLayout::PushMenu(TSubclassOf<UCommonActivatableWidget> WidgetClass)
{
	return MenuStack ? MenuStack->AddWidget<UCommonActivatableWidget>(WidgetClass) : nullptr;
}

void UNoclipPrimaryLayout::TogglePauseMenu()
{
	if (bFrontend || bWaitingRoom || !MenuStack || (ModalStack && ModalStack->GetActiveWidget()))
	{
		return;
	}
	UCommonActivatableWidget* Active = MenuStack->GetActiveWidget();
	if (Active && Active->IsA<UNoclipPauseMenu>())
	{
		Active->DeactivateWidget();
		return;
	}
	// Pausa abre por cima da HUD ou do saguao; relatorio/opcoes tratam o Voltar sozinhos.
	if (!Active || Active->IsA<UNoclipLobbyScreen>())
	{
		PushMenu(UNoclipPauseMenu::StaticClass());
	}
}

void UNoclipPrimaryLayout::PushConfirm(const FText& Title, const FText& Body, const FText& ConfirmLabel, TFunction<void()> OnConfirm, const FText& Eyebrow)
{
	if (!ModalStack)
	{
		return;
	}
	ModalStack->AddWidget<UNoclipConfirmDialog>(UNoclipConfirmDialog::StaticClass(), [&](UNoclipConfirmDialog& Dialog)
	{
		Dialog.Setup(Title, Body, ConfirmLabel, MoveTemp(OnConfirm), Eyebrow);
	});
}

void UNoclipPrimaryLayout::SetBusy(const FText& Message)
{
	bBusy = !Message.IsEmpty();
	if (BusyText)
	{
		BusyText->SetText(Upper(Message));
	}
	if (BusyPanel)
	{
		BusyPanel->SetVisibility(bBusy ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}
}

void UNoclipPrimaryLayout::ShowToast(const FText& Message, bool bError)
{
	if (!ToastPanel || Message.IsEmpty())
	{
		return;
	}
	ToastText->SetText(Message);
	SetBox(ToastPanel, bError ? Hex(TEXT("1E1310")) : Hex(TEXT("15200F")), bError ? DangerLine() : OkDeep(), 1.f);
	ToastText->SetColorAndOpacity(FSlateColor(bError ? Hex(TEXT("E7C9C0")) : OkText()));
	ToastPanel->SetVisibility(ESlateVisibility::HitTestInvisible);
	ToastRemaining = 5.f;
}

void UNoclipPrimaryLayout::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	if (!bFrontend && !bWaitingRoom)
	{
		TryBindGameState();
		SyncPhaseScreens();
	}
	RefreshCursor();
	RefreshScanlines();

	if (ToastRemaining > 0.f)
	{
		ToastRemaining -= InDeltaTime;
		if (ToastRemaining <= 0.f && ToastPanel)
		{
			ToastPanel->SetVisibility(ESlateVisibility::Collapsed);
		}
	}
}

void UNoclipPrimaryLayout::RefreshScanlines()
{
	if (!Scanlines)
	{
		return;
	}
	const bool bOn = FNoclipUserSettings::Get().bScanlines && ScanlinesMID;
	const ESlateVisibility Wanted = bOn ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed;
	if (Scanlines->GetVisibility() != Wanted)
	{
		Scanlines->SetVisibility(Wanted);
	}
	if (!bOn)
	{
		return;
	}
	// 1px escuro a cada 3px do artboard de 720 linhas, escalado para a resolucao real.
	const float Scale = FMath::Max(1.f, UWidgetLayoutLibrary::GetViewportSize(this).Y / 720.f);
	if (!FMath::IsNearlyEqual(Scale, LastScanScale))
	{
		LastScanScale = Scale;
		ScanlinesMID->SetScalarParameterValue(TEXT("Period"), 3.f * Scale);
		ScanlinesMID->SetScalarParameterValue(TEXT("Thick"), FMath::Max(1.f, FMath::RoundToFloat(Scale)));
		ScanlinesMID->SetScalarParameterValue(TEXT("Alpha"), 0.22f);
	}
}

void UNoclipPrimaryLayout::NativeDestruct()
{
	if (ANoclipGameState* GS = BoundGameState.Get())
	{
		GS->OnShiftEnded.RemoveDynamic(this, &UNoclipPrimaryLayout::HandleShiftEnded);
		GS->OnPhaseChanged.RemoveDynamic(this, &UNoclipPrimaryLayout::HandlePhaseChanged);
	}
	BoundGameState.Reset();
	Super::NativeDestruct();
}

void UNoclipPrimaryLayout::TryBindGameState()
{
	if (BoundGameState.IsValid())
	{
		return;
	}
	const UWorld* World = GetWorld();
	ANoclipGameState* GS = World ? World->GetGameState<ANoclipGameState>() : nullptr;
	if (!GS)
	{
		return;
	}
	BoundGameState = GS;
	GS->OnShiftEnded.AddDynamic(this, &UNoclipPrimaryLayout::HandleShiftEnded);
	GS->OnPhaseChanged.AddDynamic(this, &UNoclipPrimaryLayout::HandlePhaseChanged);

	if (GS->GetPhase() == ENoclipShiftPhase::Ended)
	{
		ShowShiftReport();
	}
}

void UNoclipPrimaryLayout::SyncPhaseScreens()
{
	const ANoclipGameState* GS = BoundGameState.Get();
	if (!GS || !MenuStack)
	{
		return;
	}
	// Saguao: tela cheia enquanto a fase for Lobby (fecha sozinho quando o turno comeca).
	// Chegando da sala de espera: o elevador ainda esta descendo e o turno 1 comeca sozinho (sem saguao).
	const bool bLobby = GS->GetPhase() == ENoclipShiftPhase::Lobby && !GS->IsArrivalPending();
	if (bLobby && !ActiveLobby.IsValid() && !ActiveReport.IsValid())
	{
		ActiveLobby = MenuStack->AddWidget<UNoclipLobbyScreen>(UNoclipLobbyScreen::StaticClass());
	}
	else if (!bLobby && ActiveLobby.IsValid())
	{
		// Fecha o saguao e qualquer coisa aberta por cima dele (pausa, opcoes).
		MenuStack->ClearWidgets();
		ActiveLobby.Reset();
	}
}

void UNoclipPrimaryLayout::HandleShiftEnded(const FNoclipShiftResult& Result)
{
	ShowShiftReport();
}

void UNoclipPrimaryLayout::HandlePhaseChanged(ENoclipShiftPhase OldPhase, ENoclipShiftPhase NewPhase)
{
	if (NewPhase == ENoclipShiftPhase::Ended)
	{
		ShowShiftReport();
		return;
	}
	bReportShownThisEnd = false;

	if (UCommonActivatableWidget* Report = ActiveReport.Get())
	{
		Report->DeactivateWidget();
	}
	ActiveReport.Reset();
}

void UNoclipPrimaryLayout::ShowShiftReport()
{
	const ANoclipGameState* GS = BoundGameState.Get();
	if (!GS || !MenuStack || ActiveReport.IsValid())
	{
		return;
	}
	if (GS->GetPhase() != ENoclipShiftPhase::Ended || GS->GetLastResult().ShiftIndex <= 0 || bReportShownThisEnd)
	{
		return;
	}
	bReportShownThisEnd = true;

	if (ModalStack)
	{
		ModalStack->ClearWidgets();
	}
	MenuStack->ClearWidgets();
	ActiveLobby.Reset();

	// Guarda o resumo para o cracha do menu principal.
	const FNoclipShiftResult& R = GS->GetLastResult();
	FNoclipUserSettings& Settings = FNoclipUserSettings::Get();
	Settings.LastShiftSummary = FString::Printf(TEXT("turno %d, %s"), R.ShiftIndex, R.bFired ? TEXT("demitidos") : TEXT("quota batida"));
	Settings.Save();

	if (R.bFired)
	{
		ActiveReport = MenuStack->AddWidget<UNoclipFiredScreen>(UNoclipFiredScreen::StaticClass());
	}
	else
	{
		ActiveReport = MenuStack->AddWidget<UNoclipShiftReportWidget>(UNoclipShiftReportWidget::StaticClass());
	}
}

void UNoclipPrimaryLayout::RefreshCursor()
{
	APlayerController* PC = GetOwningPlayer();
	if (!PC)
	{
		return;
	}
	const bool bWantCursor = IsMenuOpen() && !NoclipGlyphs::IsUsingGamepad(PC);
	if (PC->ShouldShowMouseCursor() != bWantCursor)
	{
		PC->SetShowMouseCursor(bWantCursor);
	}
}
