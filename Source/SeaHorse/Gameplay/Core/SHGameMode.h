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

	ASHHand* FindAvailableHand() const;

	bool bGameStarted = false;

	UPROPERTY(Transient)
	TObjectPtr<UDeckComponent> DeckComponent;

	UPROPERTY(Transient)
	TObjectPtr<UTurnComponent> TurnComponent;

	UPROPERTY()
	TArray<TObjectPtr<UCardEffectTask>> ActiveEffectTasks;
};
