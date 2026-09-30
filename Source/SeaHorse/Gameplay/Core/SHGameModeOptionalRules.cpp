#include "Gameplay/Core/SHGameMode.h"
#include "Gameplay/Core/SHGameState.h"
#include "Gameplay/Core/SHPlayerState.h"
#include "Gameplay/Cards/CardDefinition.h"
#include "Gameplay/Cards/Fragments/CardEffectFragment.h"
#include "Gameplay/Cards/SHCard.h"
#include "Gameplay/Components/TurnComponent.h"
#include "Gameplay/SHHand.h"
#include "EngineUtils.h"

namespace
{
bool HasRemainingRatfolkPartner(const UWorld* World, ASHCard* Card)
{
	const auto* PairRule = Cast<UImmediateVictoryPairFragment>(UCardDefinition::FindFragmentByClass(
		Card->GetCardDefinition(), UImmediateVictoryPairFragment::StaticClass()));
	if (!PairRule || PairRule->AllowedPartners.IsEmpty()) { return true; }
	// Activation includes paired Paulus cards: they must leave the zone first.
	for (TActorIterator<ASHCard> It(World); It; ++It)
	{
		if (!IsValid(*It) || It->IsActorBeingDestroyed()) { continue; }
		const ECardZone Zone = It->GetCardZone();
		if (Zone != ECardZone::Deck && Zone != ECardZone::Hand && Zone != ECardZone::Activation) { continue; }
		if (PairRule->AllowedPartners.Contains(FSoftClassPath(It->GetCardDefinition().Get()))) { return true; }
	}
	return false;
}
}

bool ASHGameMode::CanRemoveOrphanedRatfolk(ASHPlayerState* Player, ASHCard* Card) const
{
	const ASHGameState* State = GetGameState<ASHGameState>();
	ASHHand* Hand = IsValid(Player) ? Player->GetHand() : nullptr;
	if (!HasAuthority() || !IsValid(State) || State->IsGameEnded() ||
		!State->GetOptionalRules().bAllowOrphanedRatfolkRemoval ||
		!IsValid(Player) || Player->IsInactive() || !State->PlayerArray.Contains(Player) ||
		State->GetCurrentPlayer() != Player || !IsValid(Hand) ||
		!IsValid(Card) || Card->GetOwningHand() != Hand || Card->GetCardZone() != ECardZone::Hand || !Hand->ContainsCard(Card) ||
		IsWaitingForPlayerSelection() || HasActiveEffectTasks() ||
		!IsValid(TurnComponent) || TurnComponent->HasUnsettledPairs() || TurnComponent->HasNamedTurnTransitionBlocks())
	{
		return false;
	}
	if (State->GetTurnPhase() != ETurnPhase::FirstPairing && State->GetTurnPhase() != ETurnPhase::SecondPairing) { return false; }
	return !HasRemainingRatfolkPartner(GetWorld(), Card);
}

void ASHGameMode::RemoveOrphanedRatfolkAutomatically()
{
	const ASHGameState* State = GetGameState<ASHGameState>();
	if (!HasAuthority() || !IsValid(State) || State->IsGameEnded() ||
		!State->GetOptionalRules().bAllowOrphanedRatfolkRemoval || bRemovingOrphanedRatfolk ||
		bProcessingPairActivations || IsWaitingForPlayerSelection() || HasActiveEffectTasks() ||
		!CompletedEffectPairsWaitingForPresentation.IsEmpty() ||
		!IsValid(TurnComponent) || TurnComponent->HasUnsettledPairs() || TurnComponent->HasNamedTurnTransitionBlocks())
	{
		return;
	}
	TGuardValue<bool> Guard(bRemovingOrphanedRatfolk, true);
	// The deck is fully dealt at match start. Only unpaired cards in logical hands
	// are removed; already scored Ratfolk must retain their victory points.
	TArray<ASHCard*> Candidates;
	for (TActorIterator<ASHCard> It(GetWorld()); It; ++It)
	{
		ASHCard* Card = *It;
		ASHHand* Hand = IsValid(Card) ? Card->GetOwningHand() : nullptr;
		if (IsValid(Hand) && Card->GetCardZone() == ECardZone::Hand && Hand->ContainsCard(Card) &&
			!HasRemainingRatfolkPartner(GetWorld(), Card))
		{
			Candidates.Add(Card);
		}
	}
	for (ASHCard* Card : Candidates)
	{
		if (!IsValid(Card)) { continue; }
		ASHHand* Hand = Card->GetOwningHand();
		if (!IsValid(Hand) || !Hand->ContainsCard(Card)) { continue; }
		Hand->RemoveCard(Card);
		Card->SetCardZone(ECardZone::None);
		Card->Destroy();
	}
}

bool ASHGameMode::RequestRemoveOrphanedRatfolk(ASHPlayerState* Player, ASHCard* Card)
{
	if (!CanRemoveOrphanedRatfolk(Player, Card)) { return false; }
	Player->GetHand()->RemoveCard(Card);
	Card->SetCardZone(ECardZone::None);
	Card->Destroy();
	// This optional discard does not use a pairing action or run an activation.
	TryFinishGame();
	return true;
}
