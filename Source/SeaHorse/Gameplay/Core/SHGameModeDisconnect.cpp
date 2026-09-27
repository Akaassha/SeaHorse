#include "Gameplay/Core/SHGameMode.h"
#include "Gameplay/Core/SHGameState.h"
#include "Gameplay/Core/SHPlayerState.h"
#include "Gameplay/Core/SHPlayerController.h"
#include "Gameplay/Cards/SHCard.h"
#include "Gameplay/Cards/CardDefinition.h"
#include "Gameplay/Cards/Fragments/CardEffectFragment.h"
#include "Gameplay/Cards/Tasks/CardEffectTask.h"
#include "Gameplay/SHHand.h"
#include "TimerManager.h"

void ASHGameMode::ConvertDisconnectedPlayerToNPC(ASHPlayerState* Player)
{
	ASHHand* Hand = Player->GetHand();
	check(HasAuthority() && IsValid(Hand));
	// Stop delayed resolutions before moving their cards out of the activation zone.
	const auto Tasks = ActiveEffectTasks;
	for (UCardEffectTask* Task : Tasks)
	{
		if (!IsValid(Task) || Task->GetActivatingPlayer() != Player) { continue; }
		Task->AbandonEffect();
		GetWorldTimerManager().ClearAllTimersForObject(Task);
		SetPairTargetSelectionPresentation(Task, Player, false);
		ActiveEffectTasks.Remove(Task);
	}
	PendingPlayerSelections.Remove(Player);
	PendingParticipantSelections.Remove(Player);
	PendingPairSelections.Remove(Player);
	PendingHandCardSelections.Remove(Player);
	ActiveTargetPresentations.Remove(Player);
	PendingPairActivations.RemoveAll([Player](const FPendingPairActivation& Entry) { return Entry.ActivatingPlayer == Player; });
	PendingSuccessfulActivations.RemoveAll([Player](const FSuccessfulActivation& Entry) { return Entry.Activation.ActivatingPlayer == Player; });
	CompletedEffectPairsWaitingForPresentation.RemoveAll([Player](const FCompletedEffectPair& Entry) { return Entry.ActivatingPlayer == Player; });
	for (auto It = RepeatedPairEffects.CreateIterator(); It; ++It)
	{
		if (!It.Key().IsValid() || It.Key()->GetOwningHand() == Hand) { It.RemoveCurrent(); }
	}
	for (auto It = PairCaptureRecipients.CreateIterator(); It; ++It)
	{
		if (!It.Key().IsValid() || It.Key()->GetOwningHand() == Hand || It.Value() == Player) { It.RemoveCurrent(); }
	}

	GetGameState<ASHGameState>()->RemovePlayerState(Player);
	Player->SetHand(nullptr);
	Player->SetProtectedFromCardEffects(false);
	Hand->SetOwner(nullptr);
	// Keep the logical hand and its seat. Already scored victory cards stay scored.
	const TArray<FActivatedPair> Pairs = Hand->GetLogicalActivationPairs();
	for (const FActivatedPair& Pair : Pairs)
	{
		Hand->RemoveActivationPair(Pair.CardA, Pair.CardB);
		for (ASHCard* Card : {Pair.CardA.Get(), Pair.CardB.Get()})
		{
			if (IsValid(Card)) { Hand->AddCard(Card, Hand->GetCardCount()); }
		}
	}
	Hand->SetIsNPC(true);
	for (ASHCard* Card : Hand->GetCards())
	{
		if (IsValid(Card)) { Card->ConcealInNPCStack(); }
	}
	Hand->ShuffleStack();
	UE_LOG(LogTemp, Log, TEXT("[SH_DISCONNECT] Seat %d converted to BN with %d cards"), Hand->GetLayoutSeatIndex(), Hand->GetCardCount());
}

void ASHGameMode::RefreshSelectionsAfterPlayerDisconnected(ASHPlayerState* Player, ASHHand* ConvertedHand)
{
	TArray<UCardEffectTask*> TasksWithoutTargets;
	for (auto It = PendingPlayerSelections.CreateIterator(); It; ++It)
	{
		if (It.Value().Candidates.Remove(Player) == 0) { continue; }
		if (It.Value().Candidates.IsEmpty()) { TasksWithoutTargets.AddUnique(It.Value().Task); continue; }
		if (ASHPlayerController* PC = Cast<ASHPlayerController>(It.Key()->GetOwner()))
		{
			TArray<ASHPlayerState*> Candidates;
			for (ASHPlayerState* Candidate : It.Value().Candidates) { Candidates.Add(Candidate); }
			PC->ClientRequestPlayerSelection(Candidates, It.Value().Purpose);
		}
	}
	for (auto It = PendingPairSelections.CreateIterator(); It; ++It)
	{
		const int32 Removed = It.Value().CandidateCards.RemoveAll([ConvertedHand](ASHCard* Card)
		{
			return !IsValid(Card) || Card->GetOwningHand() == ConvertedHand || Card->GetCardZone() != ECardZone::Activation;
		});
		if (!Removed) { continue; }
		if (It.Value().CandidateCards.IsEmpty()) { TasksWithoutTargets.AddUnique(It.Value().Task); continue; }
		if (ASHPlayerController* PC = Cast<ASHPlayerController>(It.Key()->GetOwner()))
		{
			TArray<ASHCard*> Candidates;
			for (ASHCard* Card : It.Value().CandidateCards) { Candidates.Add(Card); }
			PC->ClientRequestActivationPairSelection(Candidates);
		}
	}
	for (auto It = PendingHandCardSelections.CreateIterator(); It; ++It)
	{
		FPendingPairSelection& Selection = It.Value();
		if (Selection.SourceHand != ConvertedHand) { continue; }
		// Kurt may already be choosing a draw from this hand. It is now a stack,
		// so only its newly shuffled top card remains a legal choice.
		Selection.CandidateCards.Reset();
		if (ASHCard* Top = ConvertedHand->GetTopCard()) { Selection.CandidateCards.Add(Top); }
		if (Selection.CandidateCards.Num() < Selection.MinCards) { TasksWithoutTargets.AddUnique(Selection.Task); continue; }
		if (ASHPlayerController* PC = Cast<ASHPlayerController>(It.Key()->GetOwner()))
		{
			const auto* Fragment = Cast<UCardEffectFragment>(UCardDefinition::FindFragmentByClass(
				Selection.Task->GetCardA()->GetCardDefinition(), UCardEffectFragment::StaticClass()));
			PC->ClientRequestHandCardsSelectionAfterShuffle(ConvertedHand, ConvertedHand->GetCards(),
				{ConvertedHand->GetTopCard()}, Selection.MinCards, Selection.MaxCards,
				Fragment ? Fragment->SelectionWidgetClass : nullptr);
		}
	}
	for (UCardEffectTask* Task : TasksWithoutTargets)
	{
		if (IsValid(Task) && ActiveEffectTasks.Contains(Task)) { Task->FinishEffect(); }
	}
}
