#pragma once

#include "CoreMinimal.h"
#include "Gameplay/Cards/Tasks/ExtendedCardEffectTasks.h"
#include "Gameplay/Presentation/HandRevealTypes.h"
#include "RevealHandEffectTask.generated.h"

class ASHHandRevealPawn;
class ASHPlayerController;
class APawn;
class UHandRevealWidget;

/** Server-owned private hand reveal. No card definition is added to public replication. */
UCLASS()
class SEAHORSE_API URevealHandEffectTask : public UResolvedCardEffectTask
{
	GENERATED_BODY()

#if WITH_DEV_AUTOMATION_TESTS
	friend class FSHHandRevealTest;
#endif

public:
	virtual bool RequiresTargetSelection() const override { return true; }
	virtual void StartEffect_Implementation() override;
	virtual void HandleParticipantSelected(ASHHand* Hand) override;
	virtual void AbandonEffect() override;
	virtual ECardEffectPairDisposition GetPairDisposition_Implementation() const override
	{
		return bShownToViewer ? ECardEffectPairDisposition::MoveToVictoryStack : ECardEffectPairDisposition::KeepOnTable;
	}

	bool IsSessionFor(ASHPlayerState* Player, FGuid InSessionId) const;
	void AcknowledgePresentation(ASHPlayerState* Player, FGuid InSessionId);
	void FinishViewing(ASHPlayerState* Player, FGuid InSessionId);
	void Reorder(ASHPlayerState* Player, FGuid InSessionId, ASHCard* Card, int32 InsertIndex);
	void HandleParticipantDisconnected(ASHPlayerState* Player);
	FGuid GetSessionId() const { return SessionId; }
	ASHHand* GetRevealedHand() const { return Source; }

protected:
	virtual void ResolveAbility() override;

private:
	bool CanTargetHand(ASHHand* Hand) const;
	ASHPlayerState* FindHandPlayer(ASHHand* Hand) const;
	TArray<FSHRevealedHandCard> MakeSnapshot() const;
	void RefreshSession();
	void TryPresentParticipants();
	void OnPresentationTimeout();
	void PublishSnapshot();
	void CompleteSession(ASHPlayerState* DepartingPlayer = nullptr);
	void CloseSession(ASHPlayerState* DepartingPlayer = nullptr);

	UPROPERTY() TObjectPtr<ASHHand> Source;
	UPROPERTY() TObjectPtr<ASHPlayerState> TargetPlayer;
	UPROPERTY() TObjectPtr<ASHPlayerController> ViewerController;
	UPROPERTY() TObjectPtr<ASHPlayerController> TargetController;
	UPROPERTY() TObjectPtr<ASHHandRevealPawn> ViewerPawn;
	UPROPERTY() TObjectPtr<ASHHandRevealPawn> TargetPawn;
	UPROPERTY() TArray<FSHRevealedHandCard> LastSnapshot;
	UPROPERTY() TSubclassOf<UHandRevealWidget> SessionWidgetClass;
	TWeakObjectPtr<APawn> OriginalTargetPawn;
	TWeakObjectPtr<AActor> OriginalTargetViewTarget;
	FGuid SessionId;
	FTimerHandle SessionWatchTimer;
	FTimerHandle PresentationTimeoutTimer;
	bool bOriginalTargetAutoManageCamera = true;
	bool bTargetPossessionAttempted = false;
	bool bLastCanReorder = false;
	bool bRevealingHuman = false;
	bool bViewerReady = false;
	bool bTargetReady = false;
	/** Remains true after teardown so only an acknowledged reveal consumes the pair. */
	bool bShownToViewer = false;
	bool bOpened = false;
	bool bClosed = false;
};
