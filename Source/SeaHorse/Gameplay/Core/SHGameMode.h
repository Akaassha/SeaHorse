// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameMode.h"
#include "SHGameMode.generated.h"

class ASHHand;
class ASHCard;
class UCardDefinition;
class ASHPlayerState;

USTRUCT(BlueprintType)
struct FDeckEntry : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TSubclassOf<UCardDefinition> CardDefinition;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (ClampMin = "1"))
	int32 Count = 1;
};

UENUM(BlueprintType)
enum class ETurnPhaseEndReason : uint8
{
	None,
	AutoSkipped,
	PlayerSkipped,
	PairCreated,
	CardDrawn
};

/**
 * 
 */
UCLASS()
class SEAHORSE_API ASHGameMode : public AGameMode
{
	GENERATED_BODY()
	
public:
	//Begin AGameMode Interface
	virtual void HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer) override;
	//End AGameMode Interface

	void CreateDeck();

	void SetInitialDealtCardCount(int32 InitialDeckSize) { DeckSize = InitialDeckSize; }
	int32 GetInitialDealtCardCount() { return DeckSize; };

	bool AreCardsPairCompatible(ASHCard* CardA, ASHCard* CardB);

	void ActivatePair(ASHPlayerState* PlayerState, ASHCard* CardA, ASHCard* CardB);

	UFUNCTION(BlueprintNativeEvent, Category = "Turn")
	ETurnPhase GetNextTurnPhase(ETurnPhase CurrentPhase, ETurnPhaseEndReason Reason);
	virtual ETurnPhase GetNextTurnPhase_Implementation(ETurnPhase CurrentPhase, ETurnPhaseEndReason Reason);

	UFUNCTION(BlueprintNativeEvent, Category = "Turn")
	ASHPlayerState* ChooseNextPlayer(ASHPlayerState* CurrentPlayer);
	virtual ASHPlayerState* ChooseNextPlayer_Implementation(ASHPlayerState* CurrentPlayer);

	void CompleteCurrentPhase(ETurnPhaseEndReason Reason);

	void SkipCurrentPhase(ASHPlayerState* RequestingPlayer);

	void EndTurn();

	void EnterTurnPhase(ETurnPhase NewPhase);

	bool IsPairingActionUsed() { return bPairingActionUsed; };
	void SetPairingActionUsed(bool NewValue) { bPairingActionUsed = NewValue; };

protected:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cards")
	TObjectPtr<UDataTable> DeckDefinition;

	UPROPERTY()
	TArray<TObjectPtr<ASHCard>> Deck;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cards")
	TSubclassOf<ASHCard> CardClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cards")
	TSubclassOf<ASHHand> HandClass;

	UFUNCTION(BlueprintCallable)
	void ShuffleDeck();

	UFUNCTION(BlueprintCallable)
	void DealCards();

	UPROPERTY(EditDefaultsOnly)
	int32 ExpectedPlayerCount = 3;

	void StartTurn();

	UFUNCTION(BlueprintNativeEvent, Category = "Game|Setup")
	ASHPlayerState* ChooseStartingPlayer();
	virtual ASHPlayerState* ChooseStartingPlayer_Implementation();

	UFUNCTION(BlueprintNativeEvent, Category = "Game|Setup")
	ASHPlayerState* ChooseFirstDealtPlayer();
	virtual ASHPlayerState* ChooseFirstDealtPlayer_Implementation();

private:
	void TryStartGame();
	void StartGame();
	void AssignSeats();

	ASHHand* FindAvailableHand() const;

	bool bGameStarted = false;

	int32 DeckSize = -1;

	bool bPairingActionUsed = false;
};
