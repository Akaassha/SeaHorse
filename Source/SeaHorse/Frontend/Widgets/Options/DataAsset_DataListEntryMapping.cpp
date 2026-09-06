// Fill out your copyright notice in the Description page of Project Settings.


#include "Frontend/Widgets/Options/DataAsset_DataListEntryMapping.h"
#include "Frontend/Widgets/Options/DataObjects/ListDataObjectBase.h"

TSubclassOf<UWidgetListEntryBase> UDataAsset_DataListEntryMapping::FindEntryWidgetClassByDataObject(UListDataObjectBase* InDataObject) const
{
	check(InDataObject);

	for (UClass* DataObjectClass = InDataObject->GetClass(); DataObjectClass; DataObjectClass->GetSuperClass())
	{
		if (TSubclassOf<UListDataObjectBase> ConvertedDataObjectClass = TSubclassOf<UListDataObjectBase>(DataObjectClass))
		{
			if (DataObjectListEntryMap.Contains(ConvertedDataObjectClass))
			{
				return DataObjectListEntryMap.FindRef(ConvertedDataObjectClass);
			}
		}
	}

	return TSubclassOf<UWidgetListEntryBase>();
}
