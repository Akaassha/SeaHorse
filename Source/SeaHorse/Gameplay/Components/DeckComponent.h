// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "DeckComponent.generated.h"

class ASHCard;
class ASHPlayerState;
class UCardDefinition;

USTRUCT(BlueprintType)
struct FDeckEntry : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TSubclassOf<UCardDefinition> CardDefinition;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (ClampMin = "1"))
	int32 Count = 1;
};

UCLASS(BlueprintType, Blueprintable, ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class SEAHORSE_API UDeckComponent : public UActorComponent
{
	GENERATED_BODY()

public:	
	UDeckComponent();

	UFUNCTION(BlueprintCallable, Category = "Deck")
	void ShuffleDeck();

	UFUNCTION(BlueprintCallable, Category = "Deck")
	ASHPlayerState* DealCards();

	UFUNCTION(BlueprintCallable, Category = "Deck")
	void CreateDeck();

	int32 GetInitialDeckSize() const { return InitialDeckSize; }

protected:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Deck")
	TObjectPtr<UDataTable> DeckDefinition;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Deck")
	TSubclassOf<ASHCard> CardClass;

	UPROPERTY()
	TArray<TObjectPtr<ASHCard>> Deck;

	UFUNCTION(BlueprintNativeEvent, Category = "Deck")
	ASHPlayerState* ChooseFirstDealtPlayer();
	virtual ASHPlayerState* ChooseFirstDealtPlayer_Implementation();

private:
	int32 InitialDeckSize = 0;
};
