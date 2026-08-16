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

/**
 * 
 */
UCLASS()
class SEAHORSE_API ASHGameMode : public AGameMode
{
	GENERATED_BODY()
	
public:
	//Begin AGameMode Interface
	virtual void PostLogin(APlayerController* NewPlayer) override;
	virtual void Logout(AController* Exiting) override;

	virtual void HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer) override;

	virtual void StartPlay() override;
	//End AGameMode Interface

	void CreateDeck();

	void SetInitialDealtCardCount(int32 InitialDeckSize) { DeckSize = InitialDeckSize; }
	int32 GetInitialDealtCardCount() { return DeckSize; };

	bool AreCardsPairCompatible(ASHCard* CardA, ASHCard* CardB);

	void ActivatePair(ASHPlayerState* PlayerState, ASHCard* CardA, ASHCard* CardB);

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

private:
	void TryStartGame();
	void StartGame();
	void AssignSeats();

	ASHHand* FindAvailableHand() const;

	bool bGameStarted = false;

	int32 DeckSize = -1;

	
};
