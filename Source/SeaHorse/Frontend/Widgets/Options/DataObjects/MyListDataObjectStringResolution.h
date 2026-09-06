// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Frontend/Widgets/Options/DataObjects/MyListDataObjectString.h"
#include "MyListDataObjectStringResolution.generated.h"

/**
 * 
 */
UCLASS()
class SEAHORSE_API UMyListDataObjectStringResolution : public UListDataObjectString
{
	GENERATED_BODY()

public:
	void InitResolutionValue();
	
protected:
	//~Begin UListDataObjectBase Interface
	virtual void OnDataObjectInitialized() override;
	//~End UListDataObjectBase Interface

private:
	FString ResToValueString(const FIntPoint& InResolution) const;
	FText ResToDisplayText(const FIntPoint& InResolution) const;

	FString MaximumAllowedResolution;

public:
	FORCEINLINE FString GetMaximumAllowedResolution() const { return MaximumAllowedResolution; }
};
