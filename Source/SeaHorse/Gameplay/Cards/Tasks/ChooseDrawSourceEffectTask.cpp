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
		for (ASHHand* CandidateHand : GameState->GetParticipantHands())
		{
			if (IsValid(CandidateHand) && CandidateHand != DrawingPlayer->GetHand())
			{
				SourceCandidates.Add(CandidateHand);
			}
		}

		if (SourceCandidates.IsEmpty())
		{
			UE_LOG(LogTemp, Log,
				TEXT("[SH_CHOOSE_DRAW_SOURCE] Selected drawing player has no other source; completing the effect"));
			FinishEffect();
			return;
		}

		// Select a logical hand so NPC representations without a PlayerState are
		// eligible too. Empty sources are revalidated when the draw is attempted.
		RequestParticipantSelection(SourceCandidates, EPlayerSelectionPurpose::PlayerToDrawFrom);
		return;
	}
	HandleParticipantSelected(IsValid(SelectedPlayer) ? SelectedPlayer->GetHand() : nullptr);
}

void UChooseDrawSourceEffectTask::HandleParticipantSelected(ASHHand* SelectedHand)
{
	ASHGameMode* GameMode = GetTypedOuter<ASHGameMode>();
	checkf(IsValid(GameMode), TEXT("ChooseDrawSourceEffectTask has no valid GameMode"));

	UTurnComponent* TurnComponent = GameMode->GetTurnComponent();
	checkf(IsValid(TurnComponent), TEXT("GameMode has no TurnComponent"));

	if (IsValid(DrawingPlayer) && IsValid(SelectedHand) && SelectedHand != DrawingPlayer->GetHand())
	{
		PlayActivationVFX();
		TurnComponent->SetForcedDrawSourceHand(DrawingPlayer, SelectedHand);
	}
	FinishEffect();
}

