#pragma once

#include "CoreMinimal.h"
#include "SeaHorse/Gameplay/Cards/Tasks/CardEffectTask.h"
#include "SeaHorse/Gameplay/Components/TurnComponent.h"
#include "AdditionalDrawEffectTask.generated.h"

UCLASS(Abstract)
class SEAHORSE_API UAdditionalDrawEffectTask : public UCardEffectTask
{
	GENERATED_BODY()

public:
	virtual void StartEffect_Implementation() override;
	virtual bool BlocksNextPairActivation() const override { return false; }
	virtual void RecordAdditionalDrawnCard(ASHCard* Card) override { ++CompletedAdditionalDraws; }
	virtual bool WasEffectSuccessful_Implementation() const override
	{ return CompletedAdditionalDraws >= GetAdditionalDrawCount(); }

protected:
	EAdditionalDrawSourceRule SourceRule = EAdditionalDrawSourceRule::SamePlayer;
	int32 CompletedAdditionalDraws = 0;
};

UCLASS()
class SEAHORSE_API UDrawAgainFromSamePlayerEffectTask : public UAdditionalDrawEffectTask
{
	GENERATED_BODY()

public:
	UDrawAgainFromSamePlayerEffectTask();
};

UCLASS()
class SEAHORSE_API UDrawAgainFromDifferentPlayerEffectTask : public UAdditionalDrawEffectTask
{
	GENERATED_BODY()

public:
	UDrawAgainFromDifferentPlayerEffectTask();
};
