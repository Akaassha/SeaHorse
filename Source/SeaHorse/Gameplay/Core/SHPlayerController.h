// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "SeaHorse/Gameplay/Cards/Tasks/CardEffectTask.h"
#include "SHPlayerController.generated.h"

class ASHHand;
class ASHPlayerState;
class ASHCard;
class UCardDefinition;

UENUM(BlueprintType)
enum class ECardDrawGuidanceType : uint8
{
	None UMETA(DisplayName = "None"),
	AdditionalFromSamePlayer UMETA(DisplayName = "Draw Again From The Same Player"),
	AdditionalFromDifferentPlayer UMETA(DisplayName = "Draw Again From A Different Player"),
	ForcedSelectedPlayer UMETA(DisplayName = "Draw From The Selected Player")
};

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

	UFUNCTION(BlueprintCallable, Server, Reliable, Category = "Card Effects")
	void ServerSubmitPlayerSelection(ASHPlayerState* SelectedPlayer);

	UFUNCTION(BlueprintPure, Category = "Card Effects")
	ASHPlayerState* FindPlayerStateForCard(const ASHCard* Card) const;

	UFUNCTION(Client, Reliable)
	void ClientRequestPlayerSelection(const TArray<ASHPlayerState*>& Candidates, EPlayerSelectionPurpose Purpose);

	UFUNCTION(BlueprintImplementableEvent, Category = "Card Effects")
	void OnPlayerSelectionRequested(const TArray<ASHPlayerState*>& Candidates, EPlayerSelectionPurpose Purpose);

	UFUNCTION(Client, Reliable)
	void ClientRequestAdditionalCardDraw(const TArray<ASHPlayerState*>& ValidSources);

	UFUNCTION(BlueprintImplementableEvent, Category = "Card Effects")
	void OnAdditionalCardDrawRequested(const TArray<ASHPlayerState*>& ValidSources);

	UFUNCTION(Client, Reliable)
	void ClientUpdateCardDrawGuidance(
		const TArray<ASHPlayerState*>& ValidSources,
		ECardDrawGuidanceType GuidanceType);

	UFUNCTION(BlueprintImplementableEvent, Category = "Card Effects")
	void OnCardDrawGuidanceUpdated(
		const TArray<ASHPlayerState*>& ValidSources,
		ECardDrawGuidanceType GuidanceType);

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
