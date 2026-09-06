// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Frontend/Widgets/Options/DataObjects/ListDataObjectValue.h"
#include "CommonNumericTextBlock.h"
#include "ListDataObjectScalar.generated.h"

/**
 * 
 */
UCLASS()
class SEAHORSE_API UListDataObjectScalar : public UListDataObjectValue
{
	GENERATED_BODY()

public:
	LIST_DATA_ACCESSOR(TRange<float>, DisplayValueRange)
	LIST_DATA_ACCESSOR(TRange<float>, OutputValueRange)
	LIST_DATA_ACCESSOR(float, SliderStepSzie)
	LIST_DATA_ACCESSOR(ECommonNumericType, DisplayNumericType)
	LIST_DATA_ACCESSOR(FCommonNumberFormattingOptions, NumberFormattingOptions)

	static FCommonNumberFormattingOptions NoDecimal();
	static FCommonNumberFormattingOptions WithDecimal(int32 NumFracDigit);

	float GetCurrentValue() const;
	void SetCurrentValueFromSlider(float InNewValue);

	//~Begin UListDataObjectBase Interface
	virtual bool TryResetBackToDefaultValue() override;
	//~End UListDataObjectBase Interface

private:
	//~Begin UListDataObjectBase Interface
	virtual bool CanResetBackToDefaultValue() const override;
	virtual void OnEditDependencyDataModified(UListDataObjectBase* ModifiedDependencyData, EOptionsListDataModifyReason ModifyReason) override;
	//~End UListDataObjectBase Interface

	float StringToFloat(const FString& InString) const;

	TRange<float> DisplayValueRange = TRange<float>(0.f, 1.f);
	TRange<float> OutputValueRange = TRange<float>(0.f, 1.f);
	float SliderStepSzie = 0.1f;
	ECommonNumericType DisplayNumericType = ECommonNumericType::Number;
	FCommonNumberFormattingOptions NumberFormattingOptions;
};
