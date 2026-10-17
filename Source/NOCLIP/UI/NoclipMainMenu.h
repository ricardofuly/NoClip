// Copyright Fuly Games. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UI/NoclipActivatableWidget.h"
#include "Session/NoclipSessionSubsystem.h"
#include "Game/NoclipUserSettings.h"
#include "Types/SlateEnums.h"
#include "NoclipMainMenu.generated.h"

class UNoclipButton;
class UBorder;
class UNoclipSlider;
class UTextBlock;
class UVerticalBox;
class UHorizontalBox;
class UEditableTextBox;
class UScrollBox;
class UWidget;

/** Tela inicial (Main.dc.html): titulo, menu, cracha do estagiario e rodape de teclas. */
UCLASS()
class NOCLIP_API UNoclipMainMenu : public UNoclipActivatableWidget
{
	GENERATED_BODY()

protected:
	virtual void BuildTree() override;
	virtual void NativeOnActivated() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual void NativeOnDeactivated() override;
	virtual bool HandleBack() override;

private:
	void HandleQuit();
	void RefreshBadgeCharacter();

	UNoclipButton* HostButton = nullptr;
	UTextBlock* BadgeId = nullptr;
	UTextBlock* BadgeName = nullptr;
	UTextBlock* BadgeCharacter = nullptr;
	UNoclipButton* BadgePhoto = nullptr;
	class UImage* BadgePhotoImage = nullptr;
	TWeakObjectPtr<class ANoclipPortraitStudio> PortraitStudio;
	int32 BadgeShownChoice = -1;
	UTextBlock* BadgeLast = nullptr;
	UTextBlock* KeyUp = nullptr;
	UTextBlock* KeyDown = nullptr;
	UTextBlock* KeyConfirm = nullptr;
	UTextBlock* KeyBack = nullptr;
	bool bPendingHostReopen = false;
	float RefreshAccumulator = 1.f;
};

/** Hospedar expediente (Host.dc.html). */
UCLASS()
class NOCLIP_API UNoclipHostMenu : public UNoclipActivatableWidget
{
	GENERATED_BODY()

protected:
	virtual void BuildTree() override;
	virtual void NativeOnActivated() override;
	virtual void NativeOnDeactivated() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
	void Refresh();
	void HandleCreate();

	UFUNCTION()
	void HandleHostFinished(bool bSuccess);

	UEditableTextBox* NameInput = nullptr;
	TArray<UNoclipButton*> PrivacyButtons;
	UTextBlock* PlayersValue = nullptr;
	UNoclipButton* JoinCheck = nullptr;
	UNoclipButton* CreateButton = nullptr;
	UNoclipButton* BackLink = nullptr;
	UTextBlock* SummaryAccess = nullptr;
	UTextBlock* SummarySlots = nullptr;
	UTextBlock* SummaryJoin = nullptr;

	FNoclipHostSettings Settings;
	bool bWaiting = false;
};

/** Entrar em expediente (Join.dc.html): codigo de convite, busca e lista com estados. */
UCLASS()
class NOCLIP_API UNoclipJoinMenu : public UNoclipActivatableWidget
{
	GENERATED_BODY()

protected:
	virtual void BuildTree() override;
	virtual void NativeOnActivated() override;
	virtual void NativeOnDeactivated() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
	enum class EView : uint8 { Loaded, Loading, Empty, Error };

	void Search();
	void SetView(EView View);
	void RebuildRows();
	void HandleJoinIndex(int32 Index);
	void HandleJoinCode();

	UFUNCTION()
	void HandleSessionsFound(const TArray<FNoclipSessionInfo>& Sessions);

	UFUNCTION()
	void HandleJoinFinished(bool bSuccess);

	UFUNCTION()
	void HandleFilterChanged(const FText& Text);

	UFUNCTION()
	void HandleCodeCommitted(const FText& Text, ETextCommit::Type CommitMethod);

	UEditableTextBox* CodeInput = nullptr;
	UEditableTextBox* SearchInput = nullptr;
	UNoclipButton* HideFullCheck = nullptr;
	UNoclipButton* RefreshButton = nullptr;
	UNoclipButton* BackLink = nullptr;
	UWidget* ListPanel = nullptr;
	UVerticalBox* Rows = nullptr;
	UWidget* LoadingPanel = nullptr;
	UWidget* EmptyPanel = nullptr;
	UWidget* ErrorPanel = nullptr;
	UTextBlock* ErrorTitle = nullptr;
	UTextBlock* ErrorBody = nullptr;
	TArray<UWidget*> LoadingDots;
	UNoclipButton* EmptyHost = nullptr;
	UNoclipButton* ErrorRetry = nullptr;

	TArray<FNoclipSessionInfo> Found;
	EView CurrentView = EView::Loading;
	bool bSearching = false;
	float SearchTime = 0.f;
};

/** Opcoes (Options.dc.html): abas Video / Audio / Controles / Acessibilidade. */
UCLASS()
class NOCLIP_API UNoclipOptionsMenu : public UNoclipActivatableWidget
{
	GENERATED_BODY()

protected:
	virtual void BuildTree() override;
	virtual void NativeOnActivated() override;
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;

private:
	void SelectTab(int32 Index, bool bFocusTab);
	void BuildSection();
	void Changed();

	UWidget* AddRow(const FText& Title, const FText& Desc, UWidget* Control);
	UNoclipButton* MakeSelect(TArray<FText> Options, TFunction<int32()> Get, TFunction<void(int32)> Set);
	UNoclipButton* MakeCheck(TFunction<bool()> Get, TFunction<void(bool)> Set);
	UNoclipSlider* MakeSlider(float Min, float Max, float Step, float Value, TFunction<void(float)> Set);

	TArray<UNoclipButton*> Tabs;
	UScrollBox* Section = nullptr;
	UNoclipButton* BackLink = nullptr;
	UTextBlock* KeyPrev = nullptr;
	UTextBlock* KeyNext = nullptr;
	int32 TabIndex = 0;
	TArray<UNoclipButton*> Selects;
	TMap<UNoclipButton*, TFunction<void(int32)>> SelectStep;
	/** Lista de fones/microfones mudou com a aba de audio aberta: refaz a secao. */
	FDelegateHandle AudioDevicesHandle;
	void AddAudioDeviceRows();
};
