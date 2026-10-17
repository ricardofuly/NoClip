// Copyright Fuly Games. All Rights Reserved.

#include "UI/NoclipActivatableWidget.h"
#include "UI/NoclipConsoleSubsystem.h"
#include "UI/NoclipPrimaryLayout.h"
#include "Game/NoclipGameState.h"
#include "Game/NoclipPlayerController.h"
#include "Blueprint/WidgetTree.h"
#include "Input/UIActionBindingHandle.h"
#include "Engine/World.h"
#include "GameFramework/PlayerState.h"
#include "InputCoreTypes.h"
#include "Framework/Application/SlateApplication.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(NoclipActivatableWidget)

void UNoclipActivatableWidget::InitializeNativeClassData()
{
	Super::InitializeNativeClassData();

	if (!WidgetTree)
	{
		WidgetTree = NewObject<UWidgetTree>(this, TEXT("WidgetTree"), RF_Transient);
	}
	if (!WidgetTree->RootWidget)
	{
		BuildTree();
	}
}

TOptional<FUIInputConfig> UNoclipActivatableWidget::GetDesiredInputConfig() const
{
	if (bIsMenu)
	{
		return FUIInputConfig(ECommonInputMode::Menu, EMouseCaptureMode::NoCapture);
	}
	return FUIInputConfig(ECommonInputMode::Game, EMouseCaptureMode::CapturePermanently_IncludingInitialMouseDown, true);
}

void UNoclipActivatableWidget::NativeOnActivated()
{
	Super::NativeOnActivated();

	if (bIsMenu)
	{
		if (UWidget* Target = GetDesiredFocusTarget())
		{
			Target->SetFocus();
		}
	}
}

void UNoclipActivatableWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	// Um clique no vazio tira o foco dos botoes; devolve para Voltar e o controle continuarem funcionando.
	const UNoclipPrimaryLayout* Layout = bIsMenu ? GetLayout() : nullptr;
	const bool bIsTop = Layout && Layout->GetTopMenu() == this;
	// Console aberto: o foco fica no viewport ate fechar.
	if (bIsTop && IsActivated() && !HasAnyUserFocus() && !HasFocusedDescendants() && !UNoclipConsoleSubsystem::IsConsoleOpen())
	{
		if (UWidget* Target = GetDesiredFocusTarget())
		{
			Target->SetFocus();
		}
	}
}

UWidget* UNoclipActivatableWidget::NativeGetDesiredFocusTarget() const
{
	return DefaultFocusWidget ? DefaultFocusWidget.Get() : Super::NativeGetDesiredFocusTarget();
}

FReply UNoclipActivatableWidget::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	if (bIsMenu && IsActivated())
	{
		const FKey Key = InKeyEvent.GetKey();
		// Digitando num campo de texto, P e so a letra P.
		bool bTyping = false;
		if (FSlateApplication::IsInitialized())
		{
			const TSharedPtr<SWidget> Focused = FSlateApplication::Get().GetKeyboardFocusedWidget();
			bTyping = Focused.IsValid() && (Focused->GetType() == TEXT("SEditableText") || Focused->GetType() == TEXT("SMultiLineEditableText"));
		}
		const bool bBack = Key == EKeys::Escape || (Key == EKeys::P && !bTyping) || Key == EKeys::Gamepad_FaceButton_Right || Key == EKeys::Gamepad_Special_Right;
		if (bBack && HandleBack())
		{
			return FReply::Handled();
		}
	}
	return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

bool UNoclipActivatableWidget::HandleBack()
{
	if (bClosesOnBack)
	{
		DeactivateWidget();
		return true;
	}
	return false;
}

ANoclipGameState* UNoclipActivatableWidget::GetNoclipGameState() const
{
	const UWorld* World = GetWorld();
	return World ? World->GetGameState<ANoclipGameState>() : nullptr;
}

ANoclipPlayerController* UNoclipActivatableWidget::GetNoclipPC() const
{
	return Cast<ANoclipPlayerController>(GetOwningPlayer());
}

UNoclipPrimaryLayout* UNoclipActivatableWidget::GetLayout() const
{
	return GetTypedOuter<UNoclipPrimaryLayout>() ? GetTypedOuter<UNoclipPrimaryLayout>() : (GetNoclipPC() ? GetNoclipPC()->GetUILayout() : nullptr);
}

bool UNoclipActivatableWidget::IsLeader() const
{
	const ANoclipGameState* GS = GetNoclipGameState();
	const APlayerController* PC = GetOwningPlayer();
	return GS && PC && PC->PlayerState && GS->GetLeader() == PC->PlayerState;
}

bool UNoclipActivatableWidget::IsHost() const
{
	const UWorld* World = GetWorld();
	return World && World->GetNetMode() != NM_Client;
}
