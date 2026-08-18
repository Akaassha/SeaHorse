// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameState.h"
#include "SHGameState.generated.h"

class ASHPlayerState;

UENUM(BlueprintType)
enum class ETurnPhase : uint8
{
	None,
	FirstPairing,
	DrawCard,
	SecondPairing
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnTurnStateChanged, ASHPlayerState*, CurrentPlayer, ETurnPhase, TurnPhase);

/**
 * 
 */
UCLASS()
class SEAHORSE_API ASHGameState : public AGameState
{
	GENERATED_BODY()
	
public:
	UFUNCTION(BlueprintPure, Category = "Turn")
	bool IsCurrentPlayer(const ASHPlayerState* PlayerState) const;

	UFUNCTION(BlueprintPure, Category = "Turn")
	ASHPlayerState* GetCurrentPlayer();

	UFUNCTION(BlueprintPure, Category = "Turn")
	ETurnPhase GetTurnPhase() const;

	UFUNCTION(BlueprintImplementableEvent)
	void OnCurrentPlayerChanged();

	//UFUNCTION(BlueprintImplementableEvent)
	//void OnTurnStateChanged(ASHPlayerState* NewCurrentPlayer, ETurnPhase CurrentPhase, bool bIsMyTurn);

	UFUNCTION(BlueprintImplementableEvent)
	void OnTurnPhaseChanged();

	UPROPERTY(BlueprintAssignable, Category = "Turn")
	FOnTurnStateChanged OnTurnStateChanged;

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

	bool IsMatchReady() {return bMatchReady; }

	UPROPERTY(ReplicatedUsing = OnRep_CurrentPlayer)
	TObjectPtr<ASHPlayerState> CurrentPlayer;

	UPROPERTY(ReplicatedUsing = OnRep_TurnPhase)
	ETurnPhase CurrentTurnPhase = ETurnPhase::None;

	UFUNCTION()
	void OnRep_CurrentPlayer();

	UFUNCTION()
	void OnRep_TurnPhase();

	void SetCurrentPlayer(ASHPlayerState* PlayerState);

	void SetTurnPhase(ETurnPhase NewTurnPhase);

protected:
	UPROPERTY(ReplicatedUsing = OnRep_MatchReady, BlueprintReadOnly)
	bool bMatchReady;

private:
	void HandleMatchReady();
	void NotifyTurnStateChanged();
};
