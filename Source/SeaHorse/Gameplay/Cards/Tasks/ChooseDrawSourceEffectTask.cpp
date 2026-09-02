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

		TArray<ASHPlayerState*> SourceCandidates;
		for (APlayerState* PlayerState : GameState->PlayerArray)
		{
			ASHPlayerState* Candidate = Cast<ASHPlayerState>(PlayerState);
			ASHHand* CandidateHand = IsValid(Candidate) ? Candidate->GetHand() : nullptr;
			if (IsValid(Candidate) && Candidate != DrawingPlayer &&
				IsValid(CandidateHand) && CandidateHand->GetCardCount() > 0)
			{
				SourceCandidates.Add(Candidate);
			}
		}

		if (SourceCandidates.IsEmpty())
		{
			UE_LOG(LogTemp, Log,
				TEXT("[SH_CHOOSE_DRAW_SOURCE] Selected drawing player has no valid source; completing the effect"));
			FinishEffect();
			return;
		}

		RequestPlayerSelection(SourceCandidates, EPlayerSelectionPurpose::PlayerToDrawFrom);
		return;
	}

	ASHGameMode* GameMode = GetTypedOuter<ASHGameMode>();
	checkf(IsValid(GameMode), TEXT("ChooseDrawSourceEffectTask has no valid GameMode"));

	UTurnComponent* TurnComponent = GameMode->GetTurnComponent();
	checkf(IsValid(TurnComponent), TEXT("GameMode has no TurnComponent"));

	ASHHand* SelectedHand = IsValid(SelectedPlayer) ? SelectedPlayer->GetHand() : nullptr;
	if (!IsValid(SelectedHand) || SelectedHand->GetCardCount() <= 0)
	{
		UE_LOG(LogTemp, Log,
			TEXT("[SH_CHOOSE_DRAW_SOURCE] Selected source has no cards; completing the effect without forcing a source"));
		FinishEffect();
		return;
	}

	TurnComponent->SetForcedDrawSource(DrawingPlayer, SelectedPlayer);
	FinishEffect();
}
