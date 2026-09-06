// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "LoadingScreenInterface.generated.h"

// This class does not need to be modified.
UINTERFACE(BlueprintType, MinimalAPI)
class ULoadingScreenInterface : public UInterface
{
	GENERATED_BODY()
};

/**
 * 
 */
class SEAHORSE_API ILoadingScreenInterface
{
	GENERATED_BODY()

	// Add interface functions to this class. This is the class that will be inherited to implement this interface.
public:
	UFUNCTION(BlueprintNativeEvent)
	void OnLoadingScreenActivated();
	//virtual void OnLoadingScreenActivated_Implementation();

	UFUNCTION(BlueprintNativeEvent)
	void OnLoadingScreenDeActivated();
	//virtual void OnLoadingScreenDeActivated_Implementation();
};
