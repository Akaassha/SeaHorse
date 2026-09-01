// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "VictoryStack.generated.h"

class ASHCard;
class ASHHand;

UCLASS()
class SEAHORSE_API AVictoryStack : public AActor
{
	GENERATED_BODY()
	
public:	
	void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const;

	// Sets default values for this actor's properties
	AVictoryStack();

	void AddPair(ASHCard* CardA, ASHCard* CardB);
	void RefreshCardsPresentation();

	UFUNCTION(BlueprintPure, Category = "Score")
	int32 GetPairCount() const { return ReplicatedCards.Num() / 2; }

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

	// Local presentation data consumed by BP_VictoryStack.
	UPROPERTY(BlueprintReadOnly)
	TArray<TObjectPtr<ASHCard>> Cards;

	UPROPERTY(ReplicatedUsing = OnRep_ReplicatedCards)
	TArray<TObjectPtr<ASHCard>> ReplicatedCards;

	UFUNCTION()
	void OnRep_ReplicatedCards();

	UFUNCTION(BlueprintImplementableEvent)
	void RefreshCardsLayout();

public:	
	// Called every frame
	virtual void Tick(float DeltaTime) override;

	FTransform GetLayout() { return LayoutTransform; };

private:
	ASHHand* FindOwningLogicalHand() const;
	void SetPresentedCards(const TArray<TObjectPtr<ASHCard>>& NewCards);

	FTransform LayoutTransform;
};
