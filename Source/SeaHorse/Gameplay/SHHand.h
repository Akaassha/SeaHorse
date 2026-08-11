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

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

	UPROPERTY(ReplicatedUsing = OnRep_Cards)
	TArray<TObjectPtr<ASHCard>> Cards;

public:	
	// Called every frame
	virtual void Tick(float DeltaTime) override;

	UFUNCTION(BlueprintCallable)
	void AddCard(ASHCard* Card, int32 Index);

	UFUNCTION(BlueprintCallable)
	TArray<ASHCard*> GetCards();

	UFUNCTION(BlueprintCallable)
	void RemoveCard(ASHCard* Card);

	UFUNCTION()
	void OnRep_Cards();
};
