// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "SeaHorse/Gameplay/Cards/Fragments/CardFragment.h"
#include "SeaHorse/Gameplay/Core/SHGameState.h"
#include "CardActivationRulesFragment.generated.h"

UENUM(BlueprintType)
enum class ECardActivationRules : uint8
{
    OwnTurn,
    OutsideOwnTurn,
    AnyTurn
};

/**
 * 
 */
UCLASS(BlueprintType)
class SEAHORSE_API UCardActivationRulesFragment : public UCardFragment
{
	GENERATED_BODY()
	
public:
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
    bool bCanBeActivated = true;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
    ECardActivationRules TurnRestriction = ECardActivationRules::OwnTurn;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
    TArray<ETurnPhase> AllowedPhases;
};
