// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "SeaHorse/Gameplay/Cards/Tasks/CardEffectTask.h"
#include "SeaHorse/Gameplay/Presentation/PairTargetingIndicator.h"
#include "Gameplay/Presentation/HandRevealTypes.h"
#include "SHPlayerController.generated.h"

class ASHHand;
class ASHPlayerState;
class ASHGameState;
class ASHCard;
class UCardDefinition;
class UMeshComponent;
class UCardInfoWidget;
class ASHHandRevealPawn;
class UHandRevealWidget;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FSHOrphanedRatfolkRemovalResult, bool, bRemoved);

struct FEffectOutlineMeshState
{
	bool bRenderCustomDepth = false;
	int32 StencilValue = 0;
	ERendererStencilMask WriteMask = ERendererStencilMask::ERSM_Default;
	float CardHighlightState = 0.0f;
};

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
	ASHPlayerController();
	/** Optional discard of an unpairable Ratfolk in your own pairing phase; validated by the server. */
	UFUNCTION(BlueprintCallable, Server, Reliable, Category = "Cards|Optional Rules")
	void ServerRemoveOrphanedRatfolk(ASHCard* Card);
	UPROPERTY(BlueprintAssignable, Category = "Cards|Optional Rules")
	FSHOrphanedRatfolkRemovalResult OnOrphanedRatfolkRemovalResult;
	UFUNCTION(Client, Reliable)
	void ClientOrphanedRatfolkRemovalResult(bool bRemoved);
	virtual void Tick(float DeltaSeconds) override;
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual bool InputKey(const FInputKeyEventArgs& Params) override;
	virtual void AutoManageActiveCameraTarget(AActor* SuggestedTarget) override;

	/** Private, temporary view of the hand revealed by Bodgy. Real cards stay on the table. */
	UFUNCTION(Client, Reliable)
	void ClientBeginHandReveal(FGuid SessionId, ASHHand* SourceHand, ASHHandRevealPawn* RevealPawn,
		const TArray<FSHRevealedHandCard>& Cards, bool bCanReorder, bool bCanFinish,
		TSubclassOf<UHandRevealWidget> WidgetClass);
	UFUNCTION(Client, Reliable)
	void ClientUpdateHandReveal(FGuid SessionId, const TArray<FSHRevealedHandCard>& Cards, bool bCanReorder);
	/** Private two-hand view used by Diego. Only the drawing player receives transfer permission. */
	UFUNCTION(Client, Reliable)
	void ClientBeginHandComparison(FGuid SessionId, ASHHand* LargerHand, ASHHand* ReceivingHand,
		ASHHandRevealPawn* RevealPawn, const TArray<FSHRevealedHandCard>& LargerCards,
		const TArray<FSHRevealedHandCard>& ReceivingCards, int32 RemainingTransfers,
		bool bCanTransfer, TSubclassOf<UHandRevealWidget> WidgetClass);
	UFUNCTION(Client, Reliable)
	void ClientUpdateHandComparison(FGuid SessionId, const TArray<FSHRevealedHandCard>& LargerCards,
		const TArray<FSHRevealedHandCard>& ReceivingCards, int32 RemainingTransfers, bool bCanTransfer);
	UFUNCTION(Client, Reliable)
	void ClientEndHandReveal(FGuid SessionId);
	UFUNCTION(Server, Reliable)
	void ServerReorderRevealedHand(FGuid SessionId, ASHCard* Card, int32 InsertIndex);
	UFUNCTION(Server, Reliable)
	void ServerFinishHandReveal(FGuid SessionId);
	UFUNCTION(Server, Reliable)
	void ServerAcknowledgeHandReveal(FGuid SessionId);
	UFUNCTION(Server, Reliable)
	void ServerTransferComparedHandCard(FGuid SessionId, ASHCard* Card, int32 InsertIndex);
	UFUNCTION(BlueprintCallable, Category = "Cards|Hand Reveal")
	void FinishHandReveal();
	UFUNCTION(BlueprintPure, Category = "Cards|Hand Reveal")
	ASHHandRevealPawn* GetActiveHandRevealPawn() const { return ActiveHandRevealPawn; }
	UFUNCTION(BlueprintPure, Category = "Cards|Hand Reveal")
	bool IsViewingRevealedHand() const { return ActiveHandRevealSession.IsValid(); }

	/** Local hand hover; the area between resting and raised poses retains focus while the card lifts. */
	UFUNCTION(BlueprintCallable, Category = "Cards|Hover")
	ASHCard* GetHandCardUnderCursor();
	/** Shared local target for hover and Blueprint click/drag input. */
	UFUNCTION(BlueprintPure, Category = "Cards|Hover")
	bool GetCardInteractionUnderCursor(ASHCard*& Card, FVector& Location);
	UFUNCTION(BlueprintPure, Category = "Cards|Hover")
	ASHCard* GetPointerPressedCard() const { return PointerPressedCard.Get(); }
	/** Cursor movement on the fixed press plane, unaffected by hover mesh animation. */
	UFUNCTION(BlueprintPure, Category = "Cards|Hover")
	FVector GetCardDragCursorLocation() const;

	UFUNCTION(BlueprintPure, Category = "Cards|Inspection")
	bool CanInspectCard(const ASHCard* Card) const;
	UFUNCTION(BlueprintCallable, Category = "Cards|Inspection")
	bool ShowCardInfo(ASHCard* Card);
	UFUNCTION(BlueprintCallable, Category = "Cards|Inspection")
	void CloseCardInfo();
	UFUNCTION(BlueprintPure, Category = "Cards|Inspection")
	ASHCard* GetInspectedCard() const;

	/** Designer-authored subclass of CardInfoWidget. Unassigned means inspection UI is disabled. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cards|Inspection")
	TSubclassOf<UCardInfoWidget> CardInfoWidgetClass;

	UFUNCTION(Client, Reliable)
	void ClientOfferCardReaction(int32 OfferId, ASHCard* ReactionCard, ASHCard* TargetCard, TSubclassOf<class UCardReactionPrompt> WidgetClass);
	UFUNCTION(Client, Reliable)
	void ClientCloseCardReaction(int32 OfferId);
	UFUNCTION(Server, Reliable)
	void ServerRespondToCardReaction(int32 OfferId, bool bAccept);

	UFUNCTION(BlueprintCallable)
	void TrySetupTableView();

	/** Called once on the local controller after its PlayerState and the match GameState are ready. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Match|UI")
	void OnLocalMatchUIReady(ASHPlayerState* LocalPlayerState, ASHGameState* MatchGameState);

	/** Local drag preview used by hand layout components. Gameplay remains server authoritative. */
	void BeginLocalCardDrag(ASHCard* Card);
	void EndLocalCardDrag(ASHCard* Card);
	ASHCard* GetLocallyDraggedCard() const { return LocallyDraggedCard; }
	void UpdateLocalCardDropPreview(ASHCard* Card, int32 InsertIndex, bool bOwnHandReorder = false);

	UFUNCTION(BlueprintCallable, Server, Reliable)
	void ServerSkipCurrentPhase();

	/** True only for the local player hosting the server (also true in standalone). */
	UFUNCTION(BlueprintPure, Category = "Match")
	bool IsMatchHost() const;

	/** Requests a fresh match for all players. Only accepted from the host after game end. */
	UFUNCTION(BlueprintCallable, Server, Reliable, Category = "Match")
	void ServerRestartMatch();

	UFUNCTION(Server, Reliable)
	void ServerSetCardDropPreview(ASHCard* Card, int32 InsertIndex);

	UFUNCTION(Server, Reliable)
	void ServerSetCardDropDecision(ASHCard* Card, bool bCommitDraw, int32 InsertIndex);

	UFUNCTION(Server, Reliable)
	void ServerReorderOwnCard(ASHCard* Card, int32 InsertIndex);

	UFUNCTION(BlueprintCallable, Server, Reliable, Category = "Card Effects")
	void ServerSubmitPlayerSelection(ASHPlayerState* SelectedPlayer);

	/** Requests server cancellation of the current target selection. Bind ESC to this later. */
	UFUNCTION(BlueprintCallable, Category = "Card Effects")
	bool CancelEffectTargeting();

	UFUNCTION(Server, Reliable)
	void ServerCancelEffectTargeting(ASHCard* CardA, ASHCard* CardB);

	/** Called by a world-space player picker. Returns true when the click was consumed. */
	bool TrySubmitPlayerSelectionForPicker(ASHPlayerState* SelectedPlayer);
	bool TrySubmitParticipantSelectionForHand(ASHHand* SelectedHand);

	UFUNCTION(Server, Reliable)
	void ServerSubmitParticipantSelection(ASHHand* SelectedHand);

	UFUNCTION(Client, Reliable)
	void ClientRequestParticipantSelection(const TArray<ASHHand*>& Candidates, EPlayerSelectionPurpose Purpose);

	/** Consumes card clicks while a hand/participant selection is pending. */
	bool TrySubmitParticipantSelectionForCard(const ASHCard* Card);

	UFUNCTION(BlueprintPure, Category = "Card Effects")
	ASHPlayerState* FindPlayerStateForCard(const ASHCard* Card) const;

	UFUNCTION(Client, Reliable)
	void ClientRequestPlayerSelection(const TArray<ASHPlayerState*>& Candidates,
		EPlayerSelectionPurpose Purpose);

	/** Legacy card-based selector hook; native player-picker requests no longer dispatch it. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Card Effects")
	void OnPlayerSelectionRequested(const TArray<ASHPlayerState*>& Candidates, EPlayerSelectionPurpose Purpose);

	UFUNCTION(Client, Reliable)
	void ClientRequestActivationPairSelection(const TArray<ASHCard*>& CandidateCards);
	UFUNCTION(Client, Reliable)
	void ClientRequestHandCardSelection(const TArray<ASHCard*>& CandidateCards);
	UFUNCTION(Server, Reliable)
	void ServerSubmitHandCardSelection(ASHCard* Card);
	UFUNCTION(Client, Reliable)
	void ClientRequestHandCardsSelection(const TArray<ASHCard*>& Cards, int32 Min, int32 Max,
		TSubclassOf<class UCardSelectionPrompt> WidgetClass);
	/** Wait for the hand's shuffled order to replicate and finish moving before enabling draws. */
	UFUNCTION(Client, Reliable)
	void ClientRequestHandCardsSelectionAfterShuffle(ASHHand* SourceHand, const TArray<ASHCard*>& ShuffledOrder,
		const TArray<ASHCard*>& Candidates, int32 Min, int32 Max, TSubclassOf<class UCardSelectionPrompt> WidgetClass);
	UFUNCTION(Server, Reliable)
	void ServerSubmitHandCardsSelection(const TArray<ASHCard*>& Cards);

	UFUNCTION(Client, Reliable)
	void ClientShowCardEffectMessage(const FText& Message);
	/** Override to show a game-specific notice; default is a nonblocking four-second message. */
	UFUNCTION(BlueprintNativeEvent, Category="Card Effects")
	void ShowCardEffectMessage(const FText& Message);
	virtual void ShowCardEffectMessage_Implementation(const FText& Message);
	/** Designer-authored layout. Unassigned uses the simple native notice. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Card Effects|Messages")
	TSubclassOf<class UCardEffectMessageWidget> CardEffectMessageWidgetClass;
	UFUNCTION(BlueprintCallable, Category = "Card Effects|Messages")
	void CloseCardEffectMessage();
	UFUNCTION(BlueprintPure, Category = "Card Effects|Messages")
	UCardEffectMessageWidget* GetActiveCardEffectMessage() const { return ActiveCardEffectMessage; }

	UFUNCTION(Client, Reliable)
	void ClientSetPairTargetSelection(ASHCard* CardA, ASHCard* CardB,
		bool bSelectingTarget, FName EffectPresentationId);

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
	bool bLocalMatchUIInitialized = false;

private:
	FGuid ActiveHandRevealSession;
	/** Session announced before its owner-only presentation pawn has mapped on this client. */
	FGuid PendingHandRevealSession;
	UPROPERTY(Transient) TObjectPtr<ASHHandRevealPawn> ActiveHandRevealPawn;
	UPROPERTY(Transient) TObjectPtr<UHandRevealWidget> ActiveHandRevealWidget;
	UPROPERTY(Transient) TWeakObjectPtr<AActor> ViewTargetBeforeHandReveal;
	bool bCanFinishHandReveal = false;
	bool bCursorBeforeHandReveal = false;
	bool bAutoCameraBeforeHandReveal = true;
	bool bClickEventsBeforeHandReveal = false;
	bool bMouseOverBeforeHandReveal = false;
	/** Keeps a delayed possession/ClientRestart from replacing the restored table camera after a reveal closes. */
	TWeakObjectPtr<AActor> HandRevealCameraRestoreTarget;
	FTimerHandle HandRevealCameraRestoreTimer;
	void BeginHandRevealCameraRestore(AActor* RestoreTarget);
	void MaintainHandRevealCameraRestore();
	void FinishHandRevealCameraRestore();
	void PreparePendingHandReveal(FGuid SessionId);
	friend class FSHHandCursorHoverTest;
	friend class FSHHandSelectionTest;
	friend class FSHShuffleSelectionBarrierTest;
	friend class FSHHandHoverMotionTest;
	friend class FSHHandHoverCorridorTest;
	friend class FSHHandHoverMapTest;
	void ResolveCardCursorHit(const FHitResult& Hit, const FVector& RayStart, const FVector& RayEnd, FHitResult& OutHit);
	void ResetHandCursorHover();
	TWeakObjectPtr<ASHCard> HandCursorHoverCard;
	TWeakObjectPtr<ASHHand> HandCursorHoverOwner;
	FTransform HandCursorHoverBaseTransform;
	FTransform HandCursorHoverFocusedTransform;
	bool bHasHandCursorHoverFocusedTransform = false;
	TWeakObjectPtr<ASHCard> PointerPressedCard;
	FVector PointerPressedLocation = FVector::ZeroVector;
	friend class UCardInfoWidget;
#if WITH_DEV_AUTOMATION_TESTS
	friend class FSHCardInspectionTest;
#endif
	UPROPERTY(Transient)
	TObjectPtr<UCardInfoWidget> ActiveCardInfoWidget;
	UPROPERTY(Transient)
	TWeakObjectPtr<ASHCard> InspectedCard;
	void ValidateCardInfoAccess();

	UPROPERTY(Transient)
	TObjectPtr<class UCardReactionPrompt> ActiveReactionPrompt;
	int32 ActiveReactionOfferId = INDEX_NONE;
	bool bCursorBeforeReaction = false;
#if WITH_DEV_AUTOMATION_TESTS
	friend class FSHGameplayEffectInputTest;
	friend class FSHExpansionEffectsTest;
	friend class FSHCardSelectionWidgetTest;
	friend class UCardSelectionPrompt;
	friend class FSHReportedEffectRegressionsTest;
	friend class FSHCardReactionsTest;
	friend class FSHLocalMatchUIReadinessTest;
	friend class FSHEffectTargetOutlineTest;
#endif
	bool TryHandleEffectSelectionClick(AActor* HitActor);
	bool bConsumeEffectSelectionRelease = false;
	bool bAwaitingPlayerSelectionResponse = false;
	UFUNCTION()
	void HandleTurnStateChanged(ASHPlayerState* CurrentPlayer, ETurnPhase TurnPhase);
	void KeepDraggedCardAboveOtherCards();
	void UpdateLocalActivatablePairHover();
	void StartPairTargetingIndicator(ASHCard* CardA, ASHCard* CardB, FName EffectPresentationId);
	void StopPairTargetingIndicator();
	void UpdatePairTargetingIndicator();
	bool ResolveTargetingCursor(FVector& OutLocation, bool& bOutValidTarget,
		AActor*& OutValidTargetActor) const;
	void SetCardHoverSuppressedForTargeting(bool bSuppressed);
	void SetCurrentValidEffectTarget(AActor* NewTarget);
	bool IsValidEffectTarget(const AActor* Actor) const;
	void UpdateEffectTargetOutlines(AActor* HoveredActor);
	void RestoreEffectTargetOutlines();
	TMap<TWeakObjectPtr<UMeshComponent>, FEffectOutlineMeshState> EffectOutlineMeshes;
	void ClearLocalPlayerSelection();
	void ClearLocalEffectSelectionState();
	ASHHand* FindVisualHandForPlayer(const ASHPlayerState* PlayerState) const;
	ASHHand* FindNearestDropHand(const ASHCard* DraggedCard) const;
	void ReconcileRotatedHandsPresentation();
	bool TryRoutePairPresentation(const FPendingPairPresentationEvent& Event);
	void FlushPendingPairPresentationEvents();
	FTimerHandle TableSetupRetryTimer;
	UPROPERTY(Transient)
	TObjectPtr<UCardEffectMessageWidget> ActiveCardEffectMessage;
	FTimerHandle CardEffectMessageTimer;
	FTimerHandle RotatedHandsReconcileTimer;
	int32 RemainingRotatedHandsReconciles = 0;
	UPROPERTY(Transient)
	TArray<TObjectPtr<ASHHand>> LocalParticipantSelectionCandidates;
	UPROPERTY(Transient)
	TArray<TObjectPtr<ASHPlayerState>> LocalPlayerSelectionCandidates;
	UPROPERTY(Transient)
	TArray<TObjectPtr<ASHCard>> LocalActivationPairSelectionCandidates;
	UPROPERTY(Transient)
	TArray<TObjectPtr<ASHCard>> LocalHandCardSelectionCandidates;
	UPROPERTY(Transient) TObjectPtr<ASHHand> WaitingForShuffleHand;
	UPROPERTY(Transient) TArray<TObjectPtr<ASHCard>> WaitingForShuffleOrder;
	UPROPERTY(Transient) TArray<TObjectPtr<ASHCard>> WaitingForShuffleCandidates;
	UPROPERTY(Transient) TSubclassOf<class UCardSelectionPrompt> WaitingForShuffleWidget;
	int32 WaitingForShuffleMin = 1;
	int32 WaitingForShuffleMax = 1;
	FTimerHandle ShuffleSelectionWaitTimer;
	double ShuffleLayoutWaitStartedAt = -1.0;
	void PollShuffledHandSelection();
	void ClearShuffledHandSelectionWait();
	UPROPERTY(Transient)
	TArray<TObjectPtr<ASHCard>> LocallySelectedEffectCards;
	int32 LocalSelectionMin = 1;
	int32 LocalSelectionMax = 1;
	UPROPERTY(Transient)
	TObjectPtr<UCardSelectionPrompt> SelectionPromptWidget;
	UPROPERTY(Transient)
	TSubclassOf<UCardSelectionPrompt> SelectionPromptClass;
	int32 LocalCardSelectionSerial = 0;
	void RefreshSelectionPrompt();
	void ConfirmEffectCardSelection();
	void ToggleEffectCardSelection(ASHCard* Card);
	UPROPERTY(Transient)
	TArray<TObjectPtr<ASHHand>> LocalGuidedDrawHands;
	UPROPERTY(Transient)
	TObjectPtr<ASHCard> LocallyDraggedCard;

	UPROPERTY(Transient)
	TObjectPtr<ASHCard> LastActivatableHoverCard;

	/** Optional BP subclass can add particles or further presentation without owning gameplay state. */
	UPROPERTY(EditDefaultsOnly, Category = "Card Effects|Targeting")
	TSubclassOf<APairTargetingIndicator> PairTargetingIndicatorClass;

	UPROPERTY(EditDefaultsOnly, Category = "Card Effects|Targeting")
	FPairTargetingIndicatorStyle DefaultPairTargetingStyle;

	/** Local presentation styles resolved using CardEffectFragment.EffectPresentationId. */
	UPROPERTY(EditDefaultsOnly, Category = "Card Effects|Targeting")
	TMap<FName, FPairTargetingIndicatorStyle> PairTargetingStyles;

	UPROPERTY(Transient)
	TObjectPtr<APairTargetingIndicator> PairTargetingIndicator;

	UPROPERTY(Transient)
	TObjectPtr<ASHCard> TargetingSourceCardA;

	UPROPERTY(Transient)
	TObjectPtr<ASHCard> TargetingSourceCardB;

	UPROPERTY(Transient)
	TObjectPtr<AActor> CurrentValidEffectTarget;

	FName CurrentTargetingEffectPresentationId;
	bool bSavedEnableMouseOverEvents = false;
	bool bCardHoverSuppressedForTargeting = false;

	UPROPERTY(EditDefaultsOnly, Category="Card Drag", meta=(ClampMin="0.0", Units="cm"))
	double DraggedCardZClearance = 5.0;
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
