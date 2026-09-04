// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "SeaHorse/Gameplay/Cards/Fragments/CardFragment.h"
#include "CardEffectFragment.generated.h"

class UCardEffectTask;
/**
 * 
 */
UCLASS(BlueprintType)
class SEAHORSE_API UCardEffectFragment : public UCardFragment
{
	GENERATED_BODY()
	
public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	TSubclassOf<UCardEffectTask> EffectTaskClass;

	/** Identifier used by Blueprint to choose a visual representation for this card effect. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Presentation")
	FName EffectPresentationId;
};

UCLASS(BlueprintType, EditInlineNew)
class SEAHORSE_API UTransferCardEffectFragment : public UCardEffectFragment
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Card Transfer")
	TSubclassOf<class UCardDefinition> CardDefinitionToTransfer;
};
