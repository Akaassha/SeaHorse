// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Frontend/Widgets/Options/ListEntries/WidgetListEntryBase.h"
#include "WidgetListEntryScalar.generated.h"

class UCommonNumericTextBlock;
class UAnalogSlider;
class UListDataObjectScalar;
/**
 * 
 */
UCLASS(Abstract, BlueprintType, meta = (DisableNaiveTick))
class SEAHORSE_API UWidgetListEntryScalar : public UWidgetListEntryBase
{
	GENERATED_BODY()
	
protected:
	//~Begin UUserWidget Interface
	virtual void NativeOnInitialized() override;
	//~End UUserWidget Interface

		//~Begin UWidgetListEntryBase Interface
	virtual void OnOwningListDataObjectSet(UListDataObjectBase* InOwninigListDataObject);
	virtual void OnOwningListDataObjectModified(UListDataObjectBase* ModifiedData, EOptionsListDataModifyReason ModifyReason) override;
	//~End UWidgetListEntryBase Interface

private:

	UFUNCTION()
	void OnSliderValueChanged(float Value);

	UFUNCTION()
	void OnSliderMouseCaptureBegin();

	//***** Bound Widgets *****//
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget, AllowPrivateAccess = "true"))
	UCommonNumericTextBlock* CommonNumericSettingValue;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget, AllowPrivateAccess = "true"))
	UAnalogSlider* AnalogSliderSettingSlider;
	//***** Bound Widgets *****//

	UPROPERTY(Transient)
	UListDataObjectScalar* CachedOwningScalarDataObject;
};
