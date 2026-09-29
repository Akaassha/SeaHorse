#pragma once

#include "CoreMinimal.h"
#include "Gameplay/Cards/Tasks/ExtendedCardEffectTasks.h"
#include "ChooseDrawSourceEffectTask.generated.h"

UCLASS()
class SEAHORSE_API UChooseDrawSourceEffectTask : public UResolvedCardEffectTask
{
	GENERATED_BODY()

public:
	virtual bool RequiresTargetSelection() const override { return true; }
	virtual void StartEffect_Implementation() override;
	virtual void HandlePlayerSelected(ASHPlayerState* SelectedPlayer) override;
	virtual void HandleParticipantSelected(ASHHand* SelectedHand) override;
protected:
	virtual void ResolveAbility() override;
private:
	UPROPERTY()
	TObjectPtr<ASHPlayerState> DrawingPlayer;
	UPROPERTY()
	TObjectPtr<ASHHand> SelectedSourceHand;
};
