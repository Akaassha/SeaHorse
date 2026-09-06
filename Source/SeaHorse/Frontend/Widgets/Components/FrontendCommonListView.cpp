// Fill out your copyright notice in the Description page of Project Settings.


#include "Frontend/Widgets/Components/FrontendCommonListView.h"
#include "Frontend/Widgets/Options/DataAsset_DataListEntryMapping.h"
#include "Frontend/Widgets/Options/ListEntries/WidgetListEntryBase.h"
#include "Frontend/Widgets/Options/DataObjects/ListDataObjectBase.h"
#include "Frontend/Widgets/Options/DataObjects/ListDataObjectCollection.h"

#if WITH_EDITOR
#include "Editor/WidgetCompilerLog.h"
#endif


UUserWidget& UFrontendCommonListView::OnGenerateEntryWidgetInternal(UObject* Item, TSubclassOf<UUserWidget> DesiredEntryClass, const TSharedRef<STableViewBase>& OwnerTable)
{
	if (IsDesignTime() || !DataListEntryMapping)
	{
		return Super::OnGenerateEntryWidgetInternal(Item, DesiredEntryClass, OwnerTable);
	}


	if (TSubclassOf<UWidgetListEntryBase> FoundWidgetClass = DataListEntryMapping->FindEntryWidgetClassByDataObject(CastChecked<UListDataObjectBase>(Item)))
	{
		return GenerateTypedEntry<UWidgetListEntryBase>(FoundWidgetClass, OwnerTable);
	}
	else
	{
		return Super::OnGenerateEntryWidgetInternal(Item, DesiredEntryClass, OwnerTable);
	}
	
}

bool UFrontendCommonListView::OnIsSelectableOrNavigableInternal(UObject* FirstSelectedItem)
{
	return !FirstSelectedItem->IsA<UListDataObjectCollection>();
}

#if WITH_EDITOR
void UFrontendCommonListView::ValidateCompiledDefaults(IWidgetCompilerLog& CompileLog) const
{
	Super::ValidateCompiledDefaults(CompileLog);

	if (!DataListEntryMapping)
	{
		CompileLog.Error(FText::FromString(TEXT("The variable DataListEntryMapping has no valid data asset assigned ") + GetClass()->GetName() + TEXT(" needs a valid data asset to function properly")));
	}
}
#endif
