#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "SeaHorse/Gameplay/Core/SHGameState.h"
#include "TurnComponent.generated.h"

class ASHPlayerState;
struct FActivatedPair;

UENUM(BlueprintType)
enum class ETurnPhaseEndReason : uint8
{
	None,
	AutoSkipped,
	PlayerSkipped,
	PairCreated,
	CardDrawn
};

UCLASS(BlueprintType, Blueprintable, ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class SEAHORSE_API UTurnComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UTurnComponent();

	void InitializeTurns(ASHPlayerState* StartingPlayer);
	void CompleteCurrentPhase(ETurnPhaseEndReason Reason);
	void SkipCurrentPhase(ASHPlayerState* RequestingPlayer);

	bool IsPairingActionUsed() const { return bPairingActionUsed; }
	void MarkPairingActionUsed() { bPairingActionUsed = true; }

	bool CanActivatePair(ASHPlayerState* RequestingPlayer, const FActivatedPair& ActivatedPair) const;

protected:
	UFUNCTION(BlueprintNativeEvent, Category = "Turn")
	ETurnPhase GetNextTurnPhase(ETurnPhase CurrentPhase, ETurnPhaseEndReason Reason) const;
	virtual ETurnPhase GetNextTurnPhase_Implementation(ETurnPhase CurrentPhase, ETurnPhaseEndReason Reason) const;

	UFUNCTION(BlueprintNativeEvent, Category = "Turn")
	ASHPlayerState* ChooseNextPlayer(ASHPlayerState* CurrentPlayer) const;
	virtual ASHPlayerState* ChooseNextPlayer_Implementation(ASHPlayerState* CurrentPlayer) const;

private:
	void EndTurn();
	void EnterTurnPhase(ETurnPhase NewPhase);
	ASHGameState* GetSHGameState() const;
	void CheckServerAuthority() const;

	bool bPairingActionUsed = false;
};
