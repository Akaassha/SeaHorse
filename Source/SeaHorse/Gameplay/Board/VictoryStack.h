// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "VictoryStack.generated.h"

class ASHCard;

UCLASS()
class SEAHORSE_API AVictoryStack : public AActor
{
	GENERATED_BODY()
	
public:	
	void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const;

	// Sets default values for this actor's properties
	AVictoryStack();

	void AddPair(ASHCard* CardA, ASHCard* CardB);

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

	UPROPERTY(ReplicatedUsing = OnRep_Cards, BlueprintReadOnly)
	TArray<ASHCard*> Cards;

	UFUNCTION()
	void OnRep_Cards();

	UFUNCTION(BlueprintImplementableEvent)
	void RefreshCardsLayout();

public:	
	// Called every frame
	virtual void Tick(float DeltaTime) override;

	FTransform GetLayout() { return LayoutTransform; };

private:
	TArray<ASHCard*> VictoryStack;
	FTransform LayoutTransform;
};
