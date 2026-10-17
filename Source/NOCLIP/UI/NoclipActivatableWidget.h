// Copyright Fuly Games. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "CommonActivatableWidget.h"
#include "NoclipActivatableWidget.generated.h"

class ANoclipGameState;
class ANoclipPlayerController;
class UNoclipPrimaryLayout;

/**
 * Base das telas do NOCLIP. A arvore de widgets e montada em C++ (BuildTree), sem asset de Blueprint.
 * Menus pedem input de UI (o jogo para de receber input) e fecham com Voltar (Esc / B / Circulo / Start).
 * A HUD pede input de jogo.
 */
UCLASS(Abstract)
class NOCLIP_API UNoclipActivatableWidget : public UCommonActivatableWidget
{
	GENERATED_BODY()

public:
	virtual TOptional<FUIInputConfig> GetDesiredInputConfig() const override;

	bool IsMenu() const { return bIsMenu; }

protected:
	virtual void InitializeNativeClassData() override;
	virtual void NativeOnActivated() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual UWidget* NativeGetDesiredFocusTarget() const override;
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;

	/** Monta a arvore (WidgetTree ja existe). Deve definir WidgetTree->RootWidget. */
	virtual void BuildTree() {}

	/** Voltar foi pressionado. Retorne true se tratou. */
	virtual bool HandleBack();

	ANoclipGameState* GetNoclipGameState() const;
	ANoclipPlayerController* GetNoclipPC() const;
	UNoclipPrimaryLayout* GetLayout() const;

	/** Esta maquina e o host (listen server)? Sair encerra a sessao para todos. */
	bool IsHost() const;

	/** Este jogador e o lider do grupo (quem inicia o expediente)? */
	bool IsLeader() const;

	/** true = tela de menu (input de UI, cursor); false = HUD (input de jogo). */
	bool bIsMenu = true;

	/** Se Voltar fecha esta tela. */
	bool bClosesOnBack = true;

	/** Widget que recebe o foco ao abrir (gamepad/teclado). */
	UPROPERTY(Transient)
	TObjectPtr<UWidget> DefaultFocusWidget;
};
