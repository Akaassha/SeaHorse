#pragma once

#include "CoreMinimal.h"
#include "Gameplay/Cards/Tasks/ExtendedCardEffectTasks.h"
#include "NewCardEffectTasks.generated.h"

UCLASS()
class SEAHORSE_API URotateHandsLeftEffectTask : public UCardEffectTask
{
	GENERATED_BODY()
public:
	virtual void StartEffect_Implementation() override;
};

UCLASS()
class SEAHORSE_API USkipSelectedPlayerTurnEffectTask : public UResolvedCardEffectTask
{
	GENERATED_BODY()
public:
	virtual bool RequiresTargetSelection() const override { return true; }
	virtual void StartEffect_Implementation() override;
	virtual void HandlePlayerSelected(ASHPlayerState* SelectedPlayer) override;
protected:
	virtual void ResolveAbility() override;
private:
	UPROPERTY() TObjectPtr<ASHPlayerState> TargetPlayer;
};

UCLASS()
class SEAHORSE_API UCollectAllActivationPairsEffectTask : public UCardEffectTask
{
	GENERATED_BODY()
public:
	virtual void StartEffect_Implementation() override;
};

UCLASS()
class SEAHORSE_API UTransferSpecifiedCardEffectTask : public UResolvedCardEffectTask
{
	GENERATED_BODY()
public:
	virtual bool RequiresTargetSelection() const override { return true; }
	virtual void StartEffect_Implementation() override;
	virtual void HandleParticipantSelected(ASHHand* SelectedHand) override;
	virtual ECardEffectPairDisposition GetPairDisposition_Implementation() const override
	{ return bTransferredCard ? ECardEffectPairDisposition::MoveToVictoryStack : ECardEffectPairDisposition::KeepOnTable; }
protected:
	virtual void ResolveAbility() override;
private:
	UPROPERTY() TObjectPtr<ASHHand> Recipient;
	bool bTransferredCard = false;
};

UCLASS()
class SEAHORSE_API UCollectSelectedActivationPairEffectTask : public UResolvedCardEffectTask
{
	GENERATED_BODY()
public:
	virtual bool RequiresTargetSelection() const override { return true; }
	virtual void StartEffect_Implementation() override;
	virtual void HandleActivationPairSelected(
		ASHPlayerState* PairOwner, ASHCard* SelectedCardA, ASHCard* SelectedCardB) override;
protected:
	virtual void ResolveAbility() override;
private:
	UPROPERTY() TObjectPtr<ASHPlayerState> SelectedPairOwner;
	UPROPERTY() TObjectPtr<ASHCard> SelectedPairCardA;
	UPROPERTY() TObjectPtr<ASHCard> SelectedPairCardB;
};
