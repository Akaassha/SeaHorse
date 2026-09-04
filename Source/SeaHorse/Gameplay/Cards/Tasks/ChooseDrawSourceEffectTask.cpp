#include "SeaHorse/Gameplay/Cards/Tasks/ChooseDrawSourceEffectTask.h"

#include "SeaHorse/Gameplay/Components/TurnComponent.h"
#include "SeaHorse/Gameplay/Core/SHGameMode.h"
#include "SeaHorse/Gameplay/Core/SHGameState.h"
#include "SeaHorse/Gameplay/Core/SHPlayerState.h"
#include "SeaHorse/Gameplay/SHHand.h"

void UChooseDrawSourceEffectTask::StartEffect_Implementation()
{
	const ASHGameState* GameState = GetWorld()->GetGameState<ASHGameState>();
	checkf(IsValid(GameState), TEXT("Invalid SHGameState"));

	TArray<ASHPlayerState*> Candidates;
	for (APlayerState* PlayerState : GameState->PlayerArray)
	{
		if (ASHPlayerState* Candidate = Cast<ASHPlayerState>(PlayerState); IsValid(Candidate))
		{
			Candidates.Add(Candidate);
		}
	}

	RequestPlayerSelection(Candidates, EPlayerSelectionPurpose::PlayerWhoWillDraw);
}

void UChooseDrawSourceEffectTask::HandlePlayerSelected(ASHPlayerState* SelectedPlayer)
{
	if (!IsValid(DrawingPlayer))
	{
		DrawingPlayer = SelectedPlayer;

		const ASHGameState* GameState = GetWorld()->GetGameState<ASHGameState>();
		checkf(IsValid(GameState), TEXT("Invalid SHGameState"));

		TArray<ASHHand*> SourceCandidates;
		for (APlayerState* PlayerState : GameState->PlayerArray)
		{
			const ASHPlayerState* SourcePlayer = Cast<ASHPlayerState>(PlayerState);
			ASHHand* CandidateHand = IsValid(SourcePlayer) ? SourcePlayer->GetHand() : nullptr;
			if (IsValid(CandidateHand) && CandidateHand != DrawingPlayer->GetHand() &&
				CandidateHand->GetCardCount() > 0)
			{
				SourceCandidates.Add(CandidateHand);
			}
		}

		if (SourceCandidates.IsEmpty())
		{
			UE_LOG(LogTemp, Log,
				TEXT("[SH_CHOOSE_DRAW_SOURCE] Selected drawing player has no valid source; completing the effect"));
			FinishEffect();
			return;
		}

		RequestParticipantSelection(SourceCandidates, EPlayerSelectionPurpose::PlayerToDrawFrom);
		return;
	}

	// The second step is hand-based and is handled by HandleParticipantSelected.
	FinishEffect();
}

void UChooseDrawSourceEffectTask::HandleParticipantSelected(ASHHand* SelectedHand)
{
	ASHGameMode* GameMode = GetTypedOuter<ASHGameMode>();
	checkf(IsValid(GameMode), TEXT("ChooseDrawSourceEffectTask has no valid GameMode"));

	UTurnComponent* TurnComponent = GameMode->GetTurnComponent();
	checkf(IsValid(TurnComponent), TEXT("GameMode has no TurnComponent"));

	if (!IsValid(SelectedHand) || SelectedHand->IsLogicalNPC() || SelectedHand->GetCardCount() <= 0)
	{
		UE_LOG(LogTemp, Log,
			TEXT("[SH_CHOOSE_DRAW_SOURCE] Selected source is invalid, empty, or belongs to an NPC; completing the effect without forcing a source"));
		FinishEffect();
		return;
	}

	TurnComponent->SetForcedDrawSourceHand(DrawingPlayer, SelectedHand);
	FinishEffect();
}
