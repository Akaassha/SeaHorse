#include "SeaHorse/Gameplay/Components/TurnComponent.h"

#include "SeaHorse/Gameplay/Cards/CardDefinition.h"
#include "SeaHorse/Gameplay/Cards/Fragments/CardActivationRulesFragment.h"
#include "SeaHorse/Gameplay/Cards/SHCard.h"
#include "SeaHorse/Gameplay/Cards/Tasks/CardEffectTask.h"
#include "SeaHorse/Gameplay/Core/SHPlayerController.h"
#include "SeaHorse/Gameplay/Core/SHPlayerState.h"
#include "SeaHorse/Gameplay/Core/SHGameMode.h"
#include "SeaHorse/Gameplay/SHHand.h"

UTurnComponent::UTurnComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UTurnComponent::InitializeTurns(ASHPlayerState* StartingPlayer)
{
	CheckServerAuthority();

	checkf(IsValid(StartingPlayer), TEXT("Cannot initialize turns without a starting player"));

	ASHGameState* GameState = GetSHGameState();
	GameState->SetCurrentPlayer(StartingPlayer);
	bPairingActionUsed = false;
	GameState->SetTurnPhase(ETurnPhase::FirstPairing);
}

void UTurnComponent::CompleteCurrentPhase(ETurnPhaseEndReason Reason)
{
	CheckServerAuthority();

	ASHGameState* GameState = GetSHGameState();
	const ETurnPhase NextPhase = GetNextTurnPhase(GameState->GetTurnPhase(), Reason);

	if (NextPhase == ETurnPhase::None)
	{
		EndTurn();
		return;
	}

	EnterTurnPhase(NextPhase);
}

void UTurnComponent::SkipCurrentPhase(ASHPlayerState* RequestingPlayer)
{
	CheckServerAuthority();

	ASHGameState* GameState = GetSHGameState();
	if (!IsValid(RequestingPlayer) || GameState->GetCurrentPlayer() != RequestingPlayer)
	{
		return;
	}

	const ETurnPhase CurrentPhase = GameState->GetTurnPhase();
	if (CurrentPhase != ETurnPhase::FirstPairing && CurrentPhase != ETurnPhase::SecondPairing)
	{
		return;
	}

	CompleteCurrentPhase(ETurnPhaseEndReason::PlayerSkipped);
}

bool UTurnComponent::CanActivatePair(ASHPlayerState* RequestingPlayer, const FActivatedPair& ActivatedPair) const
{
	if (!IsValid(RequestingPlayer) || !IsValid(ActivatedPair.CardA) ||
		!IsValid(ActivatedPair.CardB) || ActivatedPair.bActivated)
	{
		return false;
	}

	const ASHGameState* GameState = GetSHGameState();
	if (GameState->IsGameEnded())
	{
		return false;
	}

	const bool bIsOwnTurn = GameState->CurrentPlayer == RequestingPlayer;
	const UCardActivationRulesFragment* Rules = Cast<UCardActivationRulesFragment>(
		UCardDefinition::FindFragmentByClass(
			ActivatedPair.CardA->CardDefinition,
			UCardActivationRulesFragment::StaticClass()));

	if (!IsValid(Rules))
	{
		return bIsOwnTurn;
	}

	if (!Rules->bCanBeActivated)
	{
		return false;
	}

	switch (Rules->TurnRestriction)
	{
	case ECardActivationRules::OwnTurn:
		if (!bIsOwnTurn)
		{
			return false;
		}
		break;

	case ECardActivationRules::OutsideOwnTurn:
		if (bIsOwnTurn)
		{
			return false;
		}
		break;

	case ECardActivationRules::AnyTurn:
		break;
	}

	return Rules->AllowedPhases.IsEmpty() || Rules->AllowedPhases.Contains(GameState->GetTurnPhase());
}

bool UTurnComponent::CanDrawCard(ASHPlayerState* DrawingPlayer, ASHPlayerState* SourcePlayer) const
{
	if (!IsValid(SourcePlayer))
	{
		return false;
	}
	return CanDrawCardFromHand(DrawingPlayer, SourcePlayer->GetHand());
}

bool UTurnComponent::CanDrawCardFromHand(ASHPlayerState* DrawingPlayer, ASHHand* SourceHand) const
{
	if (!IsValid(DrawingPlayer) || !IsValid(SourceHand) || DrawingPlayer->GetHand() == SourceHand)
	{
		return false;
	}

	const ASHGameState* GameState = GetSHGameState();
	if (GameState->IsGameEnded() || GameState->GetCurrentPlayer() != DrawingPlayer)
	{
		return false;
	}

	const ETurnPhase Phase = GameState->GetTurnPhase();
	if (Phase != ETurnPhase::FirstPairing && Phase != ETurnPhase::DrawCard)
	{
		return false;
	}

	if (SourceHand->GetCardCount() <= 0)
	{
		return false;
	}

	if (bWaitingForAdditionalDraw)
	{
		if (DrawingPlayer != AdditionalDrawPlayer || !IsValid(FirstDrawSourceHand))
		{
			return false;
		}

		return AdditionalDrawSourceRule == EAdditionalDrawSourceRule::SamePlayer
			? SourceHand == FirstDrawSourceHand
			: SourceHand != FirstDrawSourceHand;
	}

	if (ASHHand* ForcedSourceHand = GetFirstForcedDrawSourceHand(DrawingPlayer))
	{
		if (SourceHand != ForcedSourceHand)
		{
			return false;
		}
	}

	if (DrawingPlayer == AdditionalDrawPlayer)
	{
		// The first draw remains legal even when no second source will be
		// available afterwards. In that case the effect is completed after
		// the first card has actually been drawn.
		return true;
	}

	return true;
}

void UTurnComponent::HandleCardDrawn(ASHPlayerState* DrawingPlayer, ASHPlayerState* SourcePlayer)
{
	checkf(IsValid(SourcePlayer), TEXT("Invalid card draw source"));
	HandleCardDrawnFromHand(DrawingPlayer, SourcePlayer->GetHand());
}

void UTurnComponent::HandleCardDrawnFromHand(ASHPlayerState* DrawingPlayer, ASHHand* SourceHand)
{
	CheckServerAuthority();
	checkf(IsValid(DrawingPlayer) && IsValid(SourceHand), TEXT("Invalid card draw notification"));

	if (bWaitingForAdditionalDraw)
	{
		ClearDrawGuidance(DrawingPlayer);
		FinishAdditionalDraw();
		return;
	}

	if (FForcedDrawSourceQueue* ForcedQueue = ForcedDrawSources.Find(DrawingPlayer))
	{
		if (!ForcedQueue->Sources.IsEmpty())
		{
			ForcedQueue->Sources.RemoveAt(0);
		}

		if (ForcedQueue->Sources.IsEmpty())
		{
			ForcedDrawSources.Remove(DrawingPlayer);
		}
	}

	if (DrawingPlayer == AdditionalDrawPlayer)
	{
		FirstDrawSourceHand = SourceHand;
		BeginWaitingForAdditionalDraw();
		return;
	}

	ClearDrawGuidance(DrawingPlayer);
	CompleteCurrentPhase(ETurnPhaseEndReason::CardDrawn);
}

void UTurnComponent::FinishAdditionalDraw()
{
	UCardEffectTask* CompletedEffectTask = AdditionalDrawEffectTask;
	if (IsValid(CompletedEffectTask))
	{
		CompletedEffectTask->FinishEffect();
	}

	if (!PendingAdditionalDraws.IsEmpty())
	{
		const FPendingAdditionalDraw NextDraw = PendingAdditionalDraws[0];
		PendingAdditionalDraws.RemoveAt(0);
		AdditionalDrawPlayer = NextDraw.Player;
		AdditionalDrawEffectTask = NextDraw.EffectTask;
		AdditionalDrawSourceRule = NextDraw.SourceRule;
		BeginWaitingForAdditionalDraw();
		return;
	}

	AdditionalDrawPlayer = nullptr;
	FirstDrawSourceHand = nullptr;
	AdditionalDrawEffectTask = nullptr;
	bWaitingForAdditionalDraw = false;
	EnterTurnPhase(ETurnPhase::SecondPairing);
	TryCompleteDeferredEndTurn();
}

void UTurnComponent::BeginWaitingForAdditionalDraw()
{
	bWaitingForAdditionalDraw = true;
	TArray<ASHPlayerState*> ValidSources;
	TArray<ASHHand*> ValidSourceHands;

	for (APlayerState* PlayerState : GetSHGameState()->PlayerArray)
	{
		ASHPlayerState* Candidate = Cast<ASHPlayerState>(PlayerState);
		if (CanDrawCard(AdditionalDrawPlayer, Candidate))
		{
			ValidSources.Add(Candidate);
			ValidSourceHands.Add(Candidate->GetHand());
		}
	}
	for (ASHHand* NPCHand : GetSHGameState()->GetNPCHands())
	{
		if (CanDrawCardFromHand(AdditionalDrawPlayer, NPCHand))
		{
			ValidSourceHands.Add(NPCHand);
		}
	}

	if (ValidSourceHands.IsEmpty())
	{
		UE_LOG(LogTemp, Log,
			TEXT("[SH_ADDITIONAL_DRAW] No valid source; completing queued effect without a draw"));
		FinishAdditionalDraw();
		return;
	}

	ASHPlayerController* Controller = Cast<ASHPlayerController>(AdditionalDrawPlayer->GetOwner());
	if (IsValid(Controller))
	{
		const ECardDrawGuidanceType GuidanceType =
			AdditionalDrawSourceRule == EAdditionalDrawSourceRule::SamePlayer
			? ECardDrawGuidanceType::AdditionalFromSamePlayer
			: ECardDrawGuidanceType::AdditionalFromDifferentPlayer;
		Controller->ClientUpdateCardDrawGuidance(ValidSources, GuidanceType);
		Controller->ClientSetGuidedDrawHands(ValidSourceHands);
	}
}

void UTurnComponent::ScheduleAdditionalDraw(
	UCardEffectTask* EffectTask,
	ASHPlayerState* PlayerState,
	EAdditionalDrawSourceRule SourceRule)
{
	CheckServerAuthority();
	checkf(IsValid(EffectTask), TEXT("Cannot schedule an additional draw without its effect task"));
	checkf(IsValid(PlayerState), TEXT("Cannot schedule a draw for an invalid player"));
	checkf(GetSHGameState()->GetCurrentPlayer() == PlayerState, TEXT("Additional draw must target the current player"));
	if (!IsValid(AdditionalDrawPlayer))
	{
		AdditionalDrawPlayer = PlayerState;
		AdditionalDrawEffectTask = EffectTask;
		AdditionalDrawSourceRule = SourceRule;
		return;
	}

	FPendingAdditionalDraw& Pending = PendingAdditionalDraws.AddDefaulted_GetRef();
	Pending.EffectTask = EffectTask;
	Pending.Player = PlayerState;
	Pending.SourceRule = SourceRule;
	UE_LOG(LogTemp, Log, TEXT("[SH_ADDITIONAL_DRAW] Queued effect; pending count=%d"), PendingAdditionalDraws.Num());
}

void UTurnComponent::SetForcedDrawSource(ASHPlayerState* DrawingPlayer, ASHPlayerState* SourcePlayer)
{
	SetForcedDrawSourceHand(DrawingPlayer, IsValid(SourcePlayer) ? SourcePlayer->GetHand() : nullptr);
}

void UTurnComponent::SetForcedDrawSourceHand(ASHPlayerState* DrawingPlayer, ASHHand* SourceHand)
{
	CheckServerAuthority();
	checkf(IsValid(DrawingPlayer) && IsValid(SourceHand) && DrawingPlayer->GetHand() != SourceHand,
		TEXT("Invalid forced draw participants"));

	ForcedDrawSources.FindOrAdd(DrawingPlayer).Sources.Add(SourceHand);
	UpdateForcedDrawGuidance(DrawingPlayer);
}

void UTurnComponent::ScheduleSkippedTurn(ASHPlayerState* PlayerState)
{
	CheckServerAuthority();
	checkf(IsValid(PlayerState), TEXT("Cannot skip a turn for an invalid player"));

	++PendingSkippedTurns.FindOrAdd(PlayerState);
}

ETurnPhase UTurnComponent::GetNextTurnPhase_Implementation(
	ETurnPhase CurrentPhase,
	ETurnPhaseEndReason Reason) const
{
	switch (CurrentPhase)
	{
	case ETurnPhase::FirstPairing:
		return Reason == ETurnPhaseEndReason::CardDrawn
			? ETurnPhase::SecondPairing
			: ETurnPhase::DrawCard;

	case ETurnPhase::DrawCard:
		return ETurnPhase::SecondPairing;

	case ETurnPhase::SecondPairing:
	case ETurnPhase::None:
	default:
		return ETurnPhase::None;
	}
}

ASHPlayerState* UTurnComponent::ChooseNextPlayer_Implementation(ASHPlayerState* CurrentPlayer) const
{
	checkf(IsValid(CurrentPlayer), TEXT("Invalid current player"));

	ASHGameState* GameState = GetSHGameState();
	const int32 ParticipantCount = GameState->GetParticipantCount();
	checkf(ParticipantCount > 0, TEXT("Cannot choose the next player without participants"));

	// Seats can contain NPCs and human seat indices are therefore not
	// necessarily contiguous. Walk clockwise through every table seat and
	// return the next human; NPC turns are skipped entirely.
	for (int32 Offset = 1; Offset <= ParticipantCount; ++Offset)
	{
		const int32 CandidateSeat = (CurrentPlayer->GetSeatIndex() + Offset) % ParticipantCount;
		for (APlayerState* PlayerState : GameState->PlayerArray)
		{
			ASHPlayerState* SHPlayerState = Cast<ASHPlayerState>(PlayerState);
			if (IsValid(SHPlayerState) && SHPlayerState->GetSeatIndex() == CandidateSeat)
			{
				return SHPlayerState;
			}
		}
	}

	return nullptr;
}

void UTurnComponent::EndTurn()
{
	checkf(!bWaitingForAdditionalDraw, TEXT("Cannot end turn while an additional draw is pending"));
	if (HasTurnTransitionBlockers())
	{
		bEndTurnRequested = true;
		const ASHGameMode* BlockingGameMode = GetWorld()->GetAuthGameMode<ASHGameMode>();
		UE_LOG(LogTemp, Log,
			TEXT("[SH_TURN_QUEUE] Deferring end turn: pairs=%d named=%d activeTasks=%d"),
			PendingPairSettlements.Num(), NamedTurnTransitionBlocks.Num(),
			IsValid(BlockingGameMode) && BlockingGameMode->HasActiveEffectTasks());
		return;
	}
	bEndTurnRequested = false;

	ASHGameMode* GameMode = GetWorld()->GetAuthGameMode<ASHGameMode>();
	if (IsValid(GameMode) && GameMode->TryFinishGame())
	{
		return;
	}

	ASHGameState* GameState = GetSHGameState();
	ASHPlayerState* CurrentPlayer = GameState->GetCurrentPlayer();
	checkf(IsValid(CurrentPlayer), TEXT("Cannot end a turn without a current player"));

	ASHPlayerState* NextPlayer = ChooseNextPlayer(CurrentPlayer);
	checkf(IsValid(NextPlayer), TEXT("ChooseNextPlayer returned an invalid player"));

	while (int32* SkipCount = PendingSkippedTurns.Find(NextPlayer))
	{
		UE_LOG(LogTemp, Log,
			TEXT("[SH_SKIP_TURN] Skipping turn for %s; pending skips before consume: %d"),
			*GetNameSafe(NextPlayer),
			*SkipCount);

		--(*SkipCount);
		if (*SkipCount <= 0)
		{
			PendingSkippedTurns.Remove(NextPlayer);
		}

		NextPlayer = ChooseNextPlayer(NextPlayer);
		checkf(IsValid(NextPlayer), TEXT("ChooseNextPlayer returned an invalid player while skipping turns"));
	}

	GameState->SetCurrentPlayer(NextPlayer);
	bPairingActionUsed = false;
	GameState->SetTurnPhase(ETurnPhase::FirstPairing);
}

void UTurnComponent::RegisterPendingPairSettlement(ASHCard* CardA, ASHCard* CardB)
{
	CheckServerAuthority();
	if (GetNetMode() == NM_DedicatedServer || !IsValid(CardA) || !IsValid(CardB))
	{
		return;
	}
	PendingPairSettlements.AddUnique(FActivatedPair{CardA, CardB, false});
}

void UTurnComponent::NotifyPairSettled(ASHCard* CardA, ASHCard* CardB)
{
	CheckServerAuthority();
	PendingPairSettlements.Remove(FActivatedPair{CardA, CardB, false});
	if (ASHGameMode* GameMode = GetWorld()->GetAuthGameMode<ASHGameMode>())
	{
		GameMode->NotifyActivationPairSettled(CardA, CardB);
	}
	TryCompleteDeferredEndTurn();
}

void UTurnComponent::NotifyEffectTaskFinished()
{
	CheckServerAuthority();
	TryCompleteDeferredEndTurn();
}

void UTurnComponent::BeginTurnTransitionBlock(FName EffectId)
{
	CheckServerAuthority();
	if (!EffectId.IsNone())
	{
		++NamedTurnTransitionBlocks.FindOrAdd(EffectId);
	}
}

void UTurnComponent::FinishTurnTransitionBlock(FName EffectId)
{
	CheckServerAuthority();
	if (int32* Count = NamedTurnTransitionBlocks.Find(EffectId))
	{
		if (--(*Count) <= 0)
		{
			NamedTurnTransitionBlocks.Remove(EffectId);
		}
	}
	if (ASHGameMode* GameMode = GetWorld()->GetAuthGameMode<ASHGameMode>())
	{
		GameMode->TryProcessQueuedPairActivations();
		GameMode->FlushCompletedEffectPairs();
	}
	TryCompleteDeferredEndTurn();
}

bool UTurnComponent::HasTurnTransitionBlockers() const
{
	const ASHGameMode* GameMode = GetWorld()->GetAuthGameMode<ASHGameMode>();
	return bWaitingForAdditionalDraw || !PendingPairSettlements.IsEmpty() || !NamedTurnTransitionBlocks.IsEmpty() ||
		(IsValid(GameMode) && GameMode->HasActiveEffectTasks());
}

void UTurnComponent::TryCompleteDeferredEndTurn()
{
	if (bEndTurnRequested && !HasTurnTransitionBlockers())
	{
		EndTurn();
	}
}

void UTurnComponent::EnterTurnPhase(ETurnPhase NewPhase)
{
	if (NewPhase == ETurnPhase::FirstPairing || NewPhase == ETurnPhase::SecondPairing)
	{
		bPairingActionUsed = false;
	}

	GetSHGameState()->SetTurnPhase(NewPhase);

	if (NewPhase == ETurnPhase::FirstPairing || NewPhase == ETurnPhase::DrawCard)
	{
		UpdateForcedDrawGuidance(GetSHGameState()->GetCurrentPlayer());
	}
}

void UTurnComponent::UpdateForcedDrawGuidance(ASHPlayerState* DrawingPlayer)
{
	const ASHGameState* GameState = GetSHGameState();
	const ETurnPhase Phase = GameState->GetTurnPhase();
	if (!IsValid(DrawingPlayer) || GameState->GetCurrentPlayer() != DrawingPlayer ||
		(Phase != ETurnPhase::FirstPairing && Phase != ETurnPhase::DrawCard))
	{
		return;
	}

	FForcedDrawSourceQueue* ForcedQueue = ForcedDrawSources.Find(DrawingPlayer);
	while (ForcedQueue && !ForcedQueue->Sources.IsEmpty())
	{
		ASHHand* CandidateHand = ForcedQueue->Sources[0];
		if (IsValid(CandidateHand) && CandidateHand->GetCardCount() > 0)
		{
			break;
		}

		ForcedQueue->Sources.RemoveAt(0);
	}

	if (ForcedQueue && ForcedQueue->Sources.IsEmpty())
	{
		ForcedDrawSources.Remove(DrawingPlayer);
		ForcedQueue = nullptr;
	}

	ASHHand* ForcedHand = ForcedQueue ? ForcedQueue->Sources[0].Get() : nullptr;

	if (!IsValid(ForcedHand) || ForcedHand->GetCardCount() <= 0)
	{
		ClearDrawGuidance(DrawingPlayer);
		return;
	}

	ASHPlayerController* Controller = Cast<ASHPlayerController>(DrawingPlayer->GetOwner());
	if (IsValid(Controller))
	{
		TArray<ASHPlayerState*> ValidSources;
		for (APlayerState* State : GameState->PlayerArray)
		{
			ASHPlayerState* Candidate = Cast<ASHPlayerState>(State);
			if (IsValid(Candidate) && Candidate->GetHand() == ForcedHand)
			{
				ValidSources.Add(Candidate);
				break;
			}
		}
		Controller->ClientUpdateCardDrawGuidance(
			ValidSources,
			ECardDrawGuidanceType::ForcedSelectedPlayer);
		Controller->ClientSetGuidedDrawHands({ForcedHand});
	}
}

ASHHand* UTurnComponent::GetFirstForcedDrawSourceHand(const ASHPlayerState* DrawingPlayer) const
{
	const FForcedDrawSourceQueue* ForcedQueue = ForcedDrawSources.Find(DrawingPlayer);
	return ForcedQueue && !ForcedQueue->Sources.IsEmpty()
		? ForcedQueue->Sources[0].Get()
		: nullptr;
}

void UTurnComponent::ClearDrawGuidance(ASHPlayerState* DrawingPlayer)
{
	ASHPlayerController* Controller = IsValid(DrawingPlayer)
		? Cast<ASHPlayerController>(DrawingPlayer->GetOwner())
		: nullptr;

	if (IsValid(Controller))
	{
		Controller->ClientUpdateCardDrawGuidance({}, ECardDrawGuidanceType::None);
		Controller->ClientSetGuidedDrawHands({});
	}
}

ASHGameState* UTurnComponent::GetSHGameState() const
{
	ASHGameState* GameState = GetWorld()->GetGameState<ASHGameState>();
	checkf(IsValid(GameState), TEXT("Invalid SHGameState"));
	return GameState;
}

void UTurnComponent::CheckServerAuthority() const
{
	checkf(GetOwner() && GetOwner()->HasAuthority(), TEXT("Turns can only be controlled on the server"));
}
