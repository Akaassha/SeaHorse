#include "Gameplay/Core/SHPlayerController.h"
#include "Gameplay/Cards/SHCard.h"
#include "Gameplay/Components/CardsLayoutComponent.h"
#include "Gameplay/SHHand.h"
#include "Gameplay/Presentation/CardSelectionPrompt.h"
#include "Engine/World.h"
#include "TimerManager.h"

void ASHPlayerController::ClearShuffledHandSelectionWait()
{
	GetWorldTimerManager().ClearTimer(ShuffleSelectionWaitTimer);
	WaitingForShuffleHand = nullptr;
	WaitingForShuffleOrder.Reset();
	WaitingForShuffleCandidates.Reset();
	WaitingForShuffleWidget = nullptr;
	ShuffleLayoutWaitStartedAt = -1.0;
}

void ASHPlayerController::ClientRequestHandCardsSelectionAfterShuffle_Implementation(
	ASHHand* SourceHand, const TArray<ASHCard*>& ShuffledOrder, const TArray<ASHCard*>& Candidates,
	int32 Min, int32 Max, TSubclassOf<UCardSelectionPrompt> WidgetClass)
{
	ClearLocalEffectSelectionState();
	if (!IsValid(SourceHand) || ShuffledOrder.IsEmpty()) { return; }
	WaitingForShuffleHand = SourceHand;
	for (ASHCard* Card : ShuffledOrder) { WaitingForShuffleOrder.Add(Card); }
	for (ASHCard* Card : Candidates) { WaitingForShuffleCandidates.Add(Card); }
	WaitingForShuffleWidget = WidgetClass;
	WaitingForShuffleMin = Min; WaitingForShuffleMax = Max;
	// Suppress world gestures while the owner RPC has arrived but the source
	// actor's replicated Cards/Owner data may still describe its previous order.
	bAwaitingPlayerSelectionResponse = true;
	GetWorldTimerManager().SetTimer(ShuffleSelectionWaitTimer, this,
		&ASHPlayerController::PollShuffledHandSelection, 0.02f, true);
	PollShuffledHandSelection();
}

void ASHPlayerController::PollShuffledHandSelection()
{
	ASHHand* Source = WaitingForShuffleHand;
	if (!Source) { return; }
	if (!IsValid(Source)) { ClearLocalEffectSelectionState(); return; }
	const TArray<ASHCard*> Actual = Source->GetCards();
	if (Actual.Num() != WaitingForShuffleOrder.Num()) { return; }
	for (int32 Index = 0; Index < Actual.Num(); ++Index)
	{
		if (!IsValid(Actual[Index]) || Actual[Index] != WaitingForShuffleOrder[Index] ||
			Actual[Index]->GetOwningHand() != Source || Actual[Index]->GetCardZone() != ECardZone::Hand) { return; }
	}
	// Replication is authoritative. The timeout below applies only to optional
	// local movement, never to missing or mismatched replicated state.
	if (ShuffleLayoutWaitStartedAt < 0.0) { ShuffleLayoutWaitStartedAt = GetWorld()->GetTimeSeconds(); }
	if (ASHHand* VisualHand = FindVisualHandForLogicalHand(Source))
	{
		VisualHand->UpdateCardPositions();
		const auto* Layout = VisualHand->FindComponentByClass<USHHandCardsLayoutComponent>();
		if (IsValid(Layout) && !Layout->AreCardsAtLayoutPositions(Actual) &&
			GetWorld()->GetTimeSeconds() - ShuffleLayoutWaitStartedAt < 3.0) { return; }
	}
	TArray<ASHCard*> Candidates;
	for (ASHCard* Card : WaitingForShuffleCandidates) { Candidates.Add(Card); }
	const int32 Min = WaitingForShuffleMin, Max = WaitingForShuffleMax;
	const TSubclassOf<UCardSelectionPrompt> Widget = WaitingForShuffleWidget;
	ClientRequestHandCardsSelection_Implementation(Candidates, Min, Max, Widget);
}
