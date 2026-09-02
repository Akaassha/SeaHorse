// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameMode.h"
#include "SHGameMode.generated.h"

class ASHHand;
class ASHCard;
class ASHPlayerState;
class UCardEffectFragment;
class UCardEffectTask;
enum class EPlayerSelectionPurpose : uint8;

class UTurnComponent;
class UDeckComponent;

/**
 * 
 */
UCLASS()
class SEAHORSE_API ASHGameMode : public AGameMode
{
	GENERATED_BODY()
	
public:
	//Begin AGameMode Interface
	virtual void HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer) override;
	//End AGameMode Interface

	bool AreCardsPairCompatible(ASHCard* CardA, ASHCard* CardB);

	void ActivatePair(ASHPlayerState* PlayerState, ASHCard* CardA, ASHCard* CardB);

	UTurnComponent* GetTurnComponent() const { return TurnComponent; }

	void MovePairToVictoryStack(ASHPlayerState* PlayerState, ASHCard* CardA, ASHCard* CardB);

	void CardActivateEffect(ASHPlayerState* InActivatingPlayer, ASHCard* CardA, ASHCard* CardB);

	void FinishEffectTask(UCardEffectTask* CardEffectTask);
	void RequestPlayerSelection(UCardEffectTask* Task, ASHPlayerState* SelectingPlayer,
		const TArray<ASHPlayerState*>& Candidates, EPlayerSelectionPurpose Purpose);
	void SubmitPlayerSelection(ASHPlayerState* SelectingPlayer, ASHPlayerState* SelectedPlayer);
	bool RequestActivationPairSelection(UCardEffectTask* Task, ASHPlayerState* SelectingPlayer,
		const TArray<ASHCard*>& CandidateCards);
	bool SubmitActivationPairSelection(ASHPlayerState* SelectingPlayer, ASHCard* SelectedCard);
	bool IsWaitingForPlayerSelection() const
	{
		return !PendingPlayerSelections.IsEmpty() || !PendingPairSelections.IsEmpty();
	}
	void PassHandsToLeft();
	void MoveAllActivationPairsToVictoryStacks();
	bool TransferCardToPlayer(ASHPlayerState* FromPlayer, ASHPlayerState* ToPlayer,
		TSubclassOf<class UCardDefinition> CardDefinition);

	bool TryFinishGame();

protected:
	UPROPERTY(EditDefaultsOnly, Category = "Systems")
	TSubclassOf<UTurnComponent> TurnComponentClass;

	UPROPERTY(EditDefaultsOnly, Category = "Systems")
	TSubclassOf<UDeckComponent> DeckComponentClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cards")
	TSubclassOf<ASHHand> HandClass;

	UPROPERTY(EditDefaultsOnly)
	int32 ExpectedPlayerCount = 3;

private:
	void TryStartGame();
	void StartGame();
	void AssignSeats();
	void RefreshPlayerScore(ASHPlayerState* PlayerState);
	bool PlayerHasAutomaticLossCard(ASHPlayerState* PlayerState) const;

	ASHHand* FindAvailableHand() const;

	bool bGameStarted = false;

	UPROPERTY(Transient)
	TObjectPtr<UDeckComponent> DeckComponent;

	UPROPERTY(Transient)
	TObjectPtr<UTurnComponent> TurnComponent;

	UPROPERTY()
	TArray<TObjectPtr<UCardEffectTask>> ActiveEffectTasks;

	struct FPendingPlayerSelection
	{
		TObjectPtr<UCardEffectTask> Task;
		TArray<TObjectPtr<ASHPlayerState>> Candidates;
	};

	TMap<TObjectPtr<ASHPlayerState>, FPendingPlayerSelection> PendingPlayerSelections;

	struct FPendingPairSelection
	{
		TObjectPtr<UCardEffectTask> Task;
		TArray<TObjectPtr<ASHCard>> CandidateCards;
	};

	TMap<TObjectPtr<ASHPlayerState>, FPendingPairSelection> PendingPairSelections;
};
