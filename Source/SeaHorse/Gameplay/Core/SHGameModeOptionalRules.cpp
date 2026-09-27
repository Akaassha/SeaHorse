#include "Gameplay/Core/SHGameMode.h"
#include "Gameplay/Core/SHGameState.h"
#include "Gameplay/Core/SHPlayerState.h"
#include "Gameplay/Cards/CardDefinition.h"
#include "Gameplay/Cards/Fragments/CardEffectFragment.h"
#include "Gameplay/Cards/SHCard.h"
#include "Gameplay/Components/TurnComponent.h"
#include "Gameplay/SHHand.h"
#include "EngineUtils.h"

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
	const auto* PairRule = Cast<UImmediateVictoryPairFragment>(UCardDefinition::FindFragmentByClass(
		Card->GetCardDefinition(), UImmediateVictoryPairFragment::StaticClass()));
	if (!PairRule || PairRule->AllowedPartners.IsEmpty()) { return false; }

	// Look at authoritative definitions, including hidden cards in human and BN hands.
	// A victory card and a removed card no longer provide a possible partner.
	for (TActorIterator<ASHCard> It(GetWorld()); It; ++It)
	{
		if (!IsValid(*It) || It->IsActorBeingDestroyed()) { continue; }
		const ECardZone Zone = It->GetCardZone();
		if (Zone != ECardZone::Deck && Zone != ECardZone::Hand && Zone != ECardZone::Activation) { continue; }
		if (PairRule->AllowedPartners.Contains(FSoftClassPath(It->GetCardDefinition().Get()))) { return false; }
	}
	return true;
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
