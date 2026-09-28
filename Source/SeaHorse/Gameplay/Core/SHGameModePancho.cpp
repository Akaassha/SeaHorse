#include "Gameplay/Core/SHGameMode.h"
#include "Gameplay/Core/SHGameState.h"
#include "Gameplay/Core/SHPlayerState.h"
#include "Gameplay/Cards/SHCard.h"
#include "Gameplay/Cards/Tasks/CardEffectTask.h"
#include "Gameplay/Board/VictoryStack.h"
#include "Gameplay/SHHand.h"

bool ASHGameMode::ApplyPanchoBoost(ASHPlayerState* Player, ASHCard* PanchoCard, ASHCard* TargetCard)
{
	if (!HasAuthority() || !IsValid(Player) || !IsValid(Player->GetHand())) { return false; }
	ASHHand* Hand = Player->GetHand();
	const FActivatedPair* Source = Hand->FindActivationPair(PanchoCard);
	FActivatedPair* Target = Hand->FindActivationPair(TargetCard);
	if (!Source || !Target || Source == Target || Target->bDoubleEffectThisTurn || Target->State != EActivationPairState::Ready) { return false; }
	FPanchoBoost& Boost = PanchoBoosts.FindOrAdd(Source->CardA);
	if (Boost.bRefundPending) { return false; }
	if (!Boost.CardA.IsValid())
	{
		Boost.Player = Player;
		Boost.OriginalHand = Hand;
		Boost.CardA = Source->CardA;
		Boost.CardB = Source->CardB;
		Boost.CreationOrder = Source->CreationOrder;
		Boost.OriginalIndex = Hand->GetLogicalActivationPairs().IndexOfByKey(*Source);
	}
	Boost.Targets.AddUnique(Target->CardA);
	PanchoBoostSources.Add(Target->CardA, Source->CardA);
	Target->bDoubleEffectThisTurn = true;
	Hand->ForceNetUpdate();
	return true;
}

void ASHGameMode::ResolvePanchoBoostForTarget(ASHCard* Target, bool bBothExecutionsSucceeded)
{
	if (IsValid(Target))
	{
		if (ASHHand* Hand = Target->GetOwningHand())
		{
			if (FActivatedPair* Pair = Hand->FindActivationPair(Target))
			{
				Target = Pair->CardA;
				if (PanchoBoostSources.Contains(Target)) { Pair->bDoubleEffectThisTurn = false; Hand->ForceNetUpdate(); }
			}
		}
	}
	TWeakObjectPtr<ASHCard> Source;
	if (!PanchoBoostSources.RemoveAndCopyValue(Target, Source)) { return; }
	FPanchoBoost* Boost = PanchoBoosts.Find(Source);
	if (!Boost) { return; }
	Boost->Targets.Remove(Target);
	if (!bBothExecutionsSucceeded)
	{
		if (Boost->bConsumedByTargetEffect)
		{
			// A collecting target such as Gnushor can move its own Pancho to
			// victory during the first execution. Its missing repeat is then an
			// expected consequence of that successful effect, not a refundable failure.
			if (Boost->Targets.IsEmpty()) { PanchoBoosts.Remove(Source); }
		}
		else { RequestPanchoRefund(Source.Get()); }
	}
	else if (Boost->Targets.IsEmpty() && !Boost->bRefundPending)
	{
		Boost->SettlementTarget = Target;
		Boost->bCommitPending = true;
	}
	FlushPanchoRefunds();
}

void ASHGameMode::RequestPanchoRefund(ASHCard* PanchoCard)
{
	FPanchoBoost* Boost = PanchoBoosts.Find(PanchoCard);
	if (!Boost) { return; }
	Boost->bRefundPending = true;
	Boost->bCommitPending = false;
	// A doubled Pancho can have granted more than one bonus. Refunding that
	// activation also revokes any of its bonuses which have not yet been used.
	for (const TWeakObjectPtr<ASHCard>& Target : Boost->Targets)
	{
		PanchoBoostSources.Remove(Target);
		if (FRepeatedPairEffect* Repeat = RepeatedPairEffects.Find(Target))
		{
			Repeat->Remaining = 0;
			Repeat->bRepeatCancelled = true;
		}
		if (Target.IsValid())
		{
			if (ASHHand* Hand = Target->GetOwningHand())
			{
				if (FActivatedPair* Pair = Hand->FindActivationPair(Target.Get()))
				{
					Pair->bDoubleEffectThisTurn = false;
					Hand->ForceNetUpdate();
				}
			}
		}
	}
	Boost->Targets.Reset();
}

void ASHGameMode::FlushPanchoRefunds()
{
	bool bQueuedSuccessfulPancho = false;
	{
		TGuardValue<bool> Guard(bProcessingPairActivations, true);
		TArray<TWeakObjectPtr<ASHCard>> Sources;
		PanchoBoosts.GetKeys(Sources);
		for (const TWeakObjectPtr<ASHCard>& Source : Sources)
		{
			const FPanchoBoost* Pending = PanchoBoosts.Find(Source);
			if (!Pending || (!Pending->bRefundPending && !Pending->bCommitPending)) { continue; }
			const TWeakObjectPtr<ASHCard> SettlementTarget = Pending->SettlementTarget;
			// Pancho's own task and the selected pair's final presentation must finish first.
			if (ActiveEffectTasks.ContainsByPredicate([Source, SettlementTarget](const UCardEffectTask* Task)
				{ return IsValid(Task) && (Task->GetCardA() == Source || Task->GetCardA() == SettlementTarget); }) ||
				PendingSuccessfulActivations.ContainsByPredicate([Source, SettlementTarget](const FSuccessfulActivation& Entry)
				{ return Entry.Activation.CardA == Source || Entry.Activation.CardA == SettlementTarget; }) ||
				CompletedEffectPairsWaitingForPresentation.ContainsByPredicate([Source, SettlementTarget](const FCompletedEffectPair& Entry)
				{ return Entry.CardA == Source || Entry.CardA == SettlementTarget; }) ||
				(bReactionWindowOpen && (ReactionRootActivation.CardA == Source || ReactionRootActivation.CardA == SettlementTarget)))
			{
				continue;
			}
			const FPanchoBoost Boost = *Pending;
			ASHHand* OriginalHand = Boost.OriginalHand.Get();
			ASHCard* A = Boost.CardA.Get();
			ASHCard* B = Boost.CardB.Get();
			if (!IsValid(OriginalHand) || OriginalHand->IsLogicalNPC() || !Boost.Player.IsValid() || !IsValid(A) || !IsValid(B))
			{
				PanchoBoosts.Remove(Source);
				continue;
			}
			if (A->GetOwner() != B->GetOwner()) { continue; }
			AVictoryStack* Stack = Cast<AVictoryStack>(A->GetOwner());
			ASHHand* CurrentHand = A->GetOwningHand();
			const FActivatedPair* Pair = IsValid(CurrentHand) ? CurrentHand->FindActivationPair(A) : nullptr;

			if (Boost.bCommitPending)
			{
				if (Stack)
				{
					PanchoBoosts.Remove(Source);
					continue;
				}
				if (!Pair || Pair->CardB != B || CurrentHand != OriginalHand || Boost.Player->GetHand() != OriginalHand) { continue; }
				PanchoBoosts.Remove(Source);
				QueueSuccessfulActivation(Boost.Player.Get(), A, B);
				bQueuedSuccessfulPancho = true;
				continue;
			}

			// Failure normally only unlocks the pair which never left the zone.
			if (Pair && Pair->CardB == B && CurrentHand == OriginalHand && Pair->State == EActivationPairState::Ready)
			{
				PanchoBoosts.Remove(Source);
				OriginalHand->SetActivationPairOutcomePending(A, B, false);
				OriginalHand->MulticastPairActivationCancelled(A, B);
				continue;
			}

			// Compatibility fallback for an already running session created by the old behavior.
			if (!Stack || !Stack->RemovePair(A, B)) { continue; }
			PanchoBoosts.Remove(Source);
			FActivatedPair Restored;
			Restored.CardA = A;
			Restored.CardB = B;
			Restored.CreationOrder = Boost.CreationOrder;
			OriginalHand->ReceiveTransferredPair(Restored, Boost.OriginalIndex);
			OriginalHand->SetActivationPairOutcomePending(A, B, false);
			OriginalHand->MulticastPairActivationCancelled(A, B);
			for (APlayerState* Entry : GetGameState<ASHGameState>()->PlayerArray) { RefreshPlayerScore(Cast<ASHPlayerState>(Entry)); }
		}
	}
	if (bQueuedSuccessfulPancho) { ProcessSuccessfulActivations(); }
}

void ASHGameMode::ExpirePanchoBoosts()
{
	check(HasAuthority());
	TArray<TWeakObjectPtr<ASHCard>> Sources;
	PanchoBoosts.GetKeys(Sources);
	for (const TWeakObjectPtr<ASHCard>& Source : Sources) { RequestPanchoRefund(Source.Get()); }
	FlushPanchoRefunds();
}

void ASHGameMode::RefundPanchoBoostsForDisconnect(ASHPlayerState* Player)
{
	TArray<TWeakObjectPtr<ASHCard>> Sources;
	for (const auto& Entry : PanchoBoosts)
	{
		if (Entry.Value.Player == Player || Entry.Value.Targets.ContainsByPredicate([Player](const TWeakObjectPtr<ASHCard>& Target)
		{
			return Target.IsValid() && IsValid(Player) && Target->GetOwningHand() == Player->GetHand();
		})) { Sources.Add(Entry.Key); }
	}
	for (const TWeakObjectPtr<ASHCard>& Source : Sources) { RequestPanchoRefund(Source.Get()); }
	FlushPanchoRefunds();
}
