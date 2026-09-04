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

USTRUCT()
struct FPendingPairPresentationEvent
{
	GENERATED_BODY()

	UPROPERTY()
	TObjectPtr<ASHHand> LogicalHand;

	UPROPERTY()
	TObjectPtr<ASHCard> CardA;

	UPROPERTY()
	TObjectPtr<ASHCard> CardB;

	bool bEffectActivation = false;
};

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
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual bool InputKey(const FInputKeyEventArgs& Params) override;

	UFUNCTION(BlueprintCallable)
	void TrySetupTableView();

	bool IsTableViewInitialized() { return bTableViewInitialized; }

	/** Local drag preview used by hand layout components. Gameplay remains server authoritative. */
	void BeginLocalCardDrag(ASHCard* Card);
	void EndLocalCardDrag(ASHCard* Card);
	ASHCard* GetLocallyDraggedCard() const { return LocallyDraggedCard; }
	void UpdateLocalCardDropPreview(ASHCard* Card, int32 InsertIndex, bool bOwnHandReorder = false);

	UFUNCTION(BlueprintCallable, Server, Reliable)
	void ServerSkipCurrentPhase();

	UFUNCTION(Server, Reliable)
	void ServerSetCardDropPreview(ASHCard* Card, int32 InsertIndex);

	UFUNCTION(Server, Reliable)
	void ServerSetCardDropDecision(ASHCard* Card, bool bCommitDraw, int32 InsertIndex);

	UFUNCTION(Server, Reliable)
	void ServerReorderOwnCard(ASHCard* Card, int32 InsertIndex);

	UFUNCTION(BlueprintCallable, Server, Reliable, Category = "Card Effects")
	void ServerSubmitPlayerSelection(ASHPlayerState* SelectedPlayer);

	/** Called by a world-space player picker. Returns true when the click was consumed. */
	bool TrySubmitPlayerSelectionForPicker(ASHPlayerState* SelectedPlayer);

	UFUNCTION(Server, Reliable)
	void ServerSubmitParticipantSelection(ASHHand* SelectedHand);

	UFUNCTION(Client, Reliable)
	void ClientRequestParticipantSelection(const TArray<ASHHand*>& Candidates, EPlayerSelectionPurpose Purpose);

	/** Consumes card clicks while a hand/participant selection is pending. */
	bool TrySubmitParticipantSelectionForCard(const ASHCard* Card);

	UFUNCTION(BlueprintPure, Category = "Card Effects")
	ASHPlayerState* FindPlayerStateForCard(const ASHCard* Card) const;

	UFUNCTION(Client, Reliable)
	void ClientRequestPlayerSelection(const TArray<ASHPlayerState*>& Candidates, EPlayerSelectionPurpose Purpose);

	UFUNCTION(BlueprintImplementableEvent, Category = "Card Effects")
	void OnPlayerSelectionRequested(const TArray<ASHPlayerState*>& Candidates, EPlayerSelectionPurpose Purpose);

	UFUNCTION(Client, Reliable)
	void ClientRequestActivationPairSelection(const TArray<ASHCard*>& CandidateCards);

	UFUNCTION(BlueprintImplementableEvent, Category = "Card Effects")
	void OnActivationPairSelectionRequested(const TArray<ASHCard*>& CandidateCards);

	UFUNCTION(Client, Reliable)
	void ClientRequestAdditionalCardDraw(const TArray<ASHPlayerState*>& ValidSources);

	UFUNCTION(BlueprintImplementableEvent, Category = "Card Effects")
	void OnAdditionalCardDrawRequested(const TArray<ASHPlayerState*>& ValidSources);

	UFUNCTION(Client, Reliable)
	void ClientUpdateCardDrawGuidance(
		const TArray<ASHPlayerState*>& ValidSources,
		ECardDrawGuidanceType GuidanceType);

	UFUNCTION(Client, Reliable)
	void ClientSetGuidedDrawHands(const TArray<ASHHand*>& ValidHands);

	/** Reconciles card layout/fronts after a server-side bulk hand transfer. */
	UFUNCTION(Client, Reliable)
	void ClientReconcileRotatedHands();

	/** Reliable owner-channel fallback for pair presentation events. */
	UFUNCTION(Client, Reliable)
	void ClientNotifyPairPresentation(ASHHand* LogicalHand, ASHCard* CardA, ASHCard* CardB,
		bool bEffectActivation);

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
	ASHHand* FindNearestDropHand(const ASHCard* DraggedCard) const;
	void ReconcileRotatedHandsPresentation();
	bool TryRoutePairPresentation(const FPendingPairPresentationEvent& Event);
	void FlushPendingPairPresentationEvents();
	FTimerHandle TableSetupRetryTimer;
	FTimerHandle RotatedHandsReconcileTimer;
	int32 RemainingRotatedHandsReconciles = 0;
	UPROPERTY(Transient)
	TArray<TObjectPtr<ASHHand>> LocalParticipantSelectionCandidates;
	UPROPERTY(Transient)
	TArray<TObjectPtr<ASHPlayerState>> LocalPlayerSelectionCandidates;
	UPROPERTY(Transient)
	TArray<TObjectPtr<ASHHand>> LocalGuidedDrawHands;
	UPROPERTY(Transient)
	TObjectPtr<ASHCard> LocallyDraggedCard;
	UPROPERTY(Transient)
	TObjectPtr<ASHCard> LastPreviewCard;
	int32 LastPreviewInsertIndex = INDEX_NONE;
	bool bLastPreviewIsOwnHandReorder = false;
	UPROPERTY(Transient)
	TObjectPtr<ASHCard> PendingDropCard;
	int32 PendingDropInsertIndex = INDEX_NONE;
	UPROPERTY(Transient)
	TArray<FPendingPairPresentationEvent> PendingPairPresentationEvents;
};
