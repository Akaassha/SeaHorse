// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "Frontend/FrontendEnumTypes.h"
#include "Frontend/FrontendStructTypes.h"
#include "ListDataObjectBase.generated.h"

#define LIST_DATA_ACCESSOR(DataType, PropertyName) \
	FORCEINLINE DataType Get##PropertyName() const {return PropertyName;} \
	void Set##PropertyName(DataType In##PropertyName) { PropertyName = In##PropertyName;}

/**
 * 
 */
UCLASS(Abstract)
class SEAHORSE_API UListDataObjectBase : public UObject
{
	GENERATED_BODY()
	
public: 
	DECLARE_MULTICAST_DELEGATE_TwoParams(FOnListDataModifiedDelegate, UListDataObjectBase*, EOptionsListDataModifyReason)
	FOnListDataModifiedDelegate OnListDataModified;
	FOnListDataModifiedDelegate OnDependencyDataModified;

	LIST_DATA_ACCESSOR(FName, DataID)
	LIST_DATA_ACCESSOR(FText, DataDisplayName)
	LIST_DATA_ACCESSOR(FText, DescriptionRichText)
	LIST_DATA_ACCESSOR(FText, DisabledRichText)
	LIST_DATA_ACCESSOR(TSoftObjectPtr<UTexture2D>, SoftDescriptionImage)
	LIST_DATA_ACCESSOR(UListDataObjectBase*, ParentData)

	void InitDataObject();

	virtual TArray< UListDataObjectBase*> GetAllChildListData() const { return  TArray< UListDataObjectBase*>(); }
	virtual bool HasAnyChildListData() const { return false; }

	void SetShouldApplySettingsImmediately(bool bShouldApplyRightAway) { bSholdApplyChangeImmediately = bShouldApplyRightAway; }

	virtual bool HasDefauldValue() const { return false; }
	virtual bool CanResetBackToDefaultValue() const { return false; }
	virtual bool TryResetBackToDefaultValue() { return false; }

	void AddEditCondition(const FOptionsDataEditConditionDescriptor& InEditCondition);

	void AddEditDependencyData(UListDataObjectBase* InDependencyData);

	bool IsDataCurrentlyEditable();

protected:
	virtual void OnDataObjectInitialized();

	virtual void NotifyListDataModified(UListDataObjectBase* ModifiedData, EOptionsListDataModifyReason ModifyReason = EOptionsListDataModifyReason::DirectlyModified);

	virtual bool CanSetToForcedStringValue(const FString& InForcedValue) const { return false; }

	virtual void OnSetToForcedStringValue(const FString& InForcedValue) {}

	virtual void OnEditDependencyDataModified(UListDataObjectBase* ModifiedDependencyData, EOptionsListDataModifyReason ModifyReason);

private:
	FName DataID;
	FText DataDisplayName;
	FText DescriptionRichText;
	FText DisabledRichText;
	TSoftObjectPtr<UTexture2D> SoftDescriptionImage;

	UPROPERTY(Transient)
	UListDataObjectBase* ParentData;

	bool bSholdApplyChangeImmediately = false;

	UPROPERTY(Transient)
	TArray<FOptionsDataEditConditionDescriptor> EditConditionTestArray;
};
