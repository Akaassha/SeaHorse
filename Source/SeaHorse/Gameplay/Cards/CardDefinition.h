// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "CardDefinition.generated.h"

class UCardFragment;
/**
 * 
 */
UCLASS(Blueprintable, BlueprintType, Abstract)
class SEAHORSE_API UCardDefinition : public UObject
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Display")
	FText CardName;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Display")
	FText SkillName;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Display", meta = (DisplayName = "Skill Description"))
	FText SkillDesc;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Display")
	FText AdditionalText;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Display", meta = (DisplayName = "Card Texture"))
	TObjectPtr<UTexture2D> CardTextrue;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Instanced, Category = "Fragments")
	TArray<TObjectPtr<UCardFragment>> CardFragments;

	UFUNCTION(BlueprintPure, meta = (DeterminesOutputType = "FragmentClass"))
	static const UCardFragment* FindFragmentByClass(const TSubclassOf<UCardDefinition> CardDefinition, const TSubclassOf< UCardFragment> FragmentClass);
};
