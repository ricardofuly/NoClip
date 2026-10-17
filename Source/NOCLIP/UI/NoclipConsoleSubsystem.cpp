// Copyright Fuly Games. All Rights Reserved.

#include "UI/NoclipConsoleSubsystem.h"
#include "Engine/Console.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Framework/Application/IInputProcessor.h"
#include "Framework/Application/SlateApplication.h"
#include "GameFramework/InputSettings.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(NoclipConsoleSubsystem)

namespace
{
	UConsole* GetViewportConsole()
	{
		return (GEngine && GEngine->GameViewport) ? GEngine->GameViewport->ViewportConsole.Get() : nullptr;
	}
}

class FNoclipConsoleKeyProcessor : public IInputProcessor
{
public:
	virtual void Tick(const float DeltaTime, FSlateApplication& SlateApp, TSharedRef<ICursor> Cursor) override
	{
		// Enquanto o console esta aberto, as teclas tem que ir para o viewport (e nao para o botao focado).
		const UConsole* Console = GetViewportConsole();
		if (Console && Console->ConsoleActive() && GEngine->GameViewport->GetGameViewportWidget().IsValid())
		{
			const TSharedPtr<SWidget> Focused = SlateApp.GetKeyboardFocusedWidget();
			const TSharedPtr<SWidget> ViewportWidget = GEngine->GameViewport->GetGameViewportWidget();
			if (Focused != ViewportWidget)
			{
				SlateApp.SetAllUserFocusToGameViewport();
			}
		}
	}

	virtual bool HandleKeyDownEvent(FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent) override
	{
		UConsole* Console = GetViewportConsole();
		if (!Console || InKeyEvent.IsRepeat() || !IsConsoleKey(InKeyEvent.GetKey()))
		{
			return false;
		}
		// Ja aberto: o proprio console fecha (a tecla chega pelo viewport, que esta com o foco).
		if (Console->ConsoleActive())
		{
			return false;
		}
		// Digitando num campo de texto (codigo da sala, nome): a tecla e so um caractere.
		const TSharedPtr<SWidget> Focused = SlateApp.GetKeyboardFocusedWidget();
		if (Focused.IsValid() && (Focused->GetType() == TEXT("SEditableText") || Focused->GetType() == TEXT("SMultiLineEditableText")))
		{
			return false;
		}
		Console->FakeGotoState(FName(TEXT("Typing")));
		SlateApp.SetAllUserFocusToGameViewport();
		return true;
	}

	static bool IsConsoleKey(const FKey& Key)
	{
		const UInputSettings* Settings = GetDefault<UInputSettings>();
		return Settings && Settings->ConsoleKeys.Contains(Key);
	}
};

void UNoclipConsoleSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
#if !UE_BUILD_SHIPPING
	if (FSlateApplication::IsInitialized())
	{
		Processor = MakeShared<FNoclipConsoleKeyProcessor>();
		// Indice 0: antes dos outros pre-processadores (CommonUI, apertar para falar).
		FSlateApplication::Get().RegisterInputPreProcessor(Processor, 0);
	}
#endif
}

void UNoclipConsoleSubsystem::Deinitialize()
{
	if (Processor.IsValid() && FSlateApplication::IsInitialized())
	{
		FSlateApplication::Get().UnregisterInputPreProcessor(Processor);
	}
	Processor.Reset();
	Super::Deinitialize();
}

bool UNoclipConsoleSubsystem::IsConsoleOpen()
{
	const UConsole* Console = GetViewportConsole();
	return Console && Console->ConsoleActive();
}
