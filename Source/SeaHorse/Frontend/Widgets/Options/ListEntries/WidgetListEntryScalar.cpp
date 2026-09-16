// Fill out your copyright notice in the Description page of Project Settings.


#include "Frontend/Widgets/Options/ListEntries/WidgetListEntryScalar.h"
#include "Frontend/Widgets/Options/DataObjects/ListDataObjectScalar.h"
#include "AnalogSlider.h"

void UWidgetListEntryScalar::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	AnalogSliderSettingSlider->OnValueChanged.AddUniqueDynamic(this, &ThisClass::OnSliderValueChanged);
	AnalogSliderSettingSlider->OnMouseCaptureBegin.AddUniqueDynamic(this, &ThisClass::OnSliderMouseCaptureBegin);
}

void UWidgetListEntryScalar::OnOwningListDataObjectSet(UListDataObjectBase* InOwninigListDataObject)
{
	TGuardValue<bool> UpdatingGuard(bUpdatingSliderFromData, true);
	Super::OnOwningListDataObjectSet(InOwninigListDataObject);

	CachedOwningScalarDataObject = CastChecked<UListDataObjectScalar>(InOwninigListDataObject);

	CommonNumericSettingValue->SetNumericType(CachedOwningScalarDataObject->GetDisplayNumericType());
	CommonNumericSettingValue->FormattingSpecification = CachedOwningScalarDataObject->GetNumberFormattingOptions();
	CommonNumericSettingValue->SetCurrentValue(CachedOwningScalarDataObject->GetCurrentValue());

	AnalogSliderSettingSlider->SetMinValue(CachedOwningScalarDataObject->GetDisplayValueRange().GetLowerBoundValue());
	AnalogSliderSettingSlider->SetMaxValue(CachedOwningScalarDataObject->GetDisplayValueRange().GetUpperBoundValue());
	AnalogSliderSettingSlider->SetStepSize(CachedOwningScalarDataObject->GetSliderStepSzie());
	AnalogSliderSettingSlider->SetValue(CachedOwningScalarDataObject->GetCurrentValue());
}

void UWidgetListEntryScalar::OnOwningListDataObjectModified(UListDataObjectBase* ModifiedData, EOptionsListDataModifyReason ModifyReason)
{
	TGuardValue<bool> UpdatingGuard(bUpdatingSliderFromData, true);
	if (CachedOwningScalarDataObject)
	{
		CommonNumericSettingValue->SetCurrentValue(CachedOwningScalarDataObject->GetCurrentValue());
		AnalogSliderSettingSlider->SetValue(CachedOwningScalarDataObject->GetCurrentValue());
	}
}

void UWidgetListEntryScalar::OnSliderValueChanged(float Value)
{
	if (!bUpdatingSliderFromData && CachedOwningScalarDataObject)
	{
		CachedOwningScalarDataObject->SetCurrentValueFromSlider(Value);
	}
}

void UWidgetListEntryScalar::OnOwningListDataObjectReleased()
{
	CachedOwningScalarDataObject = nullptr;
	Super::OnOwningListDataObjectReleased();
}

void UWidgetListEntryScalar::OnSliderMouseCaptureBegin()
{
	SelectThisEntryWidget();


}
