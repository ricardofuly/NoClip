// Copyright Fuly Games. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UI/NoclipActivatableWidget.h"
#include "NoclipCharacterSelectScreen.generated.h"

class ANoclipCharacterStage;
class UBorder;
class UHorizontalBox;
class UImage;
class UNoclipButton;
class UTextBlock;
class UUniformGridPanel;
class UWrapBox;

/**
 * Ficha de admissao (CharacterSelect.dc.html): carrossel 3D com os modelos de verdade a esquerda e a
 * ficha manila do estagiario a direita. Vestir o cracha salva a escolha nesta maquina e avisa o servidor
 * (o PlayerState replica para todo mundo). So visual: todos tem os mesmos verbos, folego e forca.
 */
UCLASS()
class NOCLIP_API UNoclipCharacterSelectScreen : public UNoclipActivatableWidget
{
	GENERATED_BODY()

protected:
	virtual void BuildTree() override;
	virtual void NativeOnActivated() override;
	virtual void NativeOnDeactivated() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;

private:
	void Rotate(int32 Delta);
	void PickRandom();
	void Wear();
	int32 GetSelected() const;
	void RefreshSheet();
	void RefreshHints();
	void SpawnStage();
	void DestroyStage();

	UPROPERTY(Transient) TObjectPtr<ANoclipCharacterStage> Stage;
	UPROPERTY(Transient) TObjectPtr<class ANoclipPortraitStudio> Portrait;

	UNoclipButton* BackLink = nullptr;
	UImage* CarouselImage = nullptr;
	UTextBlock* CurrentName = nullptr;
	UHorizontalBox* Dots = nullptr;

	UTextBlock* FileText = nullptr;
	UImage* PhotoImage = nullptr;
	UBorder* PhotoFrame = nullptr;
	UTextBlock* NameText = nullptr;
	UTextBlock* NickText = nullptr;
	UTextBlock* RoleText = nullptr;
	UBorder* StampBox = nullptr;
	UTextBlock* StampText = nullptr;
	TArray<UTextBlock*> FactKeys;
	TArray<UTextBlock*> FactValues;
	UTextBlock* StoryText = nullptr;
	UWrapBox* Traits = nullptr;

	UNoclipButton* RandomButton = nullptr;
	UNoclipButton* WearButton = nullptr;
	UTextBlock* AlsoNote = nullptr;

	UTextBlock* KeyPrev = nullptr;
	UTextBlock* KeyNext = nullptr;
	UTextBlock* KeyWear = nullptr;
	UTextBlock* KeyBack = nullptr;
	UTextBlock* KeyRandom = nullptr;

	int32 Position = 0;
	int32 ShownSlot = -1;
	FString AlsoSignature;
	float RefreshAccumulator = 0.f;
	bool bImageBound = false;
	bool bFromWaitingRoom = false;
};
