#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "SeaHorse/Gameplay/Core/SHGameState.h"
#include "TurnComponent.generated.h"

class ASHPlayerState;
class UCardEffectTask;
struct FActivatedPair;

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
	TArray<TObjectPtr<ASHPlayerState>> Sources;
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
	bool CanDrawCard(ASHPlayerState* DrawingPlayer, ASHPlayerState* SourcePlayer) const;
	void HandleCardDrawn(ASHPlayerState* DrawingPlayer, ASHPlayerState* SourcePlayer);
	void ScheduleAdditionalDraw(UCardEffectTask* EffectTask, ASHPlayerState* PlayerState, EAdditionalDrawSourceRule SourceRule);
	void SetForcedDrawSource(ASHPlayerState* DrawingPlayer, ASHPlayerState* SourcePlayer);
	void ScheduleSkippedTurn(ASHPlayerState* PlayerState);

protected:
	UFUNCTION(BlueprintNativeEvent, Category = "Turn")
	ETurnPhase GetNextTurnPhase(ETurnPhase CurrentPhase, ETurnPhaseEndReason Reason) const;
	virtual ETurnPhase GetNextTurnPhase_Implementation(ETurnPhase CurrentPhase, ETurnPhaseEndReason Reason) const;

	UFUNCTION(BlueprintNativeEvent, Category = "Turn")
	ASHPlayerState* ChooseNextPlayer(ASHPlayerState* CurrentPlayer) const;
	virtual ASHPlayerState* ChooseNextPlayer_Implementation(ASHPlayerState* CurrentPlayer) const;

private:
	void EndTurn();
	void EnterTurnPhase(ETurnPhase NewPhase);
	void FinishAdditionalDraw();
	void UpdateForcedDrawGuidance(ASHPlayerState* DrawingPlayer);
	void ClearDrawGuidance(ASHPlayerState* DrawingPlayer);
	ASHPlayerState* GetFirstForcedDrawSource(const ASHPlayerState* DrawingPlayer) const;
	ASHGameState* GetSHGameState() const;
	void CheckServerAuthority() const;

	bool bPairingActionUsed = false;

	UPROPERTY(Transient)
	TObjectPtr<ASHPlayerState> AdditionalDrawPlayer;

	UPROPERTY(Transient)
	TObjectPtr<ASHPlayerState> FirstDrawSource;

	UPROPERTY(Transient)
	TObjectPtr<UCardEffectTask> AdditionalDrawEffectTask;

	EAdditionalDrawSourceRule AdditionalDrawSourceRule = EAdditionalDrawSourceRule::SamePlayer;
	bool bWaitingForAdditionalDraw = false;

	UPROPERTY(Transient)
	TMap<TObjectPtr<ASHPlayerState>, FForcedDrawSourceQueue> ForcedDrawSources;

	UPROPERTY(Transient)
	TMap<TObjectPtr<ASHPlayerState>, int32> PendingSkippedTurns;
};
