// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Frontend/Widgets/Options/DataObjects/ListDataObjectBase.h"
#include "ListDataObjectValue.generated.h"

class FOptionsDataInteractionHelper;
/**
 * 
 */
UCLASS(Abstract)
class SEAHORSE_API UListDataObjectValue : public UListDataObjectBase
{
	GENERATED_BODY()

public:
	void SetDataDynamicGetter(const TSharedPtr<FOptionsDataInteractionHelper>& InDynamicGetter);
	void SetDataDynamicSetter(TSharedPtr<FOptionsDataInteractionHelper> InDynamicSetter);

	void SetDefaultValueFromString(const FString& InDefaultValue) { DefaultStringValue = InDefaultValue; }

	//~ Begin UListDataObjectBase Interface
	virtual bool HasDefauldValue() const { return DefaultStringValue.IsSet(); }
	//~ End UListDataObjectBase Interface
	
protected:
	FString GetDefaultValueAsString() const { return DefaultStringValue.GetValue(); }

	TSharedPtr<FOptionsDataInteractionHelper> DataDynamicGetter;
	TSharedPtr<FOptionsDataInteractionHelper> DataDynamicSetter;

private:
	TOptional<FString> DefaultStringValue;
};
