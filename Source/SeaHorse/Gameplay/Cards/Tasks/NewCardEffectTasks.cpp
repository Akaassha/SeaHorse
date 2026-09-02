#include "SeaHorse/Gameplay/Cards/Tasks/NewCardEffectTasks.h"

#include "SeaHorse/Gameplay/Cards/CardDefinition.h"
#include "SeaHorse/Gameplay/Cards/Fragments/CardEffectFragment.h"
#include "SeaHorse/Gameplay/Cards/SHCard.h"
#include "SeaHorse/Gameplay/Components/TurnComponent.h"
#include "SeaHorse/Gameplay/Core/SHGameMode.h"
#include "SeaHorse/Gameplay/Core/SHGameState.h"
#include "SeaHorse/Gameplay/Core/SHPlayerState.h"
#include "SeaHorse/Gameplay/SHHand.h"

void URotateHandsLeftEffectTask::StartEffect_Implementation()
{
	ASHGameMode* GameMode = GetTypedOuter<ASHGameMode>();
	checkf(IsValid(GameMode), TEXT("RotateHandsLeftEffectTask has no valid GameMode"));
	GameMode->PassHandsToLeft();
	FinishEffect();
}

void USkipSelectedPlayerTurnEffectTask::StartEffect_Implementation()
{
	const ASHGameState* GameState = GetWorld()->GetGameState<ASHGameState>();
	checkf(IsValid(GameState), TEXT("Invalid SHGameState"));

	TArray<ASHPlayerState*> Candidates;
	for (APlayerState* PlayerState : GameState->PlayerArray)
	{
		if (ASHPlayerState* Candidate = Cast<ASHPlayerState>(PlayerState); IsValid(Candidate))
		{
			Candidates.Add(Candidate);
		}
	}

	RequestPlayerSelection(Candidates, EPlayerSelectionPurpose::PlayerToSkipTurn);
}

void USkipSelectedPlayerTurnEffectTask::HandlePlayerSelected(ASHPlayerState* SelectedPlayer)
{
	ASHGameMode* GameMode = GetTypedOuter<ASHGameMode>();
	checkf(IsValid(GameMode), TEXT("SkipSelectedPlayerTurnEffectTask has no valid GameMode"));
	UTurnComponent* TurnComponent = GameMode->GetTurnComponent();
	checkf(IsValid(TurnComponent), TEXT("GameMode has no TurnComponent"));

	TurnComponent->ScheduleSkippedTurn(SelectedPlayer);
	FinishEffect();
}

void UCollectAllActivationPairsEffectTask::StartEffect_Implementation()
{
	ASHGameMode* GameMode = GetTypedOuter<ASHGameMode>();
	checkf(IsValid(GameMode), TEXT("CollectAllActivationPairsEffectTask has no valid GameMode"));
	GameMode->MoveAllActivationPairsToVictoryStacks();
	FinishEffect();
}

void UTransferSpecifiedCardEffectTask::StartEffect_Implementation()
{
	const UTransferCardEffectFragment* Fragment = Cast<UTransferCardEffectFragment>(
		UCardDefinition::FindFragmentByClass(
			GetCardA()->GetCardDefinition(),
			UTransferCardEffectFragment::StaticClass()));

	if (!IsValid(Fragment) || !Fragment->CardDefinitionToTransfer)
	{
		FinishEffect();
		return;
	}

	ASHHand* ActivatingHand = IsValid(GetActivatingPlayer())
		? GetActivatingPlayer()->GetHand()
		: nullptr;
	const bool bHasCardToTransfer = IsValid(ActivatingHand) &&
		ActivatingHand->GetCards().ContainsByPredicate(
			[Fragment](ASHCard* Card)
			{
				return IsValid(Card) &&
					Card->GetCardDefinition() == Fragment->CardDefinitionToTransfer;
			});

	if (!bHasCardToTransfer)
	{
		FinishEffect();
		return;
	}

	const ASHGameState* GameState = GetWorld()->GetGameState<ASHGameState>();
	checkf(IsValid(GameState), TEXT("Invalid SHGameState"));

	TArray<ASHPlayerState*> Candidates;
	for (APlayerState* PlayerState : GameState->PlayerArray)
	{
		ASHPlayerState* Candidate = Cast<ASHPlayerState>(PlayerState);
		if (IsValid(Candidate) && Candidate != GetActivatingPlayer())
		{
			Candidates.Add(Candidate);
		}
	}

	if (Candidates.IsEmpty())
	{
		FinishEffect();
		return;
	}

	RequestPlayerSelection(Candidates, EPlayerSelectionPurpose::CardTransferRecipient);
}

void UTransferSpecifiedCardEffectTask::HandlePlayerSelected(ASHPlayerState* SelectedPlayer)
{
	const UTransferCardEffectFragment* Fragment = Cast<UTransferCardEffectFragment>(
		UCardDefinition::FindFragmentByClass(
			GetCardA()->GetCardDefinition(),
			UTransferCardEffectFragment::StaticClass()));

	ASHGameMode* GameMode = GetTypedOuter<ASHGameMode>();
	if (IsValid(GameMode) && IsValid(Fragment))
	{
		GameMode->TransferCardToPlayer(
			GetActivatingPlayer(),
			SelectedPlayer,
			Fragment->CardDefinitionToTransfer);
	}

	FinishEffect();
}

void UCollectSelectedActivationPairEffectTask::StartEffect_Implementation()
{
	const ASHGameState* GameState = GetWorld()->GetGameState<ASHGameState>();
	checkf(IsValid(GameState), TEXT("Invalid SHGameState"));

	TArray<ASHCard*> CandidateCards;
	for (APlayerState* PlayerState : GameState->PlayerArray)
	{
		const ASHPlayerState* PairOwner = Cast<ASHPlayerState>(PlayerState);
		const ASHHand* Hand = IsValid(PairOwner) ? PairOwner->GetHand() : nullptr;
		if (!IsValid(Hand))
		{
			continue;
		}

		for (const FActivatedPair& Pair : Hand->GetLogicalActivationPairs())
		{
			// Olga's own pair is collected normally when this effect finishes.
			if (Pair.CardA == GetCardA() || Pair.CardB == GetCardA())
			{
				continue;
			}

			if (IsValid(Pair.CardA) && IsValid(Pair.CardB))
			{
				CandidateCards.Add(Pair.CardA);
				CandidateCards.Add(Pair.CardB);
			}
		}
	}

	if (CandidateCards.IsEmpty() || !RequestActivationPairSelection(CandidateCards))
	{
		FinishEffect();
	}
}

void UCollectSelectedActivationPairEffectTask::HandleActivationPairSelected(
	ASHPlayerState* PairOwner, ASHCard* SelectedCardA, ASHCard* SelectedCardB)
{
	ASHGameMode* GameMode = GetTypedOuter<ASHGameMode>();
	if (IsValid(GameMode) && IsValid(PairOwner))
	{
		// MovePairToVictoryStack deliberately does not run the selected pair's effect.
		GameMode->MovePairToVictoryStack(PairOwner, SelectedCardA, SelectedCardB);
	}

	FinishEffect();
}
