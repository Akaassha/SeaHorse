// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "SeaHorse/Gameplay/Cards/Fragments/CardFragment.h"
#include "CardEffectFragment.generated.h"

class UCardEffectTask;
class UNiagaraSystem;
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

	/** Optional UMG layout for card selection. None displays no selection panel. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Presentation")
	TSubclassOf<class UCardSelectionPrompt> SelectionWidgetClass;

	/** Optional one-shot presentation; targeted tasks start it after their final accepted target. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Presentation")
	TObjectPtr<UNiagaraSystem> ActivationVFX;

	/** Holds pair/turn presentation until the one-shot animation has finished. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Presentation", meta = (ClampMin = "0.0", Units = "s"))
	float ActivationVFXDuration = 1.5f;
};

UCLASS(BlueprintType, EditInlineNew)
class SEAHORSE_API UTransferCardEffectFragment : public UCardEffectFragment
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Card Transfer")
	TSubclassOf<class UCardDefinition> CardDefinitionToTransfer;
};

UCLASS(BlueprintType, EditInlineNew)
class SEAHORSE_API UStoredPairFilterEffectFragment : public UCardEffectFragment
{
	GENERATED_BODY()
public:
	/** Soft references allow card definitions to be added to the deck later. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Card Transfer", meta = (MetaClass = "/Script/SeaHorse.CardDefinition"))
	TArray<FSoftClassPath> AllowedCardDefinitions;

	TArray<class ASHCard*> GetEligibleCards(const class ASHPlayerState* Activator, const class ASHPlayerState* Owner) const;
};

/** Restricts this card to a named partner and resolves the pair without activation. */
UCLASS(BlueprintType, EditInlineNew)
class SEAHORSE_API UImmediateVictoryPairFragment : public UCardFragment
{
	GENERATED_BODY()
public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta = (MetaClass = "/Script/SeaHorse.CardDefinition"))
	TArray<FSoftClassPath> AllowedPartners;
};

/** A passive stored pair that pays the victory cost of a successful activation. */
UCLASS(BlueprintType, EditInlineNew)
class SEAHORSE_API UVictorySubstituteFragment : public UCardFragment
{
	GENERATED_BODY()
public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta = (MetaClass = "/Script/SeaHorse.CardDefinition"))
	TArray<FSoftClassPath> AllowedCardDefinitions;
};
