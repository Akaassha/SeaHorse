// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Frontend/Widgets/Options/ListEntries/WidgetListEntryBase.h"
#include "WidgetListEntryKeyRemap.generated.h"

class UFrontendCommonButtonBase;
class UListDataObjectKeyRemap;
/**
 * 
 */
UCLASS(Abstract, BlueprintType, meta = (DisableNaiveTick))
class SEAHORSE_API UWidgetListEntryKeyRemap : public UWidgetListEntryBase
{
	GENERATED_BODY()

protected:
	//~Begin UUserWidget Interface
	virtual void NativeOnInitialized() override;
	//~End UUserWidget Interface
	// 
	//Begin UWidgetListEntryBase Interface
	virtual void OnOwningListDataObjectSet(UListDataObjectBase* InOwninigListDataObject) override;
	virtual void OnOwningListDataObjectModified(UListDataObjectBase* OwningModifiedData, EOptionsListDataModifyReason ModifyReason) override;
	//End UWidgetListEntryBase Interface

private:
	void OnRemapKeyButtonClicked();
	void OnResetKeyBindingButtonClicked();

	void OnKeyToRemapPressed(const FKey& PressedKey);
	void OnKeyRemapCanceled(const FString& CanceledReason);

	//***** Bound Widgets *****//
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget, AllowPrivateAccess = "true"))
	UFrontendCommonButtonBase* CommonButton_RemapKey;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget, AllowPrivateAccess = "true"))
	UFrontendCommonButtonBase* CommonButton_ResetKeyBinding;
	//***** Bound Widgets *****//

	UPROPERTY(Transient)
	UListDataObjectKeyRemap* CachedOwningKeyRemapDataObject;
};
