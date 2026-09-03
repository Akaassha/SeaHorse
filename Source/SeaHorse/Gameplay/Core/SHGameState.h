// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameState.h"
#include "SHGameState.generated.h"

class ASHPlayerState;
class ASHHand;

USTRUCT(BlueprintType)
struct FSHMatchResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Match")
	TObjectPtr<ASHPlayerState> PlayerState;

	UPROPERTY(BlueprintReadOnly, Category = "Match")
	int32 Points = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Match")
	bool bIsWinner = false;

	UPROPERTY(BlueprintReadOnly, Category = "Match")
	bool bAutomaticallyLost = false;
};

USTRUCT()
struct FSHFinishedMatch
{
	GENERATED_BODY()

	UPROPERTY()
	bool bFinished = false;

	UPROPERTY()
	TArray<FSHMatchResult> Results;

	UPROPERTY()
	TArray<TObjectPtr<ASHHand>> AutomaticallyLosingNPCs;
};

UENUM(BlueprintType)
enum class ETurnPhase : uint8
{
	None,
	FirstPairing,
	DrawCard,
	SecondPairing
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnTurnStateChanged, ASHPlayerState*, CurrentPlayer, ETurnPhase, TurnPhase);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnGameEnded);

/**
 * 
 */
UCLASS()
class SEAHORSE_API ASHGameState : public AGameState
{
	GENERATED_BODY()
	
public:
	UFUNCTION(BlueprintPure, Category = "Participants")
	TArray<ASHHand*> GetNPCHands() const;
	UFUNCTION(BlueprintPure, Category = "Participants")
	TArray<ASHHand*> GetParticipantHands() const { return ParticipantHands; }

	UFUNCTION(BlueprintPure, Category = "Participants")
	int32 GetParticipantCount() const;

	ASHHand* FindParticipantHandBySeat(int32 SeatIndex) const;
	void SetParticipantHands(const TArray<ASHHand*>& NewHands);
	UFUNCTION(BlueprintPure, Category = "Turn")
	bool IsCurrentPlayer(const ASHPlayerState* PlayerState) const;

	UFUNCTION(BlueprintPure, Category = "Turn")
	ASHPlayerState* GetCurrentPlayer() const;

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

	UPROPERTY(BlueprintAssignable, Category = "Match")
	FOnGameEnded OnGameEnded;

	UFUNCTION(BlueprintPure, Category = "Match")
	bool IsGameEnded() const { return FinishedMatch.bFinished; }

	UFUNCTION(BlueprintPure, Category = "Match")
	const TArray<FSHMatchResult>& GetMatchResults() const { return FinishedMatch.Results; }

	UFUNCTION(BlueprintPure, Category = "Match")
	TArray<ASHHand*> GetAutomaticallyLosingNPCs() const { return FinishedMatch.AutomaticallyLosingNPCs; }

	void FinishGame(const TArray<FSHMatchResult>& Results, const TArray<ASHHand*>& AutomaticallyLosingNPCs = {});

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
	UPROPERTY(ReplicatedUsing = OnRep_ParticipantHands, BlueprintReadOnly, Category = "Participants")
	TArray<TObjectPtr<ASHHand>> ParticipantHands;

	UFUNCTION()
	void OnRep_ParticipantHands();
	UPROPERTY(ReplicatedUsing = OnRep_MatchReady, BlueprintReadOnly)
	bool bMatchReady;

	UPROPERTY(ReplicatedUsing = OnRep_FinishedMatch)
	FSHFinishedMatch FinishedMatch;

	UFUNCTION()
	void OnRep_FinishedMatch();

private:
	void HandleMatchReady();
	void NotifyTurnStateChanged();
};
