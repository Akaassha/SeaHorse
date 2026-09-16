#include "Gameplay/Cards/Tasks/ExpansionCardEffectTasks.h"
#include "Gameplay/Cards/SHCard.h"
#include "Gameplay/Core/SHGameMode.h"
#include "Gameplay/Core/SHGameState.h"
#include "Gameplay/Core/SHPlayerState.h"
#include "Gameplay/SHHand.h"

void UExchangeHandCardsEffectTask::StartEffect_Implementation()
{
	ASHHand* Hand = GetActivatingPlayer()->GetHand();
	if (!GetTypedOuter<ASHGameMode>()->RequestHandCardsSelection(this, GetActivatingPlayer(), Hand, Hand->GetCards(), 1, 3)) { FinishEffect(); }
}

void UExchangeHandCardsEffectTask::HandleHandCardsSelected(const TArray<ASHCard*>& Cards)
{
	ASHHand* Hand = GetActivatingPlayer()->GetHand();
	if (bTransferred)
	{
		if (Cards.Num() != 1 || !IsValid(Recipient) || !Recipient->ContainsCard(Cards[0])) { return; }
		ASHCard* Card = Cards[0];
		Recipient->RemoveCard(Card);
		Hand->AddCard(Card, Hand->GetCardCount());
		--RemainingDraws;
		RequestNextDraw();
		return;
	}
	OfferedCards.Reset();
	for (ASHCard* Card : Cards) { OfferedCards.Add(Card); }
	TArray<ASHHand*> Candidates;
	for (ASHHand* Other : GetWorld()->GetGameState<ASHGameState>()->GetParticipantHands())
	{
		if (IsValid(Other) && Other != Hand && !Other->IsProtectedFromCardEffects()) { Candidates.Add(Other); }
	}
	if (Candidates.IsEmpty()) { FinishEffect(); return; }
	RequestParticipantSelection(Candidates, EPlayerSelectionPurpose::CardTransferRecipient);
}

void UExchangeHandCardsEffectTask::HandleParticipantSelected(ASHHand* Hand)
{
	Recipient = Hand;
	PlayActivationVFX();
	ResolveAfterPresentation();
}

void UExchangeHandCardsEffectTask::ResolveAbility()
{
	ASHHand* Hand = GetActivatingPlayer()->GetHand();
	if (!IsValid(Recipient) || Recipient == Hand || Recipient->IsProtectedFromCardEffects() || OfferedCards.IsEmpty()) { FinishEffect(); return; }
	for (ASHCard* Card : OfferedCards) { if (!IsValid(Card) || !Hand->ContainsCard(Card)) { FinishEffect(); return; } }
	RemainingDraws = OfferedCards.Num();
	for (ASHCard* Card : OfferedCards)
	{
		Hand->RemoveCard(Card);
		Recipient->AddCard(Card, Recipient->GetCardCount());
	}
	Recipient->ShuffleCards();
	bTransferred = true;
	RequestNextDraw();
}

void UExchangeHandCardsEffectTask::RequestNextDraw()
{
	if (RemainingDraws <= 0) { FinishEffect(); return; }
	TArray<ASHCard*> Candidates = Recipient->IsLogicalNPC() ? TArray<ASHCard*>{Recipient->GetTopCard()} : Recipient->GetCards();
	if (!GetTypedOuter<ASHGameMode>()->RequestHandCardsSelection(this, GetActivatingPlayer(), Recipient, Candidates, 1, 1)) { FinishEffect(); }
}

void UProtectUntilNextTurnEffectTask::ResolveAbility()
{
	GetActivatingPlayer()->SetProtectedFromCardEffects(true);
	FinishEffect();
}
