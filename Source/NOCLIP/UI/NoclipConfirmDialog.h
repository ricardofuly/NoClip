// Copyright Fuly Games. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UI/NoclipActivatableWidget.h"
#include "NoclipConfirmDialog.generated.h"

class UNoclipButton;
class UTextBlock;

/**
 * Dialogo de confirmacao no estilo "memorando interno" (QuitConfirm do prototipo):
 * cartao manila 520px, Cancelar (primario, com o foco) e a acao destrutiva ao lado. Voltar tambem cancela.
 */
UCLASS()
class NOCLIP_API UNoclipConfirmDialog : public UNoclipActivatableWidget
{
	GENERATED_BODY()

public:
	void Setup(const FText& Title, const FText& Body, const FText& ConfirmLabel, TFunction<void()> InOnConfirm, const FText& Eyebrow = FText::GetEmpty());

protected:
	virtual void BuildTree() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
	void HandleCancel();
	void HandleConfirm();

	UTextBlock* EyebrowText = nullptr;
	UTextBlock* TitleText = nullptr;
	UTextBlock* BodyText = nullptr;
	UTextBlock* BackKey = nullptr;
	UNoclipButton* CancelButton = nullptr;
	UNoclipButton* ConfirmButton = nullptr;
	TFunction<void()> OnConfirm;
};
