#include "Gameplay/Components/TurnComponent.h"
#include "Gameplay/Core/SHPlayerState.h"
#include "Gameplay/Core/SHGameMode.h"
#include "Gameplay/Cards/SHCard.h"
#include "Gameplay/SHHand.h"

void UTurnComponent::HandlePlayerDisconnected(ASHPlayerState* Player, ASHHand* ConvertedHand)
{
	CheckServerAuthority();
	ASHGameState* State = GetSHGameState();
	ClearDrawGuidance(Player);
	ForcedDrawSources.Remove(Player);
	PendingSkippedTurns.Remove(Player);
	for (ASHHand* Hand : State->GetParticipantHands())
	{
		if (!IsValid(Hand)) { continue; }
		for (const FActivatedPair& Pair : Hand->GetLogicalActivationPairs())
		{
			if (Pair.ActivationRetryBlockedUntilTurnOf == Player)
			{
				Hand->FindActivationPair(Pair.CardA)->ActivationRetryBlockedUntilTurnOf = nullptr;
				Hand->ForceNetUpdate();
			}
		}
	}
	PendingAdditionalDraws.RemoveAll([Player](const FPendingAdditionalDraw& Entry) { return Entry.Player == Player; });
	PendingPairSettlements.RemoveAll([ConvertedHand](const FActivatedPair& Pair)
	{
		return !IsValid(Pair.CardA) || Pair.CardA->GetOwningHand() == ConvertedHand;
	});
	if (AdditionalDrawPlayer == Player)
	{
		AdditionalDrawPlayer = nullptr;
		AdditionalDrawEffectTask = nullptr;
		bWaitingForAdditionalDraw = false;
		bWaitingForDrawReturn = false;
		RemainingSequenceDraws = 1;
	}
	if (State->GetCurrentPlayer() != Player)
	{
		if (bWaitingForAdditionalDraw && FirstDrawSourceHand == ConvertedHand) { BeginWaitingForAdditionalDraw(); }
		else if (ASHPlayerState* Current = State->GetCurrentPlayer()) { UpdateForcedDrawGuidance(Current); }
		return;
	}

	// The old PlayerState may be destroyed as soon as Logout returns. Advance
	// now instead of leaving a deferred EndTurn pointing at that actor.
	bEndTurnRequested = false;
	FirstDrawSourceHand = nullptr;
	FirstDrawnCard = nullptr;
	if (ASHGameMode* Mode = GetWorld()->GetAuthGameMode<ASHGameMode>()) { Mode->ExpirePanchoBoosts(); }
	for (ASHHand* Hand : State->GetParticipantHands())
	{
		if (!IsValid(Hand)) { continue; }
		for (const FActivatedPair& Pair : Hand->GetLogicalActivationPairs())
		{
			if (Pair.bDoubleEffectThisTurn) { Hand->FindActivationPair(Pair.CardA)->bDoubleEffectThisTurn = false; Hand->ForceNetUpdate(); }
		}
	}
	ASHPlayerState* Next = State->PlayerArray.IsEmpty() ? nullptr : ChooseNextPlayer(Player);
	while (Next && PendingSkippedTurns.Contains(Next))
	{
		if (--PendingSkippedTurns.FindChecked(Next) <= 0) { PendingSkippedTurns.Remove(Next); }
		Next = ChooseNextPlayer(Next);
	}
	if (IsValid(Next) && !State->IsGameEnded())
	{
		Next->SetProtectedFromCardEffects(false);
		InitializeTurns(Next);
	}
	else
	{
		State->SetCurrentPlayer(nullptr);
		State->SetTurnPhase(ETurnPhase::None);
	}
}
