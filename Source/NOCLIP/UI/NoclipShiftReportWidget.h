// Copyright Fuly Games. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UI/NoclipActivatableWidget.h"
#include "Game/NoclipShiftTypes.h"
#include "NoclipShiftReportWidget.generated.h"

class UNoclipButton;
class UTextBlock;
class UBorder;
class UVerticalBox;
class UWidget;

/**
 * Relatorio de expediente com a quota batida (Report.dc.html): ficha manila com as contas,
 * carimbo APROVADO, mencoes honrosas da equipe e contagem para voltar ao saguao.
 * O ganho conta de 0 ate o valor (game feel). So o lider pode adiantar a volta ao saguao.
 */
UCLASS()
class NOCLIP_API UNoclipShiftReportWidget : public UNoclipActivatableWidget
{
	GENERATED_BODY()

public:
	UNoclipShiftReportWidget(const FObjectInitializer& ObjectInitializer);

protected:
	virtual void BuildTree() override;
	virtual void NativeOnActivated() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
	void ApplyResult(const FNoclipShiftResult& Result);
	void RefreshAwards();
	UTextBlock* AddLine(UVerticalBox* Col, const FText& Label, const FSlateFontInfo& ValueFont, bool bStrong = false);

	UTextBlock* Eyebrow = nullptr;
	UTextBlock* Extracted = nullptr;
	UTextBlock* QuotaValue = nullptr;
	UTextBlock* Surplus = nullptr;
	UTextBlock* Bonus = nullptr;
	UTextBlock* Earned = nullptr;
	UTextBlock* Items = nullptr;
	UTextBlock* InRoom = nullptr;
	UTextBlock* ExitText = nullptr;
	UTextBlock* LeftBehindNote = nullptr;
	TArray<UTextBlock*> AwardNames;
	TArray<UTextBlock*> AwardDetails;
	UTextBlock* Countdown = nullptr;
	UWidget* ProgressRoot = nullptr;
	class UBorder* ProgressFill = nullptr;
	class UHorizontalBoxSlot* ProgressFillSlot = nullptr;
	class UHorizontalBoxSlot* ProgressRestSlot = nullptr;
	UNoclipButton* LobbyButton = nullptr;

	FNoclipShiftResult Shown;
	float CountUpTime = 0.f;
	float RefreshAccumulator = 0.f;
	float TotalSeconds = 12.f;
};

/** Carta de demissao (Fired.dc.html): a quota falhou, a corrida recomeca do turno 1. */
UCLASS()
class NOCLIP_API UNoclipFiredScreen : public UNoclipActivatableWidget
{
	GENERATED_BODY()

public:
	UNoclipFiredScreen(const FObjectInitializer& ObjectInitializer);

protected:
	virtual void BuildTree() override;
	virtual void NativeOnActivated() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
	void ApplyResult(const FNoclipShiftResult& Result);

	UTextBlock* ShiftLabel = nullptr;
	UTextBlock* Extracted = nullptr;
	UTextBlock* QuotaValue = nullptr;
	UTextBlock* Missing = nullptr;
	UTextBlock* RunInfo = nullptr;
	UNoclipButton* NewRunButton = nullptr;
	FNoclipShiftResult Shown;
	float RefreshAccumulator = 0.f;
};
