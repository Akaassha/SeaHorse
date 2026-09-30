#pragma once

#include "CoreMinimal.h"
#include "Gameplay/Cards/Tasks/ExtendedCardEffectTasks.h"
#include "Gameplay/Presentation/HandRevealTypes.h"
#include "CompareHandsEffectTask.generated.h"

class ASHHandRevealPawn;
class ASHPlayerController;
class UHandRevealWidget;

/**
 * Compares two human hands and lets the owner of the smaller hand take exactly
 * the size difference from the larger hand through a private two-hand view.
 */
UCLASS()
class SEAHORSE_API UCompareHandsEffectTask : public UResolvedCardEffectTask
{
	GENERATED_BODY()

#if WITH_DEV_AUTOMATION_TESTS
	friend class FSHCompareHandsEffectTest;
#endif

public:
	virtual bool RequiresTargetSelection() const override { return true; }
	virtual void StartEffect_Implementation() override;
	virtual void HandleParticipantSelected(ASHHand* Hand) override;
	virtual void AbandonEffect() override;
	virtual ECardEffectPairDisposition GetPairDisposition_Implementation() const override
	{
		return bConsumePair ? ECardEffectPairDisposition::MoveToVictoryStack : ECardEffectPairDisposition::KeepOnTable;
	}
	virtual bool WasEffectSuccessful_Implementation() const override { return bConsumePair && RemainingTransfers == 0; }

	bool IsSessionFor(ASHPlayerState* Player, FGuid InSessionId) const;
	void AcknowledgePresentation(ASHPlayerState* Player, FGuid InSessionId);
	void TransferCard(ASHPlayerState* Player, FGuid InSessionId, ASHCard* Card, int32 InsertIndex);
	void HandleParticipantDisconnected(ASHPlayerState* Player);
	FGuid GetSessionId() const { return SessionId; }

protected:
	virtual void ResolveAbility() override;

private:
	bool CanTargetHand(ASHHand* Hand) const;
	ASHPlayerState* FindHandPlayer(ASHHand* Hand) const;
	TArray<FSHRevealedHandCard> MakeSnapshot(const ASHHand* Hand) const;
	TArray<FSHRevealedHandCard> MakeSnapshotForViewer(const TArray<FSHRevealedHandCard>& Snapshot,
		const ASHHand* Hand, const ASHPlayerState* Viewer) const;
	void RefreshSession();
	void TryPresentParticipants();
	void OnPresentationTimeout();
	void PublishSnapshot(bool bForce = false);
	void CompleteSession(ASHPlayerState* DepartingPlayer = nullptr);
	void CloseSession(ASHPlayerState* DepartingPlayer = nullptr);

	UPROPERTY() TObjectPtr<ASHPlayerState> SelectedPlayer;
	UPROPERTY() TObjectPtr<ASHPlayerState> DrawingPlayer;
	UPROPERTY() TObjectPtr<ASHHand> LargerHand;
	UPROPERTY() TObjectPtr<ASHHand> ReceivingHand;
	UPROPERTY() TObjectPtr<ASHPlayerController> ActivatorController;
	UPROPERTY() TObjectPtr<ASHPlayerController> SelectedController;
	UPROPERTY() TObjectPtr<ASHHandRevealPawn> ActivatorPawn;
	UPROPERTY() TObjectPtr<ASHHandRevealPawn> SelectedPawn;
	UPROPERTY() TArray<FSHRevealedHandCard> LastLargerSnapshot;
	UPROPERTY() TArray<FSHRevealedHandCard> LastReceivingSnapshot;
	UPROPERTY() TSubclassOf<UHandRevealWidget> SessionWidgetClass;
	FGuid SessionId;
	FTimerHandle SessionWatchTimer;
	FTimerHandle PresentationTimeoutTimer;
	int32 RemainingTransfers = 0;
	bool bActivatorReady = false;
	bool bSelectedReady = false;
	bool bConsumePair = false;
	bool bOpened = false;
	bool bClosed = false;
};
