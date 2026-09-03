#pragma once

#include "CoreMinimal.h"
#include "SeaHorse/Gameplay/Cards/Tasks/CardEffectTask.h"
#include "ChooseDrawSourceEffectTask.generated.h"

UCLASS()
class SEAHORSE_API UChooseDrawSourceEffectTask : public UCardEffectTask
{
	GENERATED_BODY()

public:
	virtual void StartEffect_Implementation() override;
	virtual void HandlePlayerSelected(ASHPlayerState* SelectedPlayer) override;
	virtual void HandleParticipantSelected(ASHHand* SelectedHand) override;

private:
	UPROPERTY()
	TObjectPtr<ASHPlayerState> DrawingPlayer;
};
