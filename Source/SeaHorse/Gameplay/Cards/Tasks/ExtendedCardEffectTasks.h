#pragma once

#include "CoreMinimal.h"
#include "Gameplay/Cards/Tasks/AdditionalDrawEffectTask.h"
#include "ExtendedCardEffectTasks.generated.h"

/** Resolves once the authoritative presentation has released its locks. */
UCLASS(Abstract)
class SEAHORSE_API UResolvedCardEffectTask : public UCardEffectTask
{
	GENERATED_BODY()
protected:
	void ResolveAfterPresentation();
	virtual void ResolveAbility() {}
	virtual bool WaitForOtherEffects() const { return false; }
};

UCLASS()
class SEAHORSE_API UDrawTwoReturnOneEffectTask : public UAdditionalDrawEffectTask
{
	GENERATED_BODY()
public:
	virtual void RecordDrawnCard(ASHCard* Card) override;
	virtual bool CompleteDrawSequence() override;
	// The first execution uses the ordinary draw; the repeat needs its own two cards.
	virtual int32 GetAdditionalDrawCount() const override { return IsRepeatedExecution() ? 2 : 1; }
	virtual void HandleHandCardSelected(ASHCard* Card) override;
private:
	UPROPERTY() TArray<TObjectPtr<ASHCard>> DrawnCards;
	bool bReturned = false;
};

UCLASS()
class SEAHORSE_API UTakeSpecifiedCardEffectTask : public UResolvedCardEffectTask
{
	GENERATED_BODY()
public:
	virtual bool RequiresTargetSelection() const override { return true; }
	virtual void StartEffect_Implementation() override;
	virtual void HandleParticipantSelected(ASHHand* Hand) override;
protected:
	virtual void ResolveAbility() override;
private:
	UPROPERTY() TObjectPtr<ASHHand> Source;
};

UCLASS()
class SEAHORSE_API UStealSelectedPairEffectTask : public UResolvedCardEffectTask
{
	GENERATED_BODY()
public:
	virtual bool RequiresTargetSelection() const override { return true; }
	virtual void StartEffect_Implementation() override;
	virtual void HandleActivationPairSelected(ASHPlayerState* Owner, ASHCard* A, ASHCard* B) override;
	virtual ECardEffectPairDisposition GetPairDisposition_Implementation() const override
	{
		return bTransferredPair ? ECardEffectPairDisposition::MoveToVictoryStack : ECardEffectPairDisposition::KeepOnTable;
	}
protected:
	virtual void ResolveAbility() override;
private:
	TArray<ASHCard*> GetCandidates(ASHPlayerState* Player) const;
	bool bTransferredPair = false;
	UPROPERTY() TObjectPtr<ASHPlayerState> Source;
	UPROPERTY() TObjectPtr<ASHCard> SelectedCard;
};

UCLASS()
class SEAHORSE_API URotateActivationZonesRightEffectTask : public UResolvedCardEffectTask
{
	GENERATED_BODY()
public:
	virtual void StartEffect_Implementation() override { ResolveAfterPresentation(); }
	virtual float GetRepeatPresentationDelay() const override { return TransferPresentationDuration; }
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Presentation", meta = (ClampMin = "0.0", Units = "s"))
	float TransferPresentationDuration = 1.5f;
protected:
	virtual bool WaitForOtherEffects() const override { return true; }
	virtual void ResolveAbility() override;
};

UCLASS()
class SEAHORSE_API URemoveSelectedPairEffectTask : public UResolvedCardEffectTask
{
	GENERATED_BODY()
public:
	virtual bool RequiresTargetSelection() const override { return true; }
	virtual void StartEffect_Implementation() override;
	virtual void HandleActivationPairSelected(ASHPlayerState* Owner, ASHCard* A, ASHCard* B) override;
	virtual ECardEffectPairDisposition GetPairDisposition_Implementation() const override;
protected:
	virtual void ResolveAbility() override;
private:
	UPROPERTY() TObjectPtr<ASHPlayerState> Source;
	UPROPERTY() TObjectPtr<ASHCard> SelectedCard;
	bool bRemovedOtherPair = false;
};

UCLASS()
class SEAHORSE_API UShuffleAllHandsEffectTask : public UResolvedCardEffectTask
{
	GENERATED_BODY()
public:
	virtual void StartEffect_Implementation() override { ResolveAfterPresentation(); }
protected:
	virtual bool WaitForOtherEffects() const override { return true; }
	virtual void ResolveAbility() override;
};
