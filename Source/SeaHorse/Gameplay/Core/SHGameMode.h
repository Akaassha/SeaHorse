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
enum class ECardEffectPairDisposition : uint8;

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
	virtual void InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage) override;
	virtual void PreLogin(const FString& Options, const FString& Address, const FUniqueNetIdRepl& UniqueId, FString& ErrorMessage) override;
	//Begin AGameMode Interface
	virtual void HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer) override;
	virtual void Logout(AController* Exiting) override;
	void RespondToCardReaction(ASHPlayerState* Player, int32 OfferId, bool bAccept);
	bool HasPendingCardReaction() const { return bReactionWindowOpen; }
	//End AGameMode Interface

	bool AreCardsPairCompatible(ASHCard* CardA, ASHCard* CardB);

	void ActivatePair(ASHPlayerState* PlayerState, ASHCard* CardA, ASHCard* CardB);

	UTurnComponent* GetTurnComponent() const { return TurnComponent; }

	void MovePairToVictoryStack(ASHPlayerState* PlayerState, ASHCard* CardA, ASHCard* CardB);

	void CardActivateEffect(ASHPlayerState* InActivatingPlayer, ASHCard* CardA, ASHCard* CardB);

	void FinishEffectTask(UCardEffectTask* CardEffectTask);
	bool CancelEffectTargetSelection(ASHPlayerState* SelectingPlayer, ASHCard* CardA, ASHCard* CardB);
	void FlushCompletedEffectPairs();
	void RequestStoredPairActivation(ASHPlayerState* ActivatingPlayer, ASHCard* SelectedCard);
	void NotifyActivationPairSettled(ASHCard* CardA, ASHCard* CardB);
	void TryProcessQueuedPairActivations();
	void RequestPlayerSelection(UCardEffectTask* Task, ASHPlayerState* SelectingPlayer,
		const TArray<ASHPlayerState*>& Candidates, EPlayerSelectionPurpose Purpose);
	void SubmitPlayerSelection(ASHPlayerState* SelectingPlayer, ASHPlayerState* SelectedPlayer);
	void RequestParticipantSelection(UCardEffectTask* Task, ASHPlayerState* SelectingPlayer,
		const TArray<ASHHand*>& Candidates, EPlayerSelectionPurpose Purpose);
	void SubmitParticipantSelection(ASHPlayerState* SelectingPlayer, ASHHand* SelectedHand);
	bool RequestActivationPairSelection(UCardEffectTask* Task, ASHPlayerState* SelectingPlayer,
		const TArray<ASHCard*>& CandidateCards);
	bool SubmitActivationPairSelection(ASHPlayerState* SelectingPlayer, ASHCard* SelectedCard);
	bool RequestHandCardSelection(UCardEffectTask* Task, ASHPlayerState* SelectingPlayer, const TArray<ASHCard*>& Cards);
	void SubmitHandCardSelection(ASHPlayerState* SelectingPlayer, ASHCard* Card);
	bool RequestHandCardsSelection(UCardEffectTask* Task, ASHPlayerState* Player, ASHHand* Source, const TArray<ASHCard*>& Cards, int32 Min, int32 Max);
	void SubmitHandCardsSelection(ASHPlayerState* Player, const TArray<ASHCard*>& Cards);
	bool IsWaitingForPlayerSelection() const
	{
		return bReactionWindowOpen || !PendingPlayerSelections.IsEmpty() || !PendingParticipantSelections.IsEmpty() ||
			!PendingPairSelections.IsEmpty() || !PendingHandCardSelections.IsEmpty();
	}
	bool HasActiveEffectTasks() const { return bReactionWindowOpen || !ActiveEffectTasks.IsEmpty() || !PendingSuccessfulActivations.IsEmpty(); }
	void PassHandsToLeft();
	bool TransferStoredPair(ASHHand* Source, ASHHand* Target, ASHCard* Card);
	void RotateActivationZonesRight(ASHCard* ExcludedCard);
	void ShuffleAndRedealHands();
	bool RemoveStoredPairFromGame(ASHHand* Hand, ASHCard* Card);
	bool HasOtherActiveEffects(const UCardEffectTask* Except) const;
	void MoveAllActivationPairsToVictoryStacks();
	bool TransferCardToHand(ASHHand* FromHand, ASHHand* ToHand,
		TSubclassOf<class UCardDefinition> CardDefinition);

	bool TryFinishGame();

protected:
	UPROPERTY(EditDefaultsOnly, Category = "Systems")
	TSubclassOf<UTurnComponent> TurnComponentClass;

	UPROPERTY(EditDefaultsOnly, Category = "Systems")
	TSubclassOf<UDeckComponent> DeckComponentClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Players", meta = (ClampMin = "2", ClampMax = "6"))
	int32 ExpectedPlayerCount = 2;

private:
#if WITH_DEV_AUTOMATION_TESTS
	friend struct FSHNewEffectsWorld;
	friend class FSHNewCardEffectsTest;
	friend class FSHExpansionEffectsTest;
	friend class FSHCardSelectionWidgetTest;
	friend class FSHReportedEffectRegressionsTest;
	friend class FSHCardReactionsTest;
	friend class FSHSupportPairEffectsTest;
	friend class FSHDoubledZoneRotationTest;
	friend class FSHPanchoAllCardsTest;
	friend class FSHDogsReactionChainTest;
	friend class FSHActivationQueueReadinessTest;
	friend class FSHBulkVictoryPresentationTest;
	friend class FSHTargetedActivationPresentationTest;
	friend class FSHRotationPresentationTest;
	friend class FSHDeferredDrawActivationTest;
	friend class FSHSixPlayerSeatsTest;
#endif
	void SetPairTargetSelectionPresentation(UCardEffectTask* Task,
		ASHPlayerState* SelectingPlayer, bool bSelectingTarget);
	bool HasPendingSelection(UCardEffectTask* Task, ASHPlayerState* SelectingPlayer) const;
	void FinishSelectionStep(UCardEffectTask* Task, ASHPlayerState* SelectingPlayer);
	void TryStartGame();
	void StartGame();
	void AssignSeats();
	void InitializeParticipantHands();
	void RefreshPlayerScore(ASHPlayerState* PlayerState);
	bool PlayerHasAutomaticLossCard(ASHPlayerState* PlayerState) const;
	bool HandHasAutomaticLossCard(const ASHHand* Hand) const;

	ASHHand* FindAvailableHand() const;

	bool bGameStarted = false;

	UPROPERTY(Transient)
	TObjectPtr<UDeckComponent> DeckComponent;

	UPROPERTY(Transient)
	TObjectPtr<UTurnComponent> TurnComponent;

	UPROPERTY()
	TArray<TObjectPtr<UCardEffectTask>> ActiveEffectTasks;
	struct FRepeatedPairEffect
	{
		int32 Remaining = 1;
		ECardEffectPairDisposition Disposition;
	};
	TMap<TWeakObjectPtr<ASHCard>, FRepeatedPairEffect> RepeatedPairEffects;
	void RestartRepeatedPairEffect(UCardEffectTask* PreviousTask);
	bool TryUseVictorySubstitute(ASHPlayerState* Player, ASHCard* CardA, ASHCard* CardB);

	struct FCompletedEffectPair
	{
		TObjectPtr<ASHPlayerState> ActivatingPlayer;
		TObjectPtr<ASHCard> CardA;
		TObjectPtr<ASHCard> CardB;
		bool bMoveToVictoryStack = true;
		bool bRestoreReady = false;
	};

	TArray<FCompletedEffectPair> CompletedEffectPairsWaitingForPresentation;

	struct FPendingPairActivation
	{
		TObjectPtr<ASHPlayerState> ActivatingPlayer;
		TObjectPtr<ASHCard> CardA;
		TObjectPtr<ASHCard> CardB;
		bool bClickPresentationStarted = false;
		bool bAbilityStarted = false;
		bool bReactionsChecked = false;
	};

	TArray<FPendingPairActivation> PendingPairActivations;
	struct FSuccessfulActivation
	{
		FPendingPairActivation Activation;
		bool bCaptureChecked = false;
	};
	TArray<FSuccessfulActivation> PendingSuccessfulActivations;
	bool bProcessingSuccessfulActivations = false;
	void QueueSuccessfulActivation(ASHPlayerState* Player, ASHCard* A, ASHCard* B);
	void ProcessSuccessfulActivations();
	bool bProcessingPairActivations = false;
	struct FReactionOption
	{
		TWeakObjectPtr<ASHPlayerState> Player;
		TWeakObjectPtr<ASHCard> CardA;
		TWeakObjectPtr<ASHCard> CardB;
		int64 CreationOrder = 0;
		int32 OfferId = INDEX_NONE;
	};
	// Remaining pairs are queued per owner; every eligible owner can answer concurrently.
	TArray<FReactionOption> ReactionOptions;
	TMap<TWeakObjectPtr<ASHPlayerState>, FReactionOption> ActiveReactionOffers;
	struct FReactionActivation
	{
		FReactionOption Option;
		bool bCancelled = false;
	};
	TArray<FReactionActivation> ReactionChain;
	FPendingPairActivation ReactionRootActivation;
	TWeakObjectPtr<ASHPlayerState> ReactionTargetPlayer;
	TWeakObjectPtr<ASHCard> ReactionTargetA;
	TWeakObjectPtr<ASHCard> ReactionTargetB;
	TMap<TWeakObjectPtr<ASHCard>, TWeakObjectPtr<ASHPlayerState>> PairCaptureRecipients;
	int32 ReactionOfferSerial = 0;
	int32 ReactionWindowSerial = 0;
	bool bReactionWindowOpen = false;
	bool bPostActivationWindow = false;
	bool BeginCardReactions(const FPendingPairActivation& Activation);
	void OpenCardReactionWindow(ASHPlayerState* Player, ASHCard* CardA, ASHCard* CardB);
	void OfferNextCardReaction(ASHPlayerState* Player);
	void ClearCardReactionOffers();
	void CloseCardReactions();
	void ResolveCardReactionChain();
	bool IsReactionOptionValid(const FReactionOption& Option) const;
	void ConsumeReactionPair(const FReactionOption& Option, bool bExecuteEffect);
	void StartQueuedPairAbility(const FPendingPairActivation& PendingActivation);
	void CompleteQueuedPairActivation(ASHCard* CardA, ASHCard* CardB);

	struct FPendingPlayerSelection
	{
		TObjectPtr<UCardEffectTask> Task;
		TArray<TObjectPtr<ASHPlayerState>> Candidates;
		EPlayerSelectionPurpose Purpose;
	};

	TMap<TObjectPtr<ASHPlayerState>, FPendingPlayerSelection> PendingPlayerSelections;

	struct FPendingParticipantSelection
	{
		TObjectPtr<UCardEffectTask> Task;
		TArray<TObjectPtr<ASHHand>> Candidates;
	};

	TMap<TObjectPtr<ASHPlayerState>, FPendingParticipantSelection> PendingParticipantSelections;

	struct FPendingPairSelection
	{
		TObjectPtr<UCardEffectTask> Task;
		TArray<TObjectPtr<ASHCard>> CandidateCards;
		TObjectPtr<ASHHand> SourceHand;
		int32 MinCards = 1;
		int32 MaxCards = 1;
	};

	TMap<TObjectPtr<ASHPlayerState>, FPendingPairSelection> PendingPairSelections;
	TMap<TObjectPtr<ASHPlayerState>, FPendingPairSelection> PendingHandCardSelections;

	/** Server bookkeeping for one continuous, possibly multi-step local targeting session. */
	UPROPERTY(Transient)
	TMap<TObjectPtr<ASHPlayerState>, TObjectPtr<UCardEffectTask>> ActiveTargetPresentations;

	bool DiscoverTableSeats(FString& ErrorMessage);
	int32 TotalSeatCount = 0;
};
