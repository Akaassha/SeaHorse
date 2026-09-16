#include "Gameplay/Core/SHGameMode.h"
#include "Gameplay/Core/SHGameState.h"
#include "Gameplay/Core/SHPlayerState.h"
#include "Gameplay/Core/SHPlayerController.h"
#include "Gameplay/Cards/SHCard.h"
#include "Gameplay/Cards/CardDefinition.h"
#include "Gameplay/Cards/Fragments/CardReactionFragment.h"
#include "Gameplay/Components/TurnComponent.h"
#include "Gameplay/SHHand.h"

bool ASHGameMode::IsReactionOptionValid(const FReactionOption& Option) const
{
	const ASHGameState* State = GetGameState<ASHGameState>();
	ASHPlayerState* Player = Option.Player.Get();
	ASHPlayerState* Target = ReactionTargetPlayer.Get();
	if (!State || !IsValid(Player) || !IsValid(Target) || Player == Target || Player == State->GetCurrentPlayer() ||
		Target->IsProtectedFromCardEffects() || !IsValid(Player->GetOwner()) || !IsValid(Player->GetHand()) || !IsValid(Target->GetHand())) { return false; }
	const FActivatedPair* Pair = Player->GetHand()->FindActivationPair(Option.CardA.Get());
	const FActivatedPair* TargetPair = Target->GetHand()->FindActivationPair(ReactionTargetA.Get());
	return Pair && TargetPair && TargetPair->CardB == ReactionTargetB && IsValid(Pair->CardA) && IsValid(Pair->CardB) && Pair->CardB == Option.CardB &&
		!Pair->bActivated && !Pair->bActivationQueued && Pair->State == EActivationPairState::Ready &&
		UCardDefinition::FindFragmentByClass(Pair->CardA->GetCardDefinition(), UCardReactionFragment::StaticClass());
}

bool ASHGameMode::BeginCardReactions(const FPendingPairActivation& Activation)
{
	check(HasAuthority());
	ReactionRootActivation = Activation;
	ReactionChain.Reset();
	bReactionWindowOpen = true;
	GetGameState<ASHGameState>()->SetReactionPending(true);
	OpenCardReactionWindow(Activation.ActivatingPlayer.Get(), Activation.CardA.Get(), Activation.CardB.Get());
	return bReactionWindowOpen;
}

void ASHGameMode::OpenCardReactionWindow(ASHPlayerState* TargetPlayer, ASHCard* CardA, ASHCard* CardB)
{
	const int32 WindowSerial = ++ReactionWindowSerial;
	ReactionTargetPlayer = TargetPlayer;
	ReactionTargetA = CardA;
	ReactionTargetB = CardB;
	ReactionOptions.Reset();
	ActiveReactionOffers.Reset();
	TArray<ASHPlayerState*> ReactingPlayers;
	for (APlayerState* Entry : GetGameState<ASHGameState>()->PlayerArray)
	{
		ASHPlayerState* Player = Cast<ASHPlayerState>(Entry);
		if (!IsValid(Player) || !IsValid(Player->GetHand())) { continue; }
		for (const FActivatedPair& Pair : Player->GetHand()->GetLogicalActivationPairs())
		{
			FReactionOption Option{Player, Pair.CardA.Get(), Pair.CardB.Get(), Pair.CreationOrder};
			if (IsReactionOptionValid(Option))
			{
				ReactionOptions.Add(Option);
				ReactingPlayers.AddUnique(Player);
			}
		}
	}
	if (ReactionOptions.IsEmpty()) { ResolveCardReactionChain(); return; }
	// Creation order only chooses which of one owner's pairs to offer first.
	// It gives no priority over another player: all owners receive an offer now.
	ReactionOptions.Sort([](const FReactionOption& A, const FReactionOption& B) { return A.CreationOrder < B.CreationOrder; });
	for (ASHPlayerState* Player : ReactingPlayers)
	{
		// A local widget can answer synchronously while its offer is being presented.
		if (!bReactionWindowOpen || WindowSerial != ReactionWindowSerial) { break; }
		OfferNextCardReaction(Player);
	}
}

void ASHGameMode::OfferNextCardReaction(ASHPlayerState* Player)
{
	if (!bReactionWindowOpen || ActiveReactionOffers.Contains(Player)) { return; }
	for (int32 Index = 0; Index < ReactionOptions.Num();)
	{
		if (ReactionOptions[Index].Player != Player) { ++Index; continue; }
		FReactionOption Option = ReactionOptions[Index];
		ReactionOptions.RemoveAt(Index);
		if (!IsReactionOptionValid(Option)) { continue; }
		ASHPlayerController* PC = Cast<ASHPlayerController>(Option.Player->GetOwner());
		if (!PC) { continue; }
		const auto* Fragment = Cast<UCardReactionFragment>(UCardDefinition::FindFragmentByClass(Option.CardA->GetCardDefinition(), UCardReactionFragment::StaticClass()));
		Option.OfferId = ++ReactionOfferSerial;
		ActiveReactionOffers.Add(Player, Option);
		PC->ClientOfferCardReaction(Option.OfferId, Option.CardA.Get(), ReactionTargetA.Get(), Fragment->PromptWidgetClass);
		return;
	}
	if (!ActiveReactionOffers.IsEmpty() || !ReactionOptions.IsEmpty()) { return; }
	ResolveCardReactionChain();
}

void ASHGameMode::ClearCardReactionOffers()
{
	// Invalidate every offer before callbacks can respond to a closing widget.
	const auto OffersToClose = MoveTemp(ActiveReactionOffers);
	ActiveReactionOffers.Reset();
	ReactionOptions.Reset();
	for (const auto& Entry : OffersToClose)
	{
		if (ASHPlayerState* Player = Entry.Key.Get())
		{
			if (ASHPlayerController* PC = Cast<ASHPlayerController>(Player->GetOwner())) { PC->ClientCloseCardReaction(Entry.Value.OfferId); }
		}
	}
}

void ASHGameMode::CloseCardReactions()
{
	ClearCardReactionOffers();
	bReactionWindowOpen = false;
	GetGameState<ASHGameState>()->SetReactionPending(false);
}

void ASHGameMode::ConsumeReactionPair(const FReactionOption& Option, bool bExecuteEffect)
{
	ASHHand* Hand = Option.Player->GetHand();
	const auto* Fragment = Cast<UCardReactionFragment>(UCardDefinition::FindFragmentByClass(Option.CardA->GetCardDefinition(), UCardReactionFragment::StaticClass()));
	if (bExecuteEffect)
	{
		Hand->SetActivationPairState(Option.CardA.Get(), Option.CardB.Get(), EActivationPairState::AbilityEffect);
		Hand->MulticastPairEffectActivated(Option.CardA.Get(), Option.CardB.Get());
		if (Fragment && Fragment->ActivationVFX)
		{
			Hand->MulticastPlayActivationVFX(Option.CardA.Get(), Option.CardB.Get(), Fragment->ActivationVFX, Fragment->ActivationVFXDuration);
		}
	}
	Hand->SetActivationPairState(Option.CardA.Get(), Option.CardB.Get(), EActivationPairState::VictoryPresentation);
	Hand->MulticastPairReadyForVictory(Option.CardA.Get(), Option.CardB.Get());
	MovePairToVictoryStack(Option.Player.Get(), Option.CardA.Get(), Option.CardB.Get());
}

void ASHGameMode::RespondToCardReaction(ASHPlayerState* Player, int32 OfferId, bool bAccept)
{
	if (!HasAuthority() || !bReactionWindowOpen || !IsValid(Player)) { return; }
	const FReactionOption* Offered = ActiveReactionOffers.Find(Player);
	if (!Offered || OfferId != Offered->OfferId) { return; }
	const FReactionOption Option = *Offered;
	if (!bAccept || !IsReactionOptionValid(Option))
	{
		ActiveReactionOffers.Remove(Player);
		if (ASHPlayerController* PC = Cast<ASHPlayerController>(Player->GetOwner())) { PC->ClientCloseCardReaction(OfferId); }
		OfferNextCardReaction(Player);
		return;
	}
	// First acceptance wins this window, but its effect can itself be countered.
	{
		TGuardValue<bool> Guard(bProcessingPairActivations, true);
		ClearCardReactionOffers();
		ReactionChain.Add(FReactionActivation{Option, false});
		ASHHand* Hand = Player->GetHand();
		Hand->SetActivationPairQueued(Option.CardA.Get(), Option.CardB.Get(), true);
		Hand->SetActivationPairState(Option.CardA.Get(), Option.CardB.Get(), EActivationPairState::ClickPresentation);
		Hand->MulticastPairClicked(Option.CardA.Get(), Option.CardB.Get());
	}
	OpenCardReactionWindow(Player, Option.CardA.Get(), Option.CardB.Get());
}

void ASHGameMode::ResolveCardReactionChain()
{
	if (!bReactionWindowOpen) { return; }
	{
		TGuardValue<bool> Guard(bProcessingPairActivations, true);
		ClearCardReactionOffers();
		bool bRootCancelled = false;
		for (int32 Index = ReactionChain.Num() - 1; Index >= 0; --Index)
		{
			const FReactionActivation& Reaction = ReactionChain[Index];
			const FReactionOption& Option = Reaction.Option;
			ASHPlayerState* Player = Option.Player.Get();
			ASHHand* Hand = IsValid(Player) ? Player->GetHand() : nullptr;
			const FActivatedPair* Pair = IsValid(Hand) ? Hand->FindActivationPair(Option.CardA.Get()) : nullptr;
			if (!Pair || !Option.CardA.IsValid() || !Option.CardB.IsValid() || Pair->CardB != Option.CardB) { continue; }

			ASHCard* ParentA = Index > 0 ? ReactionChain[Index - 1].Option.CardA.Get() : ReactionRootActivation.CardA.Get();
			if (!Reaction.bCancelled && IsValid(ParentA))
			{
				const auto* Fragment = Cast<UCardReactionFragment>(UCardDefinition::FindFragmentByClass(Option.CardA->GetCardDefinition(), UCardReactionFragment::StaticClass()));
				if (Fragment && Fragment->ReactionKind == ECardReactionKind::CancelActivation)
				{
					if (Index > 0) { ReactionChain[Index - 1].bCancelled = true; }
					else { bRootCancelled = true; }
				}
				else if (Fragment)
				{
					// The parent executes next, then its normal victory move transfers it instead.
					PairCaptureRecipients.Add(ParentA, Player);
				}
			}
			ConsumeReactionPair(Option, !Reaction.bCancelled);
		}
		if (bRootCancelled && IsValid(ReactionRootActivation.ActivatingPlayer))
		{
			ASHPlayerState* Player = ReactionRootActivation.ActivatingPlayer;
			ASHCard* A = ReactionRootActivation.CardA;
			ASHCard* B = ReactionRootActivation.CardB;
			ASHHand* Hand = Player->GetHand();
			if (IsValid(Hand) && IsValid(A) && IsValid(B) && Hand->FindActivationPair(A))
			{
				for (FPendingPairActivation& Pending : PendingPairActivations) { if (Pending.CardA == A) { Pending.bAbilityStarted = true; } }
				Hand->SetActivationPairState(A, B, EActivationPairState::VictoryPresentation);
				Hand->MulticastPairReadyForVictory(A, B);
				MovePairToVictoryStack(Player, A, B);
			}
		}
		ReactionChain.Reset();
		ReactionRootActivation = FPendingPairActivation{};
		// Keep gameplay paused throughout the unwind, including Blueprint callbacks.
		CloseCardReactions();
	}
	if (!bProcessingPairActivations)
	{
		TryProcessQueuedPairActivations();
		if (TurnComponent) { TurnComponent->NotifyEffectTaskFinished(); }
	}
}

void ASHGameMode::Logout(AController* Exiting)
{
	ASHPlayerState* Player = Exiting ? Exiting->GetPlayerState<ASHPlayerState>() : nullptr;
	if (bReactionWindowOpen && Player)
	{
		if (ReactionRootActivation.ActivatingPlayer == Player)
		{
			TGuardValue<bool> Guard(bProcessingPairActivations, true);
			PendingPairActivations.RemoveAll([Player](const FPendingPairActivation& Entry) { return Entry.ActivatingPlayer == Player; });
			ReactionRootActivation = FPendingPairActivation{};
			ResolveCardReactionChain();
		}
		else
		{
			ReactionOptions.RemoveAll([Player](const FReactionOption& Option) { return Option.Player == Player; });
			FReactionOption Offer;
			if (ActiveReactionOffers.RemoveAndCopyValue(Player, Offer))
			{
				if (ASHPlayerController* PC = Cast<ASHPlayerController>(Player->GetOwner())) { PC->ClientCloseCardReaction(Offer.OfferId); }
			}
			OfferNextCardReaction(Player); // Resume only if nobody else can still answer.
		}
	}
	Super::Logout(Exiting);
	TryProcessQueuedPairActivations();
	if (TurnComponent) { TurnComponent->NotifyEffectTaskFinished(); }
}
