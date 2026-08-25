// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SHHand.generated.h"

class ASHCard;
class AVictoryStack;
class ASHPlayerState;

USTRUCT(BlueprintType)
struct FActivatedPair
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	TObjectPtr<ASHCard> CardA;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	TObjectPtr<ASHCard> CardB;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	bool bActivated = false;

	bool operator==(const FActivatedPair& Other) const
	{
		return CardA == Other.CardA && CardB == Other.CardB;
	}

	friend uint32 GetTypeHash(const FActivatedPair& Pair)
	{
		return HashCombine(GetTypeHash(Pair.CardA.Get()), GetTypeHash(Pair.CardB.Get()));
	}
};

UCLASS()
class SEAHORSE_API ASHHand : public AActor
{
	GENERATED_BODY()
	
public:	
	// Sets default values for this actor's properties
	ASHHand();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UFUNCTION(BlueprintCallable, BlueprintImplementableEvent)
	void Initialize();

	void SetShowCardFronts(bool bShow);

	UFUNCTION(NetMulticast, Reliable)
	void MulticastPairActivated(ASHCard* CardA, ASHCard* CardB);

	UFUNCTION(BlueprintImplementableEvent)
	void OnPairActivated(ASHCard* CardA, ASHCard* CardB);

	bool RemoveActivationPair(ASHCard* CardA, ASHCard* CardB);

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

	UPROPERTY(ReplicatedUsing = OnRep_Cards)
	TArray<TObjectPtr<ASHCard>> Cards;

	UPROPERTY(ReplicatedUsing = OnRep_ActivatonCards)
	TArray<FActivatedPair> ActivationPairs;
	
	bool bShowCardFronts = false;

	UFUNCTION(BlueprintCallable)
	void AddActivationPair(ASHCard* CardA, ASHCard* CardB);

	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Player Area")
	TObjectPtr<AVictoryStack> VictoryStack;

public:	
	// Called every frame
	virtual void Tick(float DeltaTime) override;

	UFUNCTION(BlueprintCallable)
	void AddCard(ASHCard* Card, int32 Index);

	UFUNCTION(BlueprintCallable, BlueprintPure)
	TArray<ASHCard*> GetCards();

	UFUNCTION(BlueprintCallable, BlueprintPure)
	TArray<FActivatedPair> GetActivationPairs();

	UFUNCTION(BlueprintCallable, BlueprintPure)
	TArray<ASHCard*> GetActivationCards();

	UFUNCTION(BlueprintCallable)
	void RemoveCard(ASHCard* Card);

	UFUNCTION()
	void OnRep_Cards();

	UFUNCTION()
	void OnRep_ActivatonCards();

	UFUNCTION(BlueprintPure)
	int32 GetCardCount() const;

	UFUNCTION(BlueprintCallable, BlueprintImplementableEvent)
	void UpdateCardPositions();

	UPROPERTY(EditInstanceOnly, BlueprintReadOnly)
	int32 LayoutSeatIndex = INDEX_NONE;

	UFUNCTION(BlueprintCallable, BlueprintPure)
	int32 GetLayoutSeatIndex() const;

	UFUNCTION(BlueprintCallable, BlueprintPure)
	const FTransform& GetLayoutTransform() const;

	UPROPERTY(Transient)
	TArray<TObjectPtr<ASHCard>> PreviousCards;

	void RefreshCardsPresentation();

	bool ContainsCard(ASHCard* CardB);

	FActivatedPair* FindActivationPair(ASHCard* Card);

	AVictoryStack* GetVictoryStack() const
	{
		return VictoryStack;
	}

	void SetRepresentedPlayerState(ASHPlayerState* InPlayerState)
	{
		RepresentedPlayerState = InPlayerState;
	}

	UFUNCTION(BlueprintCallable, BlueprintPure)
	ASHPlayerState* GetRepresentedPlayerState() const;

	UFUNCTION(BlueprintCallable, BlueprintPure)
	ASHHand* GetRepresentedHand() const;

private:
	UPROPERTY(Transient)
	TObjectPtr<ASHPlayerState> RepresentedPlayerState;
	
	bool ShouldShowCardFronts();
	FTransform LayoutTransform;
};
