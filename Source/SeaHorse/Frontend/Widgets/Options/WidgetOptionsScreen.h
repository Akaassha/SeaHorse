// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Frontend/Widgets/WidgetActivatableBase.h"
#include "Frontend/FrontendEnumTypes.h"
#include "WidgetOptionsScreen.generated.h"

class UOptionsDataRegistry;
class UFrontendTabListWidgetBase;
class UFrontendCommonListView;
class UWidgetOptionsDetailView;
class UListDataObjectBase;
/**
 * 
 */
UCLASS(Abstract, BlueprintType, meta = (DisableNaiveTick))
class SEAHORSE_API UWidgetOptionsScreen : public UWidgetActivatableBase
{
	GENERATED_BODY()
	
protected:
	//~Begin UUserWidget Interface
	virtual void NativeOnInitialized() override;
	//~End UUserWidget Interface

	//~Begin UWidgetActivatableBase Interface
	virtual void NativeOnActivated() override;
	virtual void NativeOnDeactivated() override;
	virtual UWidget* NativeGetDesiredFocusTarget() const override;
	//~End UWidgetActivatableBase Interface

	//***** Bound Widgets *****//
	UPROPERTY(meta = (BindWidget))
	UFrontendTabListWidgetBase* TabListWidget_OptionsTabs;

	UPROPERTY(meta = (BindWidget))
	UFrontendCommonListView* CommonListView_OptionsList;

	UPROPERTY(meta = (BindWidget))
	UWidgetOptionsDetailView* DetailsView_ListEntryInfo;
	//***** Bound Widgets *****//

private:
	UOptionsDataRegistry* GetOrCreateDataRegistry();
	
	void OnResetBoundActionTriggered();
	void OnBackBoundActionTriggered();

	UFUNCTION()
	void OnOptionsTabSelected(FName TabId);

	void OnListViewItemHovered(UObject* InHoveredItem, bool bWasHovered);
	void OnListViewItemSelected(UObject* InSelectedItem);

	FString TryGetEntryWidgetClassName(UObject* InOwningListItem) const;

	void OnListviewListDataModified(UListDataObjectBase* ModifiedData, EOptionsListDataModifyReason ModifyReason);

	UPROPERTY(Transient)
	UOptionsDataRegistry* CreatedOwningDataRegistry;

	UPROPERTY(EditDefaultsOnly, Category = "Frontend Options Screen", meta = (RowType = "/Script/CommonUI.CommonInputActionDataBase"))
	FDataTableRowHandle ResetAction;

	FUIActionBindingHandle ResetActionHandle;

	UPROPERTY(Transient)
	TArray<UListDataObjectBase*> ResetableDataArray;

	bool bIsResetingData = false;
};
