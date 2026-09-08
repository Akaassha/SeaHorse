// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "GameplayTagContainer.h"
#include "FrontendFunctionLibrary.generated.h"

class UWidgetActivatableBase;

/**
 * 
 */
UCLASS()
class SEAHORSE_API UFrontendFunctionLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
	
public:
	/** Keeps at most MaxCharacters display characters, adding an ellipsis only if shortened.
	 * The optional ellipsis is extra (not included in MaxCharacters). Nonpositive limits return empty text.
	 * Intended for display: keep the original text for gameplay and identity.
	 */
	UFUNCTION(BlueprintPure, Category = "Frontend Function Library|Text", meta = (ClampMin = "0", Keywords = "shorten truncate nickname ellipsis"))
	static FText ShortenText(const FText& Text, int32 MaxCharacters = 16, bool bAddEllipsis = true);

	UFUNCTION(BlueprintPure, Category = "Frontend Function Library")
	static TSoftClassPtr<UWidgetActivatableBase> GetFrontendSoftWidgetClassByTag(UPARAM(meta = (Categories = "Frontend.Widget")) FGameplayTag InWidgetTag);

	UFUNCTION(BlueprintPure, Category = "Frontend Function Library")
	static TSoftObjectPtr<UTexture2D> GetOptionsSoftImageByTag(UPARAM(meta = (Categories = "Frontend.Image")) FGameplayTag InImageTag);
};
