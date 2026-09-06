// Fill out your copyright notice in the Description page of Project Settings.


#include "Frontend/Widgets/Options/WidgetOptionsScreen.h"
#include "ICommonInputModule.h"
#include "Frontend/Widgets/Options/OptionsDataRegistry.h"
#include "Frontend/Widgets/Components/FrontendTabListWidgetBase.h"
#include "Frontend/Widgets/Options/DataObjects/ListDataObjectCollection.h"
#include "Frontend/Debug/FrontendDebugHelper.h"
#include "Frontend/Widgets/Components/FrontendCommonListView.h"
#include "GameSettings/SHGameUserSettings.h"
#include "Frontend/Widgets/Options/ListEntries/WidgetListEntryBase.h"
#include "Frontend/Widgets/Options/WidgetOptionsDetailView.h"
#include "Frontend/FrontendSubsystem.h"
#include "Frontend/Widgets/Components/FronendCommonButtonBase.h"
#include "Input/CommonUIInputTypes.h"

void UWidgetOptionsScreen::NativeOnInitialized()
{
	Super::NativeOnInitialized();

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

	RegisterUIActionBinding(
		FBindUIActionArgs(
			ICommonInputModule::GetSettings().GetDefaultBackAction(),
			true,
			FSimpleDelegate::CreateUObject(this, &ThisClass::OnBackBoundActionTriggered)
		)
	);

	TabListWidget_OptionsTabs->OnTabSelected.AddUniqueDynamic(this, &UWidgetOptionsScreen::OnOptionsTabSelected);

	CommonListView_OptionsList->OnItemIsHoveredChanged().AddUObject(this, &ThisClass::OnListViewItemHovered);
	CommonListView_OptionsList->OnItemSelectionChanged().AddUObject(this, &ThisClass::OnListViewItemSelected);
}

void UWidgetOptionsScreen::NativeOnActivated()
{
	Super::NativeOnActivated();

	for (UListDataObjectCollection* TabCollection : GetOrCreateDataRegistry()->GetRegisteredOptionsTabCollections())
	{
		if (!TabCollection) {
			continue;
		}

		const FName TabID = TabCollection->GetDataID();
		TabListWidget_OptionsTabs->GetTabButtonBaseByID(TabID);

		if (TabListWidget_OptionsTabs->GetTabButtonBaseByID(TabID) != nullptr)
		{
			continue;
		}

		TabListWidget_OptionsTabs->RequestRegisterTab(TabID, TabCollection->GetDataDisplayName());
	}
}

void UWidgetOptionsScreen::NativeOnDeactivated()
{
	Super::NativeOnDeactivated();

	USHGameUserSettings::Get()->ApplySettings(true);
}

UWidget* UWidgetOptionsScreen::NativeGetDesiredFocusTarget() const
{
	if (UObject* SelectedObject = CommonListView_OptionsList->GetSelectedItem())
	{
		if (UUserWidget* SelectedEntryWidget = CommonListView_OptionsList->GetEntryWidgetFromItem(SelectedObject))
		{
			return SelectedEntryWidget;
		}
	}

	return Super::NativeGetDesiredFocusTarget();
}

UOptionsDataRegistry* UWidgetOptionsScreen::GetOrCreateDataRegistry()
{
	if (!CreatedOwningDataRegistry)
	{
		CreatedOwningDataRegistry = NewObject<UOptionsDataRegistry>();
		CreatedOwningDataRegistry->InitOptionsDataRegistry(GetOwningLocalPlayer());
	}

	checkf(CreatedOwningDataRegistry, TEXT("Data registry for options screen is not valid"));

	return CreatedOwningDataRegistry;
}

void UWidgetOptionsScreen::OnResetBoundActionTriggered()
{
	if (ResetableDataArray.IsEmpty())
	{
		return;
	}

	UCommonButtonBase* SelectedTabButton = TabListWidget_OptionsTabs->GetTabButtonBaseByID(TabListWidget_OptionsTabs->GetActiveTab());

	const FString SelectedTabButtonName = CastChecked<UFrontendCommonButtonBase>(SelectedTabButton)->GetButtonDisplayText().ToString();

	UFrontendSubsystem::Get(this)->PushConfirmScreenToModalStackAsync(
		EConfirmScreenType::YesNo,
		FText::FromString(TEXT("Reset")),
		FText::FromString(TEXT("Are you sure want to reset all the settings under the ") + SelectedTabButtonName + TEXT(" tab.")),
		[this](EConfirmScreenButtonType ClickedButtonType) 
		{
			if (ClickedButtonType != EConfirmScreenButtonType::Confirmed)
			{
				return;
			}

			bIsResetingData = true;
			bool bHasDataFailedToReset = false;

			for (UListDataObjectBase* DataToReset : ResetableDataArray)
			{
				if (!DataToReset)
				{
					continue;
				}

				if (DataToReset->TryResetBackToDefaultValue())
				{
					Debug::Print(DataToReset->GetDataDisplayName().ToString() + TEXT(" was reset"));
				}
				else
				{
					bHasDataFailedToReset = true;
					Debug::Print(DataToReset->GetDataDisplayName().ToString() + TEXT(" failed to reset"));
				}
			}

			if (!bHasDataFailedToReset)
			{
				ResetableDataArray.Empty();
				RemoveActionBinding(ResetActionHandle);
			}

			bIsResetingData = false;

		}
	);
}

void UWidgetOptionsScreen::OnBackBoundActionTriggered()
{
	DeactivateWidget();
}

void UWidgetOptionsScreen::OnOptionsTabSelected(FName TabId)
{
	DetailsView_ListEntryInfo->ClearDetailsViewInfo();
	
	TArray<UListDataObjectBase*> FoundListSourceItems = GetOrCreateDataRegistry()->GetListSourceItemsBySelectedTabID(TabId);
	
	CommonListView_OptionsList->SetListItems(FoundListSourceItems);
	CommonListView_OptionsList->RequestRefresh();
	
	if (CommonListView_OptionsList->GetNumItems() != 0)
	{
		CommonListView_OptionsList->NavigateToIndex(0);
		CommonListView_OptionsList->SetSelectedIndex(0);
	}
	
	ResetableDataArray.Empty();
	
	for (UListDataObjectBase* FoundListSourceItem : FoundListSourceItems)
	{
		if (!FoundListSourceItem)
		{
			continue;
		}
		
		if (!FoundListSourceItem->OnListDataModified.IsBoundToObject(this))
		{
			FoundListSourceItem->OnListDataModified.AddUObject(this, &ThisClass::OnListviewListDataModified);
		}
		
		if (FoundListSourceItem->CanResetBackToDefaultValue())
		{
			ResetableDataArray.AddUnique(FoundListSourceItem);
		}
		
		if (ResetableDataArray.IsEmpty())
		{
			RemoveActionBinding(ResetActionHandle);
		}
		else
		{
			if (!GetActionBindings().Contains(ResetActionHandle))
			{
				AddActionBinding(ResetActionHandle);
			}
		}
	}

}

void UWidgetOptionsScreen::OnListViewItemHovered(UObject* InHoveredItem, bool bWasHovered)
{
	if (!InHoveredItem) {
		return;
	}

	UWidgetListEntryBase* HoveredEntryWidget = CommonListView_OptionsList->GetEntryWidgetFromItem<UWidgetListEntryBase>(InHoveredItem);

	check(HoveredEntryWidget);

	HoveredEntryWidget->NativeOnListEntryHovered(bWasHovered);

	if (bWasHovered)
	{
		DetailsView_ListEntryInfo->UpdateDetailsViewInfo(
			CastChecked<UListDataObjectBase>(InHoveredItem),
			TryGetEntryWidgetClassName(InHoveredItem)
		);
	}
	else
	{
		if (UListDataObjectBase* SelectedItem = CommonListView_OptionsList->GetSelectedItem<UListDataObjectBase>())
		{
			DetailsView_ListEntryInfo->UpdateDetailsViewInfo(
				SelectedItem,
				TryGetEntryWidgetClassName(SelectedItem)
			);
		}
	}
}

void UWidgetOptionsScreen::OnListViewItemSelected(UObject* InSelectedItem)
{
	if (!InSelectedItem) {
		return;
	}

	DetailsView_ListEntryInfo->UpdateDetailsViewInfo(
		CastChecked<UListDataObjectBase>(InSelectedItem),
		TryGetEntryWidgetClassName(InSelectedItem)
	);
}

FString UWidgetOptionsScreen::TryGetEntryWidgetClassName(UObject* InOwningListItem) const
{
	if (UUserWidget* FoundEntryWidget = CommonListView_OptionsList->GetEntryWidgetFromItem(InOwningListItem))
	{
		return FoundEntryWidget->GetClass()->GetName();
	}

	return TEXT("Entry Widget Not Valid");
}

void UWidgetOptionsScreen::OnListviewListDataModified(UListDataObjectBase* ModifiedData, EOptionsListDataModifyReason ModifyReason)
{
	if (!ModifiedData || bIsResetingData)
	{
		return;
	}

	if (ModifiedData->CanResetBackToDefaultValue())
	{
		ResetableDataArray.AddUnique(ModifiedData);

		if (!GetActionBindings().Contains(ResetActionHandle))
		{
			AddActionBinding(ResetActionHandle);
		}
	}
	else
	{
		if (ResetableDataArray.Contains(ModifiedData))
		{
			ResetableDataArray.Remove(ModifiedData);
		}
	}

	if (ResetableDataArray.IsEmpty())
	{
		RemoveActionBinding(ResetActionHandle);
	}
}
