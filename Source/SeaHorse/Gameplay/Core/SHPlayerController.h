// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "SHPlayerController.generated.h"

class ASHHand;
class ASHPlayerState;
/**
 * 
 */
UCLASS()
class SEAHORSE_API ASHPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable)
	void TrySetupTableView();

	bool IsTableViewInitialized() { return bTableViewInitialized; }

	UFUNCTION(BlueprintCallable, Server, Reliable)
	void ServerSkipCurrentPhase();

	ASHHand* FindVisualHandForLogicalHand(const ASHHand* LogicalHand) const;

protected:
	UFUNCTION()
	void SetupTableView();

	UFUNCTION(BlueprintCallable)
	int32 GetVisualSeatIndex(int32 PlayerSeatIndex, int32 PlayerCount) const;

	UFUNCTION()
	ASHHand* FindLayoutHand(int32 LayoutSeatIndex) const;

	UFUNCTION(BlueprintCallable, Server, Reliable)
	void ServerTakeCard(ASHCard* Card, int32 InsertIndex);

	UFUNCTION(BlueprintCallable, Server, Reliable)
	void ServerCreatePair(ASHCard* CardA, ASHCard* CardB);

	UFUNCTION(BlueprintCallable, Server, Reliable)
	void ServerActivateStoredPair(ASHCard* Card);

	UFUNCTION(BlueprintImplementableEvent)
	void OnPairActivated(ASHCard* CardA, ASHCard* CardB);

	UFUNCTION(Client, Reliable)
	void ClientReceiveCardDefinition(ASHCard* Card, TSubclassOf<UCardDefinition> CardDefinition);

protected:
	bool bTableViewInitialized = false;

private:
	ASHHand* FindVisualHandForPlayer(const ASHPlayerState* PlayerState) const;
};
