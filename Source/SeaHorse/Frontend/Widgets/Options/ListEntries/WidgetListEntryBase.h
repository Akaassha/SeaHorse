// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "CommonUserWidget.h"
#include "Blueprint/IUserObjectListEntry.h"
#include "Frontend/FrontendEnumTypes.h"
#include "WidgetListEntryBase.generated.h"

class UCommonTextBlock;
class UListDataObjectBase;
/**
 * 
 */
UCLASS(Abstract, BlueprintType, meta = (DisableNaiveTick))
class SEAHORSE_API UWidgetListEntryBase : public UCommonUserWidget, public IUserObjectListEntry
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintImplementableEvent, meta = (DisplayName = "On List Entry Hovered"))
	void BP_OnListEntryHovered(bool bWasHovered, bool bIsEntryWidgetStillSelected);
	void NativeOnListEntryHovered(bool bWasHovered);
	
protected:
	UFUNCTION(BlueprintImplementableEvent, meta = (DisplayName = "Get Widget To Focus Gamepad"))
	UWidget* BP_GetWidgetToFocusForGamepad() const;

	UFUNCTION(BlueprintImplementableEvent, meta = (DisplayName = "On Toggle Entry Widget Highlight State"))
	void BP_OnToggleEntryWidgetHighlightState(bool bShouldHighlight) const;

	//~Begin IUserObjectListEntry Interface
	virtual void NativeOnListItemObjectSet(UObject* ListItemObject) override;
	virtual void NativeOnItemSelectionChanged(bool bIsSelected) override;
	virtual void NativeOnEntryReleased() override;
	//~End IUserObjectListEntry Interface

	//~Begin UUserWidget Interface
	virtual FReply NativeOnFocusReceived(const FGeometry& InGeometry, const FFocusEvent& InFocusEvent) override;
	//~Begin UUserWidget Interface

	virtual void OnOwningListDataObjectSet(UListDataObjectBase* InOwninigListDataObject);

	virtual void OnOwningListDataObjectModified(UListDataObjectBase* OwningModifiedData, EOptionsListDataModifyReason ModifyReason);

	virtual void OnOwningDependecyDataObjectModified(UListDataObjectBase* OwningModifiedDependencyData, EOptionsListDataModifyReason ModifyReason);

	virtual void OnToggleEditableState(bool bIsEditable);

	void SelectThisEntryWidget();

private:
	//***** Bound Widgets *****
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	UCommonTextBlock* CommonText_SettingDisplayName;
	//***** Bound Widgets *****

	UPROPERTY(Transient)
	UListDataObjectBase* CachedOwningDataObject;
};
