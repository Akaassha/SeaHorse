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
			ASHPlayerState* SourcePlayer = Cast<ASHPlayerState>(PlayerState);
			ASHHand* CandidateHand = IsValid(SourcePlayer) ? SourcePlayer->GetHand() : nullptr;
			if (IsValid(SourcePlayer) && SourcePlayer != DrawingPlayer &&
				IsValid(CandidateHand) && !CandidateHand->IsLogicalNPC())
			{
				SourceCandidates.Add(SourcePlayer);
			}
		}

		if (SourceCandidates.IsEmpty())
		{
			UE_LOG(LogTemp, Log,
				TEXT("[SH_CHOOSE_DRAW_SOURCE] Selected drawing player has no other human source; completing the effect"));
			FinishEffect();
			return;
		}

		// This is another player choice, so keep using the world-space player
		// representations instead of requiring a click on one of their cards.
		// Empty hands remain selectable: forced-draw validation handles a source
		// that is still empty when that player's draw is actually attempted.
		RequestPlayerSelection(SourceCandidates, EPlayerSelectionPurpose::PlayerToDrawFrom);
		return;
	}

	ASHGameMode* GameMode = GetTypedOuter<ASHGameMode>();
	checkf(IsValid(GameMode), TEXT("ChooseDrawSourceEffectTask has no valid GameMode"));

	UTurnComponent* TurnComponent = GameMode->GetTurnComponent();
	checkf(IsValid(TurnComponent), TEXT("GameMode has no TurnComponent"));

	ASHHand* SelectedHand = IsValid(SelectedPlayer) ? SelectedPlayer->GetHand() : nullptr;
	if (IsValid(SelectedPlayer) && SelectedPlayer != DrawingPlayer &&
		IsValid(SelectedHand) && !SelectedHand->IsLogicalNPC())
	{
		TurnComponent->SetForcedDrawSource(DrawingPlayer, SelectedPlayer);
	}
	FinishEffect();
}

