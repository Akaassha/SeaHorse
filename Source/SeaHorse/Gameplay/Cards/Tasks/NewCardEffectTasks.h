#pragma once

#include "CoreMinimal.h"
#include "SeaHorse/Gameplay/Cards/Tasks/CardEffectTask.h"
#include "NewCardEffectTasks.generated.h"

UCLASS()
class SEAHORSE_API URotateHandsLeftEffectTask : public UCardEffectTask
{
	GENERATED_BODY()
public:
	virtual void StartEffect_Implementation() override;
};

UCLASS()
class SEAHORSE_API USkipSelectedPlayerTurnEffectTask : public UCardEffectTask
{
	GENERATED_BODY()
public:
	virtual void StartEffect_Implementation() override;
	virtual void HandlePlayerSelected(ASHPlayerState* SelectedPlayer) override;
};

UCLASS()
class SEAHORSE_API UCollectAllActivationPairsEffectTask : public UCardEffectTask
{
	GENERATED_BODY()
public:
	virtual void StartEffect_Implementation() override;
};

UCLASS()
class SEAHORSE_API UTransferSpecifiedCardEffectTask : public UCardEffectTask
{
	GENERATED_BODY()
public:
	virtual void StartEffect_Implementation() override;
	virtual void HandlePlayerSelected(ASHPlayerState* SelectedPlayer) override;
};

UCLASS()
class SEAHORSE_API UCollectSelectedActivationPairEffectTask : public UCardEffectTask
{
	GENERATED_BODY()
public:
	virtual void StartEffect_Implementation() override;
	virtual void HandleActivationPairSelected(
		ASHPlayerState* PairOwner, ASHCard* SelectedCardA, ASHCard* SelectedCardB) override;
};
