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
	if (!IsValid(DrawingPlayer) || !IsValid(SourcePlayer) || DrawingPlayer == SourcePlayer)
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

	ASHHand* SourceHand = SourcePlayer->GetHand();
	if (!IsValid(SourceHand) || SourceHand->GetCardCount() <= 0)
	{
		return false;
	}

	if (bWaitingForAdditionalDraw)
	{
		if (DrawingPlayer != AdditionalDrawPlayer || !IsValid(FirstDrawSource))
		{
			return false;
		}

		return AdditionalDrawSourceRule == EAdditionalDrawSourceRule::SamePlayer
			? SourcePlayer == FirstDrawSource
			: SourcePlayer != FirstDrawSource;
	}

	if (ASHPlayerState* ForcedSource = GetFirstForcedDrawSource(DrawingPlayer))
	{
		if (SourcePlayer != ForcedSource)
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
	CheckServerAuthority();
	checkf(IsValid(DrawingPlayer) && IsValid(SourcePlayer), TEXT("Invalid card draw notification"));

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
		FirstDrawSource = SourcePlayer;
		bWaitingForAdditionalDraw = true;

		TArray<ASHPlayerState*> ValidSources;
		for (APlayerState* PlayerState : GetSHGameState()->PlayerArray)
		{
			ASHPlayerState* Candidate = Cast<ASHPlayerState>(PlayerState);
			if (CanDrawCard(DrawingPlayer, Candidate))
			{
				ValidSources.Add(Candidate);
			}
		}

		if (ValidSources.IsEmpty())
		{
			UE_LOG(LogTemp, Log,
				TEXT("[SH_ADDITIONAL_DRAW] No valid source for the second draw; completing the effect after the first draw"));
			FinishAdditionalDraw();
			return;
		}

		ASHPlayerController* Controller = Cast<ASHPlayerController>(DrawingPlayer->GetOwner());
		if (IsValid(Controller))
		{
			const ECardDrawGuidanceType GuidanceType =
				AdditionalDrawSourceRule == EAdditionalDrawSourceRule::SamePlayer
				? ECardDrawGuidanceType::AdditionalFromSamePlayer
				: ECardDrawGuidanceType::AdditionalFromDifferentPlayer;

			Controller->ClientUpdateCardDrawGuidance(ValidSources, GuidanceType);
		}
		return;
	}

	ClearDrawGuidance(DrawingPlayer);
	CompleteCurrentPhase(ETurnPhaseEndReason::CardDrawn);
}

void UTurnComponent::FinishAdditionalDraw()
{
	UCardEffectTask* CompletedEffectTask = AdditionalDrawEffectTask;
	AdditionalDrawPlayer = nullptr;
	FirstDrawSource = nullptr;
	AdditionalDrawEffectTask = nullptr;
	bWaitingForAdditionalDraw = false;
	EnterTurnPhase(ETurnPhase::SecondPairing);

	if (IsValid(CompletedEffectTask))
	{
		CompletedEffectTask->FinishEffect();
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
	checkf(!IsValid(AdditionalDrawPlayer), TEXT("An additional draw is already scheduled"));

	AdditionalDrawPlayer = PlayerState;
	AdditionalDrawEffectTask = EffectTask;
	AdditionalDrawSourceRule = SourceRule;
}

void UTurnComponent::SetForcedDrawSource(ASHPlayerState* DrawingPlayer, ASHPlayerState* SourcePlayer)
{
	CheckServerAuthority();
	checkf(IsValid(DrawingPlayer) && IsValid(SourcePlayer) && DrawingPlayer != SourcePlayer,
		TEXT("Invalid forced draw participants"));

	ForcedDrawSources.FindOrAdd(DrawingPlayer).Sources.Add(SourcePlayer);
	UpdateForcedDrawGuidance(DrawingPlayer);
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
	const int32 PlayerCount = GameState->PlayerArray.Num();
	checkf(PlayerCount > 0, TEXT("Cannot choose the next player without players"));

	const int32 NextSeatIndex = (CurrentPlayer->GetSeatIndex() + 1) % PlayerCount;
	for (APlayerState* PlayerState : GameState->PlayerArray)
	{
		ASHPlayerState* SHPlayerState = Cast<ASHPlayerState>(PlayerState);
		if (IsValid(SHPlayerState) && SHPlayerState->GetSeatIndex() == NextSeatIndex)
		{
			return SHPlayerState;
		}
	}

	return nullptr;
}

void UTurnComponent::EndTurn()
{
	checkf(!bWaitingForAdditionalDraw, TEXT("Cannot end turn while an additional draw is pending"));
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

	GameState->SetCurrentPlayer(NextPlayer);
	bPairingActionUsed = false;
	GameState->SetTurnPhase(ETurnPhase::FirstPairing);
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
		ASHPlayerState* Candidate = ForcedQueue->Sources[0];
		ASHHand* CandidateHand = IsValid(Candidate) ? Candidate->GetHand() : nullptr;
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

	ASHPlayerState* ForcedSource = ForcedQueue ? ForcedQueue->Sources[0].Get() : nullptr;
	ASHHand* ForcedHand = IsValid(ForcedSource) ? ForcedSource->GetHand() : nullptr;

	if (!IsValid(ForcedHand) || ForcedHand->GetCardCount() <= 0)
	{
		ClearDrawGuidance(DrawingPlayer);
		return;
	}

	ASHPlayerController* Controller = Cast<ASHPlayerController>(DrawingPlayer->GetOwner());
	if (IsValid(Controller))
	{
		TArray<ASHPlayerState*> ValidSources;
		ValidSources.Add(ForcedSource);
		Controller->ClientUpdateCardDrawGuidance(
			ValidSources,
			ECardDrawGuidanceType::ForcedSelectedPlayer);
	}
}

ASHPlayerState* UTurnComponent::GetFirstForcedDrawSource(const ASHPlayerState* DrawingPlayer) const
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
