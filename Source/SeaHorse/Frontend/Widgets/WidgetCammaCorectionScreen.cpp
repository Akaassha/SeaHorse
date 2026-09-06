// Fill out your copyright notice in the Description page of Project Settings.


#include "Frontend/Widgets/WidgetCammaCorectionScreen.h"
#include "Frontend/Widgets/Options/DataObjects/ListDataObjectScalar.h"
#include "Frontend/Widgets/Options/OptionsDataInteractionHelper.h"
#include "GameSettings/SHGameUserSettings.h"
#include "Input/CommonUIInputTypes.h"
#include "AnalogSlider.h"

#define MAKE_OPTIONS_DATA_CONTROL(SetterOrGetterFuncName) \
	MakeShared<FOptionsDataInteractionHelper>(GET_FUNCTION_NAME_STRING_CHECKED(USHGameUserSettings, SetterOrGetterFuncName))

void UWidgetCammaCorectionScreen::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	DisplayGamma = NewObject<UListDataObjectScalar>();
	DisplayGamma->SetDataID(FName("DisplayGammaScreen"));
	DisplayGamma->SetDataDisplayName(FText::FromString(TEXT("Gamma")));
	DisplayGamma->SetDescriptionRichText(FText::FromString(TEXT("This is description for Brightness")));
	DisplayGamma->SetDisplayValueRange(TRange<float>(0.f, 1.f));
	DisplayGamma->SetOutputValueRange(TRange<float>(1.7f, 2.7f)); //The default value Unreal has is 2.2f
	DisplayGamma->SetSliderStepSzie(0.01f);
	DisplayGamma->SetDisplayNumericType(ECommonNumericType::Percentage);
	DisplayGamma->SetNumberFormattingOptions(UListDataObjectScalar::NoDecimal());
	DisplayGamma->SetDataDynamicGetter(MAKE_OPTIONS_DATA_CONTROL(GetCurrentDisplayGamma));
	DisplayGamma->SetDataDynamicSetter(MAKE_OPTIONS_DATA_CONTROL(SetCurrentDisplayGamma));
	DisplayGamma->SetDefaultValueFromString(LexToString(2.2f));

	AnalogSliderSettingSlider->OnValueChanged.AddUniqueDynamic(this, &ThisClass::OnSliderValueChanged);
	AnalogSliderSettingSlider->OnMouseCaptureBegin.AddUniqueDynamic(this, &ThisClass::OnSliderMouseCaptureBegin);
	AnalogSliderSettingSlider->SetStepSize(0.01f);

	AnalogSliderSettingSlider->SetMinValue(0.f);
	AnalogSliderSettingSlider->SetMaxValue(1.f);

	CommonNumericSettingValue->SetNumericType(ECommonNumericType::Percentage);
	AnalogSliderSettingSlider->SetValue(DisplayGamma->GetCurrentValue());
	CommonNumericSettingValue->SetCurrentValue(DisplayGamma->GetCurrentValue());

	if (!ConfirmAction.IsNull())
	{
		ConfirmActionHandle = RegisterUIActionBinding(
			FBindUIActionArgs(
				ConfirmAction,
				true,
				FSimpleDelegate::CreateUObject(this, &ThisClass::OnConfirmBoundActionTriggered)
			)
		);

	}

	if (!ResetAction.IsNull())
	{
		ResetActionHandle = RegisterUIActionBinding(
			FBindUIActionArgs(
				ResetAction,
				true,
				FSimpleDelegate::CreateUObject(this, &ThisClass::OnResetBoundActionTriggered)
			)
		);

	}

}

void UWidgetCammaCorectionScreen::OnSliderValueChanged(float Value)
{
	DisplayGamma->SetCurrentValueFromSlider(Value);
	CommonNumericSettingValue->SetCurrentValue(Value);
}

void UWidgetCammaCorectionScreen::OnSliderMouseCaptureBegin()
{
	
}

void UWidgetCammaCorectionScreen::OnConfirmBoundActionTriggered()
{
	USHGameUserSettings::Get()->SaveSettings();
	FOnGammaConfirmed.Broadcast();
}


void UWidgetCammaCorectionScreen::OnResetBoundActionTriggered()
{
	if (DisplayGamma->TryResetBackToDefaultValue())
	{
		AnalogSliderSettingSlider->SetValue(DisplayGamma->GetCurrentValue());
	}

}