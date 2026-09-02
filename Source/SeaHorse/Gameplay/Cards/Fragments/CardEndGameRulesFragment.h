#pragma once

#include "CoreMinimal.h"
#include "SeaHorse/Gameplay/Cards/Fragments/CardFragment.h"
#include "CardEndGameRulesFragment.generated.h"

UCLASS(BlueprintType, EditInlineNew)
class SEAHORSE_API UCardEndGameRulesFragment : public UCardFragment
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "End Game")
	bool bOwnerAutomaticallyLoses = false;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "End Game", meta = (ClampMin = "0"))
	int32 BonusVictoryPointsPerPairInActivationZone = 0;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "End Game")
	bool bWinsScoreTies = false;
};
