// Fill out your copyright notice in the Description page of Project Settings.


#include "Frontend/Widgets/Options/ListEntries/WidgetListEntryString.h"
#include "Frontend/Widgets/Options/DataObjects/MyListDataObjectString.h"
#include "Frontend/Widgets/Components/FrontendCommonRotator.h"
#include "CommonInputSubsystem.h"
#include "Frontend/Widgets/Components/FronendCommonButtonBase.h"

void UWidgetListEntryString::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	CommonButton_PreviousOption->OnClicked().AddUObject(this, &ThisClass::OnPreviousOptionButtonCliced);
	CommonButton_NextOption->OnClicked().AddUObject(this, &ThisClass::OnNextOptionButtonCliced);

	CommonRotator_AvailableOptions->OnClicked().AddWeakLambda(this, [this]() {
		SelectThisEntryWidget();
	});

	CommonRotator_AvailableOptions->OnRotatedEvent.AddUObject(this, &ThisClass::OnRotatorValueChanged);
}

void UWidgetListEntryString::OnOwningListDataObjectSet(UListDataObjectBase* InOwninigListDataObject)
{
	Super::OnOwningListDataObjectSet(InOwninigListDataObject);

	CachedOwninigDataObject = CastChecked<UListDataObjectString>(InOwninigListDataObject);

	CommonRotator_AvailableOptions->PopulateTextLabels(CachedOwninigDataObject->GetAvaliableOptionsTextArray());
	CommonRotator_AvailableOptions->SetSelectedOptionsByText(CachedOwninigDataObject->GetCurrentDisplayText());
}

void UWidgetListEntryString::OnOwningListDataObjectModified(UListDataObjectBase* ModifiedData, EOptionsListDataModifyReason ModifyReason)
{
	if (CachedOwninigDataObject)
	{
		CommonRotator_AvailableOptions->SetSelectedOptionsByText(CachedOwninigDataObject->GetCurrentDisplayText());
	}
}

void UWidgetListEntryString::OnOwningListDataObjectReleased()
{
	CachedOwninigDataObject = nullptr;
	Super::OnOwningListDataObjectReleased();
}

void UWidgetListEntryString::OnToggleEditableState(bool bIsEditable)
{
	Super::OnToggleEditableState(bIsEditable);

	CommonButton_PreviousOption->SetIsEnabled(bIsEditable);
	CommonRotator_AvailableOptions->SetIsEnabled(bIsEditable);
	CommonButton_NextOption->SetIsEnabled(bIsEditable);
}

void UWidgetListEntryString::OnPreviousOptionButtonCliced()
{
	if (CachedOwninigDataObject)
	{
		CachedOwninigDataObject->BackToPreviousOption();
	}

	SelectThisEntryWidget();
}

void UWidgetListEntryString::OnNextOptionButtonCliced()
{
	if (CachedOwninigDataObject)
	{
		CachedOwninigDataObject->AdvanceToNextOption();
	}

	SelectThisEntryWidget();
}

void UWidgetListEntryString::OnRotatorValueChanged(int32 Value, bool bUserInitiated)
{
	if (!CachedOwninigDataObject)
	{
		return;
	}

	UCommonInputSubsystem* CommonInputSubsystem = GetInputSubsystem();

	if (!CommonInputSubsystem || !bUserInitiated)
	{
		return;
	}

	if (CommonInputSubsystem->GetCurrentInputType() == ECommonInputType::Gamepad)
	{
		CachedOwninigDataObject->OnRotatorInitiatedValueChange(CommonRotator_AvailableOptions->GetSelectedText());

	}
}
