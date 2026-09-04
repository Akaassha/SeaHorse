// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "CardEffectTask.generated.h"

class ASHPlayerState;
class ASHCard;
class ASHHand;

UENUM(BlueprintType)
enum class EPlayerSelectionPurpose : uint8
{
    PlayerWhoWillDraw,
    PlayerToDrawFrom,
    PlayerToSkipTurn,
    CardTransferRecipient
};

UENUM(BlueprintType)
enum class ECardEffectPairDisposition : uint8
{
	MoveToVictoryStack,
	KeepOnTable
};

/**
 * 
 */
UCLASS(Abstract, Blueprintable)
class SEAHORSE_API UCardEffectTask : public UObject
{
	GENERATED_BODY()
	
public:
    void Initialize(ASHPlayerState* InActivatingPlayer, ASHCard* InCardA, ASHCard* InCardB,
        FName InEffectPresentationId);

    UFUNCTION(BlueprintNativeEvent)
    void StartEffect();

    UFUNCTION(BlueprintCallable)
    void FinishEffect();

	/** Override for card abilities whose activating pair must remain on the table. */
	UFUNCTION(BlueprintNativeEvent, Category = "Card Effect")
	ECardEffectPairDisposition GetPairDisposition() const;
	virtual ECardEffectPairDisposition GetPairDisposition_Implementation() const;

    void RequestPlayerSelection(const TArray<ASHPlayerState*>& Candidates, EPlayerSelectionPurpose Purpose);
    void RequestParticipantSelection(const TArray<ASHHand*>& Candidates, EPlayerSelectionPurpose Purpose);
    bool RequestActivationPairSelection(const TArray<ASHCard*>& CandidateCards);
    virtual void HandlePlayerSelected(ASHPlayerState* SelectedPlayer);
    virtual void HandleParticipantSelected(ASHHand* SelectedHand);
    virtual void HandleActivationPairSelected(ASHPlayerState* PairOwner, ASHCard* SelectedCardA, ASHCard* SelectedCardB);

    ASHPlayerState* GetActivatingPlayer() const { return ActivatingPlayer; }
    ASHCard* GetCardA() const { return CardA; }
    ASHCard* GetCardB() const { return CardB; }
    FName GetEffectPresentationId() const { return EffectPresentationId; }
	bool IsFinished() const { return bFinished; }

protected:
    UPROPERTY()
    TObjectPtr<ASHPlayerState> ActivatingPlayer;

    UPROPERTY()
    TObjectPtr<ASHCard> CardA;

    UPROPERTY()
    TObjectPtr<ASHCard> CardB;

    FName EffectPresentationId;
	bool bFinished = false;
};
