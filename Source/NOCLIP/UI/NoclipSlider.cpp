// Copyright Fuly Games. All Rights Reserved.

#include "UI/NoclipSlider.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Slider.h"
#include "Components/Border.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/SizeBox.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "Brushes/SlateNoResource.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(NoclipSlider)

using namespace NoclipUI;

void UNoclipSlider::InitializeNativeClassData()
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

	// Trilho: cinza com a parte preenchida em ambar (accent-color).
	Track = MakeBar(WidgetTree, 4.f, Hex(TEXT("4A4232")), Amber());
	UOverlaySlot* TrackSlot = Root->AddChildToOverlay(Track.Root);
	TrackSlot->SetHorizontalAlignment(HAlign_Fill);
	TrackSlot->SetVerticalAlignment(VAlign_Center);
	TrackSlot->SetPadding(FMargin(8.f, 0.f));

	Slider = WidgetTree->ConstructWidget<USlider>();
	FSliderStyle SliderStyle = Slider->GetWidgetStyle();
	SliderStyle.SetNormalBarImage(FSlateNoResource());
	SliderStyle.SetHoveredBarImage(FSlateNoResource());
	SliderStyle.SetDisabledBarImage(FSlateNoResource());
	const FSlateRoundedBoxBrush Thumb(Amber(), 8.f, FVector2f(16.f, 16.f));
	const FSlateRoundedBoxBrush ThumbHot(AmberHover(), 8.f, TextBright(), 1.f, FVector2f(16.f, 16.f));
	SliderStyle.SetNormalThumbImage(Thumb);
	SliderStyle.SetHoveredThumbImage(ThumbHot);
	SliderStyle.SetDisabledThumbImage(Thumb);
	SliderStyle.SetBarThickness(4.f);
	Slider->SetWidgetStyle(SliderStyle);
	Slider->RequiresControllerLock = false;
	Slider->IsFocusable = true;
	Slider->OnValueChanged.AddDynamic(this, &UNoclipSlider::HandleValueChanged);
	UOverlaySlot* SliderSlot = Root->AddChildToOverlay(Slider);
	SliderSlot->SetHorizontalAlignment(HAlign_Fill);
	SliderSlot->SetVerticalAlignment(VAlign_Center);

	FocusRing = Box(WidgetTree, FLinearColor::Transparent, FMargin(0.f), Amber(), 2.f);
	FocusRing->SetVisibility(ESlateVisibility::Collapsed);
	UOverlaySlot* RingSlot = Root->AddChildToOverlay(FocusRing);
	RingSlot->SetHorizontalAlignment(HAlign_Fill);
	RingSlot->SetVerticalAlignment(VAlign_Fill);
	RingSlot->SetPadding(FMargin(-3.f));

	USizeBox* Size = WidgetTree->ConstructWidget<USizeBox>();
	Size->SetHeightOverride(28.f);
	Size->AddChild(Root);
	WidgetTree->RootWidget = Size;
}

void UNoclipSlider::Setup(float InMin, float InMax, float InStep, float InValue, TFunction<void(float)> InOnChanged)
{
	Min = InMin;
	Max = FMath::Max(InMax, InMin + KINDA_SMALL_NUMBER);
	Step = InStep;
	OnChanged = MoveTemp(InOnChanged);
	Slider->SetStepSize(Step / (Max - Min));
	SetValue(InValue);
}

void UNoclipSlider::SetValue(float InValue)
{
	Value = FMath::Clamp(InValue, Min, Max);
	Slider->SetValue((Value - Min) / (Max - Min));
	RefreshFill();
}

void UNoclipSlider::HandleValueChanged(float Normalized)
{
	float NewValue = Min + Normalized * (Max - Min);
	if (Step > 0.f)
	{
		NewValue = Min + FMath::RoundToFloat((NewValue - Min) / Step) * Step;
	}
	NewValue = FMath::Clamp(NewValue, Min, Max);
	if (!FMath::IsNearlyEqual(NewValue, Value))
	{
		Value = NewValue;
		if (OnChanged)
		{
			OnChanged(Value);
		}
	}
	RefreshFill();
}

void UNoclipSlider::RefreshFill()
{
	Track.SetPercent((Value - Min) / (Max - Min));
}

void UNoclipSlider::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	const bool bFocus = Slider && Slider->HasKeyboardFocus();
	const ESlateVisibility Wanted = bFocus ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed;
	if (FocusRing && FocusRing->GetVisibility() != Wanted)
	{
		FocusRing->SetVisibility(Wanted);
	}
}
