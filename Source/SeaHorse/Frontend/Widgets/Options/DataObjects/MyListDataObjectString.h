// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Frontend/Widgets/Options/DataObjects/ListDataObjectValue.h"
#include "MyListDataObjectString.generated.h"

/**
 * 
 */
UCLASS()
class SEAHORSE_API UListDataObjectString : public UListDataObjectValue
{
	GENERATED_BODY()

public:
	void AddDynamicOption(const FString& InStringValue, const FText& InDisplayText);
	void AdvanceToNextOption();
	void BackToPreviousOption();
	void OnRotatorInitiatedValueChange(const FText& InNewSelectedText);
	
protected:
	//~Begin UListDataObjectBase Interface
	virtual void OnDataObjectInitialized() override;
	virtual bool CanResetBackToDefaultValue() const override;
	virtual bool TryResetBackToDefaultValue() override;
	virtual bool CanSetToForcedStringValue(const FString& InForcedValue) const override;
	virtual void OnSetToForcedStringValue(const FString& InForcedValue) override;
	//~End UListDataObjectBase Interface

	bool TrySetDisplayTextFromStringValue(const FString& InStringValue);

	FString CurrentStringValue;
	FText CurrentDisplayText;
	TArray<FString> AvailableOptionsStringArray;
	TArray<FText> AvailableOptionsTextArray;

public: 
	FORCEINLINE const TArray<FText>& GetAvaliableOptionsTextArray() const { return AvailableOptionsTextArray; };
	FORCEINLINE FText GetCurrentDisplayText() const { return CurrentDisplayText; };
};

UCLASS()
class SEAHORSE_API UListDataObjectStringBool : public UListDataObjectString
{
	GENERATED_BODY()

public:
	void OverrideTrueDisplayText(const FText& InNewTrueDisplayText);
	void OverrideFalseDisplayText(const FText& InNewFalseDisplayText);
	void SetTrueAsDefaultValue();
	void SetFalseAsDefaultValue();

protected:
	//~Begin UListDataObjectString Interface
	virtual void OnDataObjectInitialized() override;
	//~Begin UListDataObjectString Interface

private:
	void TryInitBoolValues();

	const FString TrueString = TEXT("true");
	const FString FalseString = TEXT("false");


};

UCLASS()
class SEAHORSE_API UListDataObjectStringEnum : public UListDataObjectString
{
	GENERATED_BODY()

public:
	template<typename EnumType>
	void AddEnumOption(EnumType InEnumOption, const FText& InDisplayText)
	{
		const UEnum* StaticEnumOption = StaticEnum<EnumType>();
		const FString ConvertedEnumString = StaticEnumOption->GetNameStringByValue(InEnumOption);

		AddDynamicOption(ConvertedEnumString, InDisplayText);
	}

	template<typename EnumType>
	EnumType GetCurrentValueAsEnum() const
	{
		const UEnum* StaticEnumOption = StaticEnum<EnumType>();

		return (EnumType)StaticEnumOption->GetValueByNameString(CurrentStringValue);
	}

	template<typename EnumType>
	void SetDefaultValueFromEnumOption(EnumType InEnumOption)
	{
		const UEnum* StaticEnumOption = StaticEnum<EnumType>();
		const FString ConvertedEnumString = StaticEnumOption->GetNameStringByValue(InEnumOption);

		SetDefaultValueFromString(ConvertedEnumString);
	}
};

UCLASS()
class SEAHORSE_API UListDataObjectStringInteger : public UListDataObjectString
{
	GENERATED_BODY()

public:
	void AddIntegerOption(int32 InIntegerValue, const FText& InDisplayText);

protected:
	//~Begin UListDataObjectString Interface
	virtual void OnDataObjectInitialized() override;
	virtual void OnEditDependencyDataModified(UListDataObjectBase* ModifiedDependencyData, EOptionsListDataModifyReason ModifyReason) override;
	//~Begin UListDataObjectString Interface
};