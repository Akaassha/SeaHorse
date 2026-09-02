// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "CardEffectTask.generated.h"

class ASHPlayerState;
class ASHCard;

UENUM(BlueprintType)
enum class EPlayerSelectionPurpose : uint8
{
    PlayerWhoWillDraw,
    PlayerToDrawFrom,
    PlayerToSkipTurn,
    CardTransferRecipient
};
/**
 * 
 */
UCLASS(Abstract, Blueprintable)
class SEAHORSE_API UCardEffectTask : public UObject
{
	GENERATED_BODY()
	
public:
    void Initialize(ASHPlayerState* InActivatingPlayer, ASHCard* InCardA,  ASHCard* InCardB);

    UFUNCTION(BlueprintNativeEvent)
    void StartEffect();

    UFUNCTION(BlueprintCallable)
    void FinishEffect();

    void RequestPlayerSelection(const TArray<ASHPlayerState*>& Candidates, EPlayerSelectionPurpose Purpose);
    bool RequestActivationPairSelection(const TArray<ASHCard*>& CandidateCards);
    virtual void HandlePlayerSelected(ASHPlayerState* SelectedPlayer);
    virtual void HandleActivationPairSelected(ASHPlayerState* PairOwner, ASHCard* SelectedCardA, ASHCard* SelectedCardB);

    ASHPlayerState* GetActivatingPlayer() const { return ActivatingPlayer; }
    ASHCard* GetCardA() const { return CardA; }
    ASHCard* GetCardB() const { return CardB; }

protected:
    UPROPERTY()
    TObjectPtr<ASHPlayerState> ActivatingPlayer;

    UPROPERTY()
    TObjectPtr<ASHCard> CardA;

    UPROPERTY()
    TObjectPtr<ASHCard> CardB;
};
