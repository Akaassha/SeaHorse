// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerState.h"
#include "SHPlayerState.generated.h"

class ASHHand;
/**
 * 
 */
UCLASS()
class SEAHORSE_API ASHPlayerState : public APlayerState
{
	GENERATED_BODY()

public:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	void SetHand(ASHHand* NewHand);
	ASHHand* GetHand();

	UPROPERTY(ReplicatedUsing = OnRep_SeatIndex, BlueprintReadOnly)
	int32 SeatIndex = INDEX_NONE;

	UFUNCTION()
	void SetSeatIndex(int32 NewSeatIndex);

	UFUNCTION()
	int32 GetSeatIndex();

	UFUNCTION()
	void OnRep_SeatIndex();
	
protected:
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Hand")
	TObjectPtr<ASHHand> Hand;
};
