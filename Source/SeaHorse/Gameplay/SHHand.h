// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SHHand.generated.h"

class ASHCard;

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

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

	UPROPERTY(ReplicatedUsing = OnRep_Cards)
	TArray<TObjectPtr<ASHCard>> Cards;


	
	bool bShowCardFronts = false;

public:	
	// Called every frame
	virtual void Tick(float DeltaTime) override;

	UFUNCTION(BlueprintCallable)
	void AddCard(ASHCard* Card, int32 Index);

	UFUNCTION(BlueprintCallable, BlueprintPure)
	TArray<ASHCard*> GetCards();

	UFUNCTION(BlueprintCallable)
	void RemoveCard(ASHCard* Card);

	UFUNCTION()
	void OnRep_Cards();

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

private:
	FTransform LayoutTransform;
};
