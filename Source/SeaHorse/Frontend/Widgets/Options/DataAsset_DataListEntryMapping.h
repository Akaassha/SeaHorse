// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "DataAsset_DataListEntryMapping.generated.h"

class UListDataObjectBase;
class UWidgetListEntryBase;
/**
 * 
 */
UCLASS()
class SEAHORSE_API UDataAsset_DataListEntryMapping : public UDataAsset
{
	GENERATED_BODY()

public:
	TSubclassOf<UWidgetListEntryBase> FindEntryWidgetClassByDataObject(UListDataObjectBase* InDataObject) const;
	
private:
	UPROPERTY(EditDefaultsOnly)
	TMap<TSubclassOf<UListDataObjectBase>, TSubclassOf<UWidgetListEntryBase>> DataObjectListEntryMap;
};
