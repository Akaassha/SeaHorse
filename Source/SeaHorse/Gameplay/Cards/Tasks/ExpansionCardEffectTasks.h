#pragma once

#include "CoreMinimal.h"
#include "Gameplay/Cards/Tasks/ExtendedCardEffectTasks.h"
#include "ExpansionCardEffectTasks.generated.h"

UCLASS()
class SEAHORSE_API UExchangeHandCardsEffectTask : public UResolvedCardEffectTask
{
	GENERATED_BODY()
public:
	virtual bool RequiresTargetSelection() const override { return true; }
	virtual void StartEffect_Implementation() override;
	virtual void HandleHandCardsSelected(const TArray<ASHCard*>& Cards) override;
	virtual void HandleParticipantSelected(ASHHand* Hand) override;
	virtual ECardEffectPairDisposition GetPairDisposition_Implementation() const override
	{ return bTransferred ? ECardEffectPairDisposition::MoveToVictoryStack : ECardEffectPairDisposition::KeepOnTable; }
protected:
	virtual void ResolveAbility() override;
private:
	void RequestNextDraw();
	UPROPERTY() TArray<TObjectPtr<ASHCard>> OfferedCards;
	UPROPERTY() TObjectPtr<ASHHand> Recipient;
	int32 RemainingDraws = 0;
	bool bTransferred = false;
};

UCLASS()
class SEAHORSE_API UProtectUntilNextTurnEffectTask : public UResolvedCardEffectTask
{
	GENERATED_BODY()
public:
	virtual void StartEffect_Implementation() override { ResolveAfterPresentation(); }
protected:
	virtual void ResolveAbility() override;
};
