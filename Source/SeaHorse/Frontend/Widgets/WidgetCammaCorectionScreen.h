// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Frontend/Widgets/WidgetActivatableBase.h"
#include "WidgetCammaCorectionScreen.generated.h"

class UCommonNumericTextBlock;
class UAnalogSlider;
class UListDataObjectScalar;


DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnGammaConfirmed);
/**
 * 
 */
UCLASS()
class SEAHORSE_API UWidgetCammaCorectionScreen : public UWidgetActivatableBase
{



	GENERATED_BODY()

public: 
	UPROPERTY(BlueprintAssignable)
	FOnGammaConfirmed FOnGammaConfirmed;

protected:
	//~Begin UUserWidget Interface
	virtual void NativeOnInitialized() override;
	//~End UUserWidget Interface

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

	void OnConfirmBoundActionTriggered();

	void OnResetBoundActionTriggered();

	UPROPERTY(EditDefaultsOnly, Category = "Frontend Options Screen", meta = (RowType = "/Script/CommonUI.CommonInputActionDataBase"))
	FDataTableRowHandle ConfirmAction;

	FUIActionBindingHandle ConfirmActionHandle;

	UPROPERTY(EditDefaultsOnly, Category = "Frontend Options Screen", meta = (RowType = "/Script/CommonUI.CommonInputActionDataBase"))
	FDataTableRowHandle ResetAction;

	FUIActionBindingHandle ResetActionHandle;

	UListDataObjectScalar* DisplayGamma;
};
