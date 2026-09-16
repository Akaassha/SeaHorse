// Fill out your copyright notice in the Description page of Project Settings.


#include "Frontend/Widgets/Options/ListEntries/WidgetListEntryBase.h"
#include "Frontend/Widgets/Options/DataObjects/ListDataObjectBase.h"
#include "Components/ListView.h"
#include "CommonInputSubsystem.h"
#include "CommonTextBlock.h"

void UWidgetListEntryBase::NativeOnListEntryHovered(bool bWasHovered)
{
	BP_OnListEntryHovered(bWasHovered, GetListItem() ?  IsListItemSelected() : false);

	if (bWasHovered)
	{
		BP_OnToggleEntryWidgetHighlightState(true);

	}
	else
	{
		BP_OnToggleEntryWidgetHighlightState(GetListItem() && IsListItemSelected() ? true : false);
	}
}

void UWidgetListEntryBase::NativeOnListItemObjectSet(UObject* ListItemObject)
{
	IUserObjectListEntry::NativeOnListItemObjectSet(ListItemObject);

	OnOwningListDataObjectSet(CastChecked<UListDataObjectBase>(ListItemObject));

}

void UWidgetListEntryBase::NativeOnItemSelectionChanged(bool bIsSelected)
{
	IUserObjectListEntry::NativeOnItemSelectionChanged(bIsSelected);

	BP_OnToggleEntryWidgetHighlightState(bIsSelected);
}

void UWidgetListEntryBase::NativeOnEntryReleased()
{
	OnOwningListDataObjectReleased();
	IUserObjectListEntry::NativeOnEntryReleased();

	NativeOnListEntryHovered(false);
}

void UWidgetListEntryBase::OnOwningListDataObjectReleased()
{
	if (IsValid(CachedOwningDataObject))
	{
		CachedOwningDataObject->OnListDataModified.RemoveAll(this);
		CachedOwningDataObject->OnDependencyDataModified.RemoveAll(this);
	}
	CachedOwningDataObject = nullptr;
}

FReply UWidgetListEntryBase::NativeOnFocusReceived(const FGeometry& InGeometry, const FFocusEvent& InFocusEvent)
{
	UCommonInputSubsystem* CommonInputSubsystem = GetInputSubsystem();

	if (CommonInputSubsystem && CommonInputSubsystem->GetCurrentInputType() == ECommonInputType::Gamepad)
	{
		if (UWidget* WidgetToFocus = BP_GetWidgetToFocusForGamepad())
		{
			if (TSharedPtr<SWidget> SlateWidgetToFocus = WidgetToFocus->GetCachedWidget())
			{
				return FReply::Handled().SetUserFocus(SlateWidgetToFocus.ToSharedRef());
			}
		}
	}

	return Super::NativeOnFocusReceived(InGeometry, InFocusEvent);
}

void UWidgetListEntryBase::OnOwningListDataObjectSet(UListDataObjectBase* InOwninigListDataObject)
{
	OnOwningListDataObjectReleased();
	CachedOwningDataObject = InOwninigListDataObject;
	if (CommonText_SettingDisplayName)
	{
		CommonText_SettingDisplayName->SetText(InOwninigListDataObject->GetDataDisplayName());
	}

	if (!InOwninigListDataObject->OnListDataModified.IsBoundToObject(this))
	{
		InOwninigListDataObject->OnListDataModified.AddUObject(this, &ThisClass::OnOwningListDataObjectModified);
	}

	if (!InOwninigListDataObject->OnDependencyDataModified.IsBoundToObject(this))
	{
		InOwninigListDataObject->OnDependencyDataModified.AddUObject(this, &ThisClass::OnOwningDependecyDataObjectModified);
	}

	OnToggleEditableState(InOwninigListDataObject->IsDataCurrentlyEditable());

}

void UWidgetListEntryBase::OnOwningListDataObjectModified(UListDataObjectBase* ModifiedData, EOptionsListDataModifyReason ModifyReason)
{

}

void UWidgetListEntryBase::OnOwningDependecyDataObjectModified(UListDataObjectBase* OwningModifiedDependencyData, EOptionsListDataModifyReason ModifyReason)
{
	if (CachedOwningDataObject)
	{
		OnToggleEditableState(CachedOwningDataObject->IsDataCurrentlyEditable());
	}
	
}

void UWidgetListEntryBase::OnToggleEditableState(bool bIsEditable)
{
	if (CommonText_SettingDisplayName)
	{
		CommonText_SettingDisplayName->SetIsEnabled(bIsEditable);
	}
}

void UWidgetListEntryBase::SelectThisEntryWidget()
{
	if (UListView* List = Cast<UListView>(GetOwningListView()); List && GetListItem())
	{
		List->SetSelectedItem(GetListItem());
	}
}
