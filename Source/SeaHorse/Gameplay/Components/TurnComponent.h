#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "SeaHorse/Gameplay/Core/SHGameState.h"
#include "SeaHorse/Gameplay/SHHand.h"
#include "TurnComponent.generated.h"

class ASHPlayerState;
class UCardEffectTask;

UENUM(BlueprintType)
enum class ETurnPhaseEndReason : uint8
{
	None,
	AutoSkipped,
	PlayerSkipped,
	PairCreated,
	CardDrawn
};

UENUM(BlueprintType)
enum class EAdditionalDrawSourceRule : uint8
{
	SamePlayer,
	DifferentPlayer
};

USTRUCT()
struct FForcedDrawSourceQueue
{
	GENERATED_BODY()

	UPROPERTY(Transient)
	TArray<TObjectPtr<ASHHand>> Sources;
};

USTRUCT()
struct FPendingAdditionalDraw
{
	GENERATED_BODY()

	UPROPERTY(Transient)
	TObjectPtr<UCardEffectTask> EffectTask;

	UPROPERTY(Transient)
	TObjectPtr<ASHPlayerState> Player;

	EAdditionalDrawSourceRule SourceRule = EAdditionalDrawSourceRule::SamePlayer;
};

UCLASS(BlueprintType, Blueprintable, ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class SEAHORSE_API UTurnComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UTurnComponent();

	void InitializeTurns(ASHPlayerState* StartingPlayer);
	void CompleteCurrentPhase(ETurnPhaseEndReason Reason);
	void SkipCurrentPhase(ASHPlayerState* RequestingPlayer);

	bool IsPairingActionUsed() const { return bPairingActionUsed; }
	void MarkPairingActionUsed() { bPairingActionUsed = true; }

	bool CanActivatePair(ASHPlayerState* RequestingPlayer, const FActivatedPair& ActivatedPair) const;
	/** Shared server/client rule evaluation. Does not grant authority to activate. */
	static bool CanActivatePairForState(const ASHGameState* GameState,
		const ASHPlayerState* RequestingPlayer, const FActivatedPair& ActivatedPair);
	bool CanDrawCard(ASHPlayerState* DrawingPlayer, ASHPlayerState* SourcePlayer) const;
	bool CanDrawCardFromHand(ASHPlayerState* DrawingPlayer, ASHHand* SourceHand) const;
	void HandleCardDrawn(ASHPlayerState* DrawingPlayer, ASHPlayerState* SourcePlayer);
	void HandleCardDrawnFromHand(ASHPlayerState* DrawingPlayer, ASHHand* SourceHand, ASHCard* DrawnCard = nullptr);
	void FinishAdditionalDraw();
	ASHHand* GetInitialDrawSourceHand() const { return FirstDrawSourceHand; }
	void ScheduleAdditionalDraw(UCardEffectTask* EffectTask, ASHPlayerState* PlayerState, EAdditionalDrawSourceRule SourceRule);
	void SetForcedDrawSource(ASHPlayerState* DrawingPlayer, ASHPlayerState* SourcePlayer);
	void SetForcedDrawSourceHand(ASHPlayerState* DrawingPlayer, ASHHand* SourceHand);
	void ScheduleSkippedTurn(ASHPlayerState* PlayerState);
	void RegisterPendingPairSettlement(ASHCard* CardA, ASHCard* CardB);
	void NotifyPairSettled(ASHCard* CardA, ASHCard* CardB);
	void NotifyEffectTaskFinished();

	/** Server-side named lock for BP presentation that must finish before the next turn. */
	void BeginTurnTransitionBlock(FName EffectId);
	void FinishTurnTransitionBlock(FName EffectId);
	bool HasNamedTurnTransitionBlocks() const { return !NamedTurnTransitionBlocks.IsEmpty(); }
	bool HasUnsettledPairs() const { return !PendingPairSettlements.IsEmpty(); }

protected:
	UFUNCTION(BlueprintNativeEvent, Category = "Turn")
	ETurnPhase GetNextTurnPhase(ETurnPhase CurrentPhase, ETurnPhaseEndReason Reason) const;
	virtual ETurnPhase GetNextTurnPhase_Implementation(ETurnPhase CurrentPhase, ETurnPhaseEndReason Reason) const;

	UFUNCTION(BlueprintNativeEvent, Category = "Turn")
	ASHPlayerState* ChooseNextPlayer(ASHPlayerState* CurrentPlayer) const;
	virtual ASHPlayerState* ChooseNextPlayer_Implementation(ASHPlayerState* CurrentPlayer) const;

private:
#if WITH_DEV_AUTOMATION_TESTS
	friend class FSHSixPlayerSeatsTest;
	friend class FSHExpansionEffectsTest;
	friend class FSHReportedEffectRegressionsTest;
	friend class FSHPanchoAllCardsTest;
#endif
#if WITH_DEV_AUTOMATION_TESTS
	friend class FSHDrawRulesTest;
	friend class FSHTargetedActivationPresentationTest;
#endif
	void EndTurn();
	void EnterTurnPhase(ETurnPhase NewPhase);
	void BeginWaitingForAdditionalDraw();
	void UpdateForcedDrawGuidance(ASHPlayerState* DrawingPlayer);
	UFUNCTION()
	void HandleForcedSourceCardsChanged(int32 CardCount);
	void ClearDrawGuidance(ASHPlayerState* DrawingPlayer);
	ASHHand* GetFirstForcedDrawSourceHand(const ASHPlayerState* DrawingPlayer) const;
	ASHGameState* GetSHGameState() const;
	void CheckServerAuthority() const;
	bool HasTurnTransitionBlockers() const;
	void TryCompleteDeferredEndTurn();

	bool bPairingActionUsed = false;

	UPROPERTY(Transient)
	TObjectPtr<ASHPlayerState> AdditionalDrawPlayer;

	UPROPERTY(Transient)
	TObjectPtr<ASHHand> FirstDrawSourceHand;
	UPROPERTY(Transient)
	TObjectPtr<ASHCard> FirstDrawnCard;
	bool bWaitingForDrawReturn = false;

	UPROPERTY(Transient)
	TObjectPtr<UCardEffectTask> AdditionalDrawEffectTask;

	EAdditionalDrawSourceRule AdditionalDrawSourceRule = EAdditionalDrawSourceRule::SamePlayer;
	bool bWaitingForAdditionalDraw = false;
	int32 RemainingSequenceDraws = 1;

	UPROPERTY(Transient)
	TArray<FPendingAdditionalDraw> PendingAdditionalDraws;

	UPROPERTY(Transient)
	TMap<TObjectPtr<ASHPlayerState>, FForcedDrawSourceQueue> ForcedDrawSources;

	UPROPERTY(Transient)
	TMap<TObjectPtr<ASHPlayerState>, int32> PendingSkippedTurns;

	UPROPERTY(Transient)
	TArray<FActivatedPair> PendingPairSettlements;

	UPROPERTY(Transient)
	TMap<FName, int32> NamedTurnTransitionBlocks;

	bool bEndTurnRequested = false;
};
