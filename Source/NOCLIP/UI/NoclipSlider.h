// Copyright Fuly Games. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "CommonUserWidget.h"
#include "UI/NoclipUIStyle.h"
#include "NoclipSlider.generated.h"

class USlider;
class UBorder;

/**
 * input[type=range] do prototipo (accent ambar): trilho de 4px com parte cheia e pino.
 * Mouse arrasta; setas/direcional/analogico ajustam direto (sem "travar" com A antes).
 */
UCLASS()
class NOCLIP_API UNoclipSlider : public UCommonUserWidget
{
	GENERATED_BODY()

public:
	void Setup(float InMin, float InMax, float InStep, float InValue, TFunction<void(float)> InOnChanged);
	void SetValue(float InValue);
	float GetValue() const { return Value; }
	USlider* GetSlider() const { return Slider; }

protected:
	virtual void InitializeNativeClassData() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
	UFUNCTION()
	void HandleValueChanged(float Normalized);

	void RefreshFill();

	UPROPERTY(Transient) TObjectPtr<USlider> Slider;
	UPROPERTY(Transient) TObjectPtr<UBorder> FocusRing;
	NoclipUI::FBar Track;

	float Min = 0.f;
	float Max = 1.f;
	float Step = 0.01f;
	float Value = 0.f;
	TFunction<void(float)> OnChanged;
};
