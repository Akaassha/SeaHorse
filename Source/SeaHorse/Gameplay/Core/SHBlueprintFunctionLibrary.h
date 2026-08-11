// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "SHBlueprintFunctionLibrary.generated.h"

/**
 * 
 */
UCLASS()
class SEAHORSE_API USHBlueprintFunctionLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
	
public:
	UFUNCTION(BlueprintPure, Category = "SeaHorse|Utilities", meta = (WorldContext = "WorldContextObject"))
	static bool IsEditorWorld(const UObject* WorldContextObject);
};
