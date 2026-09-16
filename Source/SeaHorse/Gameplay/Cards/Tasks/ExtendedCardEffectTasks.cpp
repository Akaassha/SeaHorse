#include "Gameplay/Cards/Tasks/ExtendedCardEffectTasks.h"
#include "Gameplay/Cards/CardDefinition.h"
#include "Gameplay/Cards/Fragments/CardEffectFragment.h"
#include "Gameplay/Cards/SHCard.h"
#include "Gameplay/Core/SHGameMode.h"
#include "Gameplay/Core/SHGameState.h"
#include "Gameplay/Core/SHPlayerState.h"
#include "Gameplay/SHHand.h"
#include "TimerManager.h"

void UResolvedCardEffectTask::ResolveAfterPresentation()
{
	if (IsFinished()) { return; }
	ASHGameMode* Mode = GetTypedOuter<ASHGameMode>();
	if (!IsValid(Mode)) { return; }
	CommitEffect();
	if ((Mode->GetTurnComponent() && Mode->GetTurnComponent()->HasNamedTurnTransitionBlocks()) ||
		(WaitForOtherEffects() && (Mode->HasOtherActiveEffects(this) || (Mode->GetTurnComponent() && Mode->GetTurnComponent()->HasUnsettledPairs()))))
	{
		FTimerHandle Timer;
		Mode->GetWorldTimerManager().SetTimer(Timer, FTimerDelegate::CreateWeakLambda(this, [this]()
		{
			ResolveAfterPresentation();
		}), 0.02f, false);
		return;
	}
	ResolveAbility();
}

void UDrawTwoReturnOneEffectTask::RecordDrawnCard(ASHCard* Card)
{
	if (IsValid(Card)) { DrawnCards.AddUnique(Card); }
}

bool UDrawTwoReturnOneEffectTask::CompleteDrawSequence()
{
	if (bReturned) { return true; }
	TArray<ASHCard*> Candidates;
	ASHHand* Hand = GetActivatingPlayer()->GetHand();
	for (ASHCard* Card : DrawnCards)
	{
		if (IsValid(Card) && Hand->ContainsCard(Card)) { Candidates.Add(Card); }
	}
	if (Candidates.IsEmpty()) { return true; }
	CommitEffect(); // A draw has already changed gameplay; ESC cannot roll it back.
	return !RequestHandCardSelection(Candidates);
}

void UDrawTwoReturnOneEffectTask::HandleHandCardSelected(ASHCard* Card)
{
	ASHGameMode* Mode = GetTypedOuter<ASHGameMode>();
	UTurnComponent* Turns = Mode->GetTurnComponent();
	ASHHand* Hand = GetActivatingPlayer()->GetHand();
	ASHHand* Source = Turns->GetInitialDrawSourceHand();
	if (bReturned || !DrawnCards.Contains(Card) || !IsValid(Source) || !Hand->ContainsCard(Card)) { return; }
	Hand->RemoveCard(Card);
	Source->AddCard(Card, Source->GetCardCount());
	bReturned = true;
	Turns->FinishAdditionalDraw();
}

void UTakeSpecifiedCardEffectTask::StartEffect_Implementation()
{
	TArray<ASHHand*> Candidates;
	for (ASHHand* Hand : GetWorld()->GetGameState<ASHGameState>()->GetParticipantHands())
	{
		// Do not leak possession of the requested private card through target highlighting.
		if (IsValid(Hand) && Hand != GetActivatingPlayer()->GetHand()) { Candidates.Add(Hand); }
	}
	if (Candidates.IsEmpty()) { FinishEffect(); return; }
	RequestParticipantSelection(Candidates, EPlayerSelectionPurpose::CardTransferSource);
}

void UTakeSpecifiedCardEffectTask::HandleParticipantSelected(ASHHand* Hand)
{
	Source = Hand;
	PlayActivationVFX();
	ResolveAfterPresentation();
}

void UTakeSpecifiedCardEffectTask::ResolveAbility()
{
	const auto* Fragment = Cast<UTransferCardEffectFragment>(UCardDefinition::FindFragmentByClass(
		GetCardA()->GetCardDefinition(), UTransferCardEffectFragment::StaticClass()));
	if (IsValid(Source) && IsValid(Fragment))
	{
		GetTypedOuter<ASHGameMode>()->TransferCardToHand(Source, GetActivatingPlayer()->GetHand(), Fragment->CardDefinitionToTransfer);
	}
	// Shuffle even when Bodgy was absent, so targeting does not preserve known stack order.
	if (IsValid(Source) && Source->IsLogicalNPC()) { Source->ShuffleStack(); }
	FinishEffect();
}

TArray<ASHCard*> UStealSelectedPairEffectTask::GetCandidates(ASHPlayerState* Player) const
{
	const auto* Fragment = Cast<UStoredPairFilterEffectFragment>(UCardDefinition::FindFragmentByClass(
		GetCardA()->GetCardDefinition(), UStoredPairFilterEffectFragment::StaticClass()));
	return IsValid(Fragment) ? Fragment->GetEligibleCards(GetActivatingPlayer(), Player) : TArray<ASHCard*>();
}
void UStealSelectedPairEffectTask::StartEffect_Implementation()
{
	TArray<ASHCard*> Candidates;
	for (APlayerState* State : GetWorld()->GetGameState<ASHGameState>()->PlayerArray)
	{
		ASHPlayerState* Player = Cast<ASHPlayerState>(State);
		Candidates.Append(GetCandidates(Player));
	}
	if (!RequestActivationPairSelection(Candidates)) { FinishEffect(); }
}

void UStealSelectedPairEffectTask::HandleActivationPairSelected(ASHPlayerState* Owner, ASHCard* A, ASHCard* B)
{
	if (!GetCandidates(Owner).Contains(A)) { FinishEffect(); return; }
	Source = Owner;
	SelectedCard = A;
	PlayActivationVFX();
	ResolveAfterPresentation();
}

void UStealSelectedPairEffectTask::ResolveAbility()
{
	if (IsValid(Source) && GetCandidates(Source).Contains(SelectedCard))
	{
		bTransferredPair = GetTypedOuter<ASHGameMode>()->TransferStoredPair(Source->GetHand(), GetActivatingPlayer()->GetHand(), SelectedCard);
	}
	FinishEffect();
}

void URotateActivationZonesRightEffectTask::ResolveAbility()
{
	GetTypedOuter<ASHGameMode>()->RotateActivationZonesRight(GetCardA());
	FinishEffect();
}

void URemoveSelectedPairEffectTask::StartEffect_Implementation()
{
	TArray<ASHCard*> Candidates;
	for (APlayerState* State : GetWorld()->GetGameState<ASHGameState>()->PlayerArray)
	{
		ASHPlayerState* Player = Cast<ASHPlayerState>(State);
		if (!IsValid(Player) || !IsValid(Player->GetHand())) { continue; }
		for (const FActivatedPair& Pair : Player->GetHand()->GetLogicalActivationPairs())
		{
			if (Pair.CardA != GetCardA() && Pair.CardB != GetCardA() && IsValid(Pair.CardA) && IsValid(Pair.CardB) &&
				!Pair.bActivated && Pair.State == EActivationPairState::Ready)
			{
				Candidates.Add(Pair.CardA); Candidates.Add(Pair.CardB);
			}
		}
	}
	if (!RequestActivationPairSelection(Candidates)) { FinishEffect(); }
}

void URemoveSelectedPairEffectTask::HandleActivationPairSelected(ASHPlayerState* Owner, ASHCard* A, ASHCard* B)
{
	Source = Owner;
	SelectedCard = A;
	PlayActivationVFX();
	ResolveAfterPresentation();
}

void URemoveSelectedPairEffectTask::ResolveAbility()
{
	if (IsValid(Source))
	{
		bRemovedOtherPair = GetTypedOuter<ASHGameMode>()->RemoveStoredPairFromGame(Source->GetHand(), SelectedCard);
	}
	FinishEffect();
}

ECardEffectPairDisposition URemoveSelectedPairEffectTask::GetPairDisposition_Implementation() const
{
	return bRemovedOtherPair ? ECardEffectPairDisposition::RemoveFromGame : ECardEffectPairDisposition::KeepOnTable;
}

void UShuffleAllHandsEffectTask::ResolveAbility()
{
	GetTypedOuter<ASHGameMode>()->ShuffleAndRedealHands();
	FinishEffect();
}
