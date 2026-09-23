#include "Gameplay/Core/SHGameMode.h"
#include "Gameplay/Core/SHPlayerState.h"
#include "Gameplay/Cards/CardDefinition.h"
#include "Gameplay/Cards/SHCard.h"
#include "Gameplay/Cards/Fragments/CardEffectFragment.h"
#include "Gameplay/Components/TurnComponent.h"
#include "Gameplay/SHHand.h"
#include "Gameplay/Cards/Tasks/CardEffectTask.h"
#include "TimerManager.h"

void ASHGameMode::RestartRepeatedPairEffect(UCardEffectTask* PreviousTask)
{
	if (!IsValid(PreviousTask) || !ActiveEffectTasks.Contains(PreviousTask)) { return; }
	if (TurnComponent && TurnComponent->HasNamedTurnTransitionBlocks())
	{
		// Older tasks finish immediately after starting their VFX. Let pending
		// victory moves flush before a fresh task builds its candidate lists.
		FTimerHandle Retry;
		GetWorldTimerManager().SetTimer(Retry, FTimerDelegate::CreateWeakLambda(this, [this, PreviousTask]()
		{
			RestartRepeatedPairEffect(PreviousTask);
		}), 0.02f, false);
		return;
	}
	ASHPlayerState* Player = PreviousTask->GetActivatingPlayer();
	ASHCard* A = PreviousTask->GetCardA();
	ASHCard* B = PreviousTask->GetCardB();
	ActiveEffectTasks.Remove(PreviousTask);
	if (IsValid(Player) && IsValid(Player->GetHand()) && IsValid(A) && IsValid(B) &&
		Player->GetHand()->FindActivationPair(A) && RepeatedPairEffects.Contains(A))
	{
		CardActivateEffect(Player, A, B);
		return;
	}
	RepeatedPairEffects.Remove(A);
	CompleteQueuedPairActivation(A, B);
	if (TurnComponent) { TurnComponent->NotifyEffectTaskFinished(); }
}

bool ASHGameMode::TryUseVictorySubstitute(ASHPlayerState* Player, ASHCard* CardA, ASHCard* CardB)
{
	// Capture replaces the destination itself; no victory cost is paid in that case.
	if (!IsValid(Player) || !IsValid(Player->GetHand()) || !IsValid(CardA) || PairCaptureRecipients.Contains(CardA)) { return false; }
	ASHHand* Hand = Player->GetHand();
	const FActivatedPair* Substitute = nullptr;
	for (const FActivatedPair& Pair : Hand->GetLogicalActivationPairs())
	{
		if (Pair.CardA == CardA || !IsValid(Pair.CardA) || !IsValid(Pair.CardB) ||
			Pair.bActivated || Pair.bActivationQueued || Pair.State != EActivationPairState::Ready) { continue; }
		const auto* Rule = Cast<UVictorySubstituteFragment>(UCardDefinition::FindFragmentByClass(
			Pair.CardA->GetCardDefinition(), UVictorySubstituteFragment::StaticClass()));
		if (Rule && Rule->AllowedCardDefinitions.Contains(FSoftClassPath(CardA->GetCardDefinition().Get())) &&
			(!Substitute || Pair.CreationOrder < Substitute->CreationOrder)) { Substitute = &Pair; }
	}
	if (!Substitute) { return false; }
	ASHCard* A = Substitute->CardA;
	ASHCard* B = Substitute->CardB;
	{
		TGuardValue<bool> Guard(bProcessingPairActivations, true);
		Hand->SetActivationPairQueued(A, B, true);
		Hand->SetActivationPairState(A, B, EActivationPairState::AbilityEffect);
		Hand->MulticastPairEffectActivated(A, B);
		Hand->SetActivationPairState(A, B, EActivationPairState::VictoryPresentation);
		Hand->MulticastPairReadyForVictory(A, B);
		MovePairToVictoryStack(Player, A, B);
		if (TurnComponent && TurnComponent->HasNamedTurnTransitionBlocks())
		{
			Hand->SetActivationPairQueued(CardA, CardB, true);
			FCompletedEffectPair& Completion = CompletedEffectPairsWaitingForPresentation.AddDefaulted_GetRef();
			Completion.ActivatingPlayer = Player;
			Completion.CardA = CardA;
			Completion.CardB = CardB;
			Completion.bMoveToVictoryStack = false;
			Completion.bRestoreReady = true;
		}
		else
		{
			Hand->SetActivationPairState(CardA, CardB, EActivationPairState::Ready);
			Hand->SetActivationPairQueued(CardA, CardB, false);
			Hand->RefreshActivationPairsPresentation();
			CompleteQueuedPairActivation(CardA, CardB);
		}
	}
	TryProcessQueuedPairActivations();
	return true;
}
