// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Frontend/Widgets/Options/DataObjects/ListDataObjectBase.h"
#include "ListDataObjectCollection.generated.h"

/**
 * 
 */
UCLASS()
class SEAHORSE_API UListDataObjectCollection : public UListDataObjectBase
{
	GENERATED_BODY()
	
public:
	void AddChildListData(UListDataObjectBase* InChildListData);

	//~Begin UListDataObjectBase Interface
	virtual TArray< UListDataObjectBase*> GetAllChildListData() const override;
	virtual bool HasAnyChildListData() const override;
	//~End UListDataObjectBase Interface

private:
	UPROPERTY(Transient)
	TArray<UListDataObjectBase*> ChildListDataArray;

};
