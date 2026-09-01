#include "SeaHorse/Gameplay/Components/TurnComponent.h"

#include "SeaHorse/Gameplay/Cards/CardDefinition.h"
#include "SeaHorse/Gameplay/Cards/Fragments/CardActivationRulesFragment.h"
#include "SeaHorse/Gameplay/Cards/SHCard.h"
#include "SeaHorse/Gameplay/Core/SHPlayerState.h"
#include "SeaHorse/Gameplay/Core/SHGameMode.h"
#include "SeaHorse/Gameplay/SHHand.h"

UTurnComponent::UTurnComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UTurnComponent::InitializeTurns(ASHPlayerState* StartingPlayer)
{
	CheckServerAuthority();

	checkf(IsValid(StartingPlayer), TEXT("Cannot initialize turns without a starting player"));

	ASHGameState* GameState = GetSHGameState();
	GameState->SetCurrentPlayer(StartingPlayer);
	bPairingActionUsed = false;
	GameState->SetTurnPhase(ETurnPhase::FirstPairing);
}

void UTurnComponent::CompleteCurrentPhase(ETurnPhaseEndReason Reason)
{
	CheckServerAuthority();

	ASHGameState* GameState = GetSHGameState();
	const ETurnPhase NextPhase = GetNextTurnPhase(GameState->GetTurnPhase(), Reason);

	if (NextPhase == ETurnPhase::None)
	{
		EndTurn();
		return;
	}

	EnterTurnPhase(NextPhase);
}

void UTurnComponent::SkipCurrentPhase(ASHPlayerState* RequestingPlayer)
{
	CheckServerAuthority();

	ASHGameState* GameState = GetSHGameState();
	if (!IsValid(RequestingPlayer) || GameState->GetCurrentPlayer() != RequestingPlayer)
	{
		return;
	}

	const ETurnPhase CurrentPhase = GameState->GetTurnPhase();
	if (CurrentPhase != ETurnPhase::FirstPairing && CurrentPhase != ETurnPhase::SecondPairing)
	{
		return;
	}

	CompleteCurrentPhase(ETurnPhaseEndReason::PlayerSkipped);
}

bool UTurnComponent::CanActivatePair(ASHPlayerState* RequestingPlayer, const FActivatedPair& ActivatedPair) const
{
	if (!IsValid(RequestingPlayer) || !IsValid(ActivatedPair.CardA) ||
		!IsValid(ActivatedPair.CardB) || ActivatedPair.bActivated)
	{
		return false;
	}

	const ASHGameState* GameState = GetSHGameState();
	if (GameState->IsGameEnded())
	{
		return false;
	}

	const bool bIsOwnTurn = GameState->CurrentPlayer == RequestingPlayer;
	const UCardActivationRulesFragment* Rules = Cast<UCardActivationRulesFragment>(
		UCardDefinition::FindFragmentByClass(
			ActivatedPair.CardA->CardDefinition,
			UCardActivationRulesFragment::StaticClass()));

	if (!IsValid(Rules))
	{
		return bIsOwnTurn;
	}

	if (!Rules->bCanBeActivated)
	{
		return false;
	}

	switch (Rules->TurnRestriction)
	{
	case ECardActivationRules::OwnTurn:
		if (!bIsOwnTurn)
		{
			return false;
		}
		break;

	case ECardActivationRules::OutsideOwnTurn:
		if (bIsOwnTurn)
		{
			return false;
		}
		break;

	case ECardActivationRules::AnyTurn:
		break;
	}

	return Rules->AllowedPhases.IsEmpty() || Rules->AllowedPhases.Contains(GameState->GetTurnPhase());
}

ETurnPhase UTurnComponent::GetNextTurnPhase_Implementation(
	ETurnPhase CurrentPhase,
	ETurnPhaseEndReason Reason) const
{
	switch (CurrentPhase)
	{
	case ETurnPhase::FirstPairing:
		return Reason == ETurnPhaseEndReason::CardDrawn
			? ETurnPhase::SecondPairing
			: ETurnPhase::DrawCard;

	case ETurnPhase::DrawCard:
		return ETurnPhase::SecondPairing;

	case ETurnPhase::SecondPairing:
	case ETurnPhase::None:
	default:
		return ETurnPhase::None;
	}
}

ASHPlayerState* UTurnComponent::ChooseNextPlayer_Implementation(ASHPlayerState* CurrentPlayer) const
{
	checkf(IsValid(CurrentPlayer), TEXT("Invalid current player"));

	ASHGameState* GameState = GetSHGameState();
	const int32 PlayerCount = GameState->PlayerArray.Num();
	checkf(PlayerCount > 0, TEXT("Cannot choose the next player without players"));

	const int32 NextSeatIndex = (CurrentPlayer->GetSeatIndex() + 1) % PlayerCount;
	for (APlayerState* PlayerState : GameState->PlayerArray)
	{
		ASHPlayerState* SHPlayerState = Cast<ASHPlayerState>(PlayerState);
		if (IsValid(SHPlayerState) && SHPlayerState->GetSeatIndex() == NextSeatIndex)
		{
			return SHPlayerState;
		}
	}

	return nullptr;
}

void UTurnComponent::EndTurn()
{
	ASHGameMode* GameMode = GetWorld()->GetAuthGameMode<ASHGameMode>();
	if (IsValid(GameMode) && GameMode->TryFinishGame())
	{
		return;
	}

	ASHGameState* GameState = GetSHGameState();
	ASHPlayerState* CurrentPlayer = GameState->GetCurrentPlayer();
	checkf(IsValid(CurrentPlayer), TEXT("Cannot end a turn without a current player"));

	ASHPlayerState* NextPlayer = ChooseNextPlayer(CurrentPlayer);
	checkf(IsValid(NextPlayer), TEXT("ChooseNextPlayer returned an invalid player"));

	GameState->SetCurrentPlayer(NextPlayer);
	bPairingActionUsed = false;
	GameState->SetTurnPhase(ETurnPhase::FirstPairing);
}

void UTurnComponent::EnterTurnPhase(ETurnPhase NewPhase)
{
	if (NewPhase == ETurnPhase::FirstPairing || NewPhase == ETurnPhase::SecondPairing)
	{
		bPairingActionUsed = false;
	}

	GetSHGameState()->SetTurnPhase(NewPhase);
}

ASHGameState* UTurnComponent::GetSHGameState() const
{
	ASHGameState* GameState = GetWorld()->GetGameState<ASHGameState>();
	checkf(IsValid(GameState), TEXT("Invalid SHGameState"));
	return GameState;
}

void UTurnComponent::CheckServerAuthority() const
{
	checkf(GetOwner() && GetOwner()->HasAuthority(), TEXT("Turns can only be controlled on the server"));
}
