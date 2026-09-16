// Fill out your copyright notice in the Description page of Project Settings.


#include "Frontend/Widgets/Options/DataObjects/ListDataObjectBase.h"
#include "GameSettings/SHGameUserSettings.h"

void UListDataObjectBase::InitDataObject()
{
	OnDataObjectInitialized();
}

void UListDataObjectBase::AddEditCondition(const FOptionsDataEditConditionDescriptor& InEditCondition)
{
	EditConditionTestArray.Add(InEditCondition);
}

void UListDataObjectBase::AddEditDependencyData(UListDataObjectBase* InDependencyData)
{
	if (!IsValid(InDependencyData) || InDependencyData == this)
	{
		return;
	}
	if (!InDependencyData->OnListDataModified.IsBoundToObject(this))
	{
		InDependencyData->OnListDataModified.AddUObject(this, &ThisClass::OnEditDependencyDataModified);
	}
}

bool UListDataObjectBase::IsDataCurrentlyEditable()
{
	bool bIsEditable = true;
	SetDisabledRichText(FText::GetEmpty());

	if (EditConditionTestArray.IsEmpty())
	{
		return bIsEditable;
	}

	FString CachedDisabledRichReason;

	for (const FOptionsDataEditConditionDescriptor& Condition : EditConditionTestArray)
	{
		if (!Condition.IsValid() || Condition.IsEditConditionMet())
		{
			continue;
		}

		bIsEditable = false;

		CachedDisabledRichReason.Append(Condition.GetDisabledRichReason());
		
		SetDisabledRichText(FText::FromString(CachedDisabledRichReason));

		if (Condition.HasForcedStringValue())
		{
			const FString ForcedStringValue = Condition.GetDisabledForcedStringValue();

			if (CanSetToForcedStringValue(ForcedStringValue))
			{
				OnSetToForcedStringValue(ForcedStringValue);
			}
		}
	}

	return bIsEditable;
}

void UListDataObjectBase::OnDataObjectInitialized()
{

}

void UListDataObjectBase::NotifyListDataModified(UListDataObjectBase* ModifiedData, EOptionsListDataModifyReason ModifyReason)
{
	OnListDataModified.Broadcast(ModifiedData, ModifyReason);

	if (bSholdApplyChangeImmediately)
	{
		USHGameUserSettings::Get()->ApplySettings(true);
	}
}

void UListDataObjectBase::OnEditDependencyDataModified(UListDataObjectBase* ModifiedDependencyData, EOptionsListDataModifyReason ModifyReason)
{
	OnDependencyDataModified.Broadcast(ModifiedDependencyData, ModifyReason);
}
