// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Frontend/Widgets/Options/ListEntries/WidgetListEntryBase.h"
#include "WidgetListEntryString.generated.h"

class UFrontendCommonButtonBase;
class UFrontendCommonRotator;
class UListDataObjectString;
/**
 * 
 */
UCLASS(Abstract, BlueprintType, meta = (DisableNaiveTick))
class SEAHORSE_API UWidgetListEntryString : public UWidgetListEntryBase
{
	GENERATED_BODY()

protected:
	//~Begin UUserWidget Interface
	virtual void NativeOnInitialized() override;
	//~End UUserWidget Interface

	//~Begin UWidgetListEntryBase Interface
	virtual void OnOwningListDataObjectSet(UListDataObjectBase* InOwninigListDataObject);
	virtual void OnOwningListDataObjectModified(UListDataObjectBase* ModifiedData, EOptionsListDataModifyReason ModifyReason) override;
	virtual void OnToggleEditableState(bool bIsEditable) override;
	//~End UWidgetListEntryBase Interface
		
private:
	void OnPreviousOptionButtonCliced();
	void OnNextOptionButtonCliced();

	void OnRotatorValueChanged(int32 Value, bool bUserInitiated);

	//***** Bound Widgets *****//
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget, AllowPrivateAccess = "true"))
	UFrontendCommonButtonBase* CommonButton_PreviousOption;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget, AllowPrivateAccess = "true"))
	UFrontendCommonRotator* CommonRotator_AvailableOptions;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget, AllowPrivateAccess = "true"))
	UFrontendCommonButtonBase* CommonButton_NextOption;
	//***** Bound Widgets *****//

	UPROPERTY(Transient)
	UListDataObjectString* CachedOwninigDataObject;
};
