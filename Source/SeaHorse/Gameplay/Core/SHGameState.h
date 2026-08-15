// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameState.h"
#include "SHGameState.generated.h"

/**
 * 
 */
UCLASS()
class SEAHORSE_API ASHGameState : public AGameState
{
	GENERATED_BODY()
	
public:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	virtual void AddPlayerState(APlayerState* PlayerState) override;

	void SetMatchReady(bool bReady);

	UFUNCTION()
	void OnRep_MatchReady();

	UPROPERTY(Replicated)
	int32 InitialDealtCardCount = INDEX_NONE;

	int32 GetInitialDealtCardCount() const
	{
		return InitialDealtCardCount;
	}

	void SetInitialDealtCardCount(int32 Count)
	{
		InitialDealtCardCount = Count;
	}

	bool IsMatchReady() {
		return bMatchReady
			;
	}
protected:
	UPROPERTY(ReplicatedUsing = OnRep_MatchReady, BlueprintReadOnly)
	bool bMatchReady;

private:
	void HandleMatchReady();
};
