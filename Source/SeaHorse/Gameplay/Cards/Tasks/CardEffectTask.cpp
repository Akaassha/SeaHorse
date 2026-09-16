// Fill out your copyright notice in the Description page of Project Settings.


#include "SeaHorse/Gameplay/Cards/Tasks/CardEffectTask.h"
#include "SeaHorse/Gameplay/Cards/SHCard.h"
#include "SeaHorse/Gameplay/Core/SHPlayerState.h"
#include "SeaHorse/Gameplay/Core/SHGameMode.h"
#include "SeaHorse/Gameplay/SHHand.h"
#include "SeaHorse/Gameplay/Cards/CardDefinition.h"
#include "SeaHorse/Gameplay/Cards/Fragments/CardEffectFragment.h"

void UCardEffectTask::Initialize(ASHPlayerState* InActivatingPlayer, ASHCard* InCardA, ASHCard* InCardB,
    FName InEffectPresentationId)
{
	bFinished = false;
	bActivationVFXStarted = false;
	bGameplayCommitted = false;
    ActivatingPlayer = InActivatingPlayer;
    CardA = InCardA;
    CardB = InCardB;
    EffectPresentationId = InEffectPresentationId;
}

void UCardEffectTask::RequestParticipantSelection(
    const TArray<ASHHand*>& Candidates,
    EPlayerSelectionPurpose Purpose)
{
	if (bFinished)
	{
		return;
	}
    ASHGameMode* GameMode = GetTypedOuter<ASHGameMode>();
    checkf(IsValid(GameMode), TEXT("CardEffectTask has no valid GameMode"));
    GameMode->RequestParticipantSelection(this, ActivatingPlayer, Candidates, Purpose);
}


void UCardEffectTask::StartEffect_Implementation()
{

}

void UCardEffectTask::FinishEffect()
{
	if (bFinished)
	{
		return;
	}
	bFinished = true;

    ASHGameMode* GameMode = GetTypedOuter<ASHGameMode>();
    checkf(IsValid(GameMode), TEXT("CardEffectTask has no valid GameMode"));

    GameMode->FinishEffectTask(this);
}

void UCardEffectTask::PlayActivationVFX()
{
    if (bFinished || bActivationVFXStarted || !IsValid(ActivatingPlayer) || !IsValid(CardA) || !IsValid(CardB)) { return; }
    ASHHand* Hand = ActivatingPlayer->GetHand();
    const UCardEffectFragment* Fragment = Cast<UCardEffectFragment>(
        UCardDefinition::FindFragmentByClass(CardA->GetCardDefinition(), UCardEffectFragment::StaticClass()));
    if (!IsValid(Hand) || !Hand->HasAuthority() || !IsValid(Fragment) || !Fragment->ActivationVFX) { return; }
    bActivationVFXStarted = true;
    Hand->MulticastPlayActivationVFX(CardA, CardB, Fragment->ActivationVFX, Fragment->ActivationVFXDuration);
}

bool UCardEffectTask::CancelPendingTargetSelection()
{
	if (bFinished || bGameplayCommitted || bActivationVFXStarted || !RequiresTargetSelection()) { return false; }
	bFinished = true;
	return true;
}

ECardEffectPairDisposition UCardEffectTask::GetPairDisposition_Implementation() const
{
	return ECardEffectPairDisposition::MoveToVictoryStack;
}

void UCardEffectTask::RequestPlayerSelection(
    const TArray<ASHPlayerState*>& Candidates,
    EPlayerSelectionPurpose Purpose)
{
	if (bFinished)
	{
		return;
	}
    ASHGameMode* GameMode = GetTypedOuter<ASHGameMode>();
    checkf(IsValid(GameMode), TEXT("CardEffectTask has no valid GameMode"));
    GameMode->RequestPlayerSelection(this, ActivatingPlayer, Candidates, Purpose);
}

void UCardEffectTask::HandlePlayerSelected(ASHPlayerState* SelectedPlayer)
{
    checkNoEntry();
}

bool UCardEffectTask::RequestHandCardSelection(const TArray<ASHCard*>& Candidates)
{
    ASHGameMode* Mode = GetTypedOuter<ASHGameMode>();
    return !bFinished && IsValid(Mode) && Mode->RequestHandCardSelection(this, ActivatingPlayer, Candidates);
}

void UCardEffectTask::HandleParticipantSelected(ASHHand* SelectedHand)
{
    checkNoEntry();
}

bool UCardEffectTask::RequestActivationPairSelection(const TArray<ASHCard*>& CandidateCards)
{
	if (bFinished)
	{
		return false;
	}
    ASHGameMode* GameMode = GetTypedOuter<ASHGameMode>();
    checkf(IsValid(GameMode), TEXT("CardEffectTask has no valid GameMode"));
    return GameMode->RequestActivationPairSelection(this, ActivatingPlayer, CandidateCards);
}

void UCardEffectTask::HandleActivationPairSelected(
    ASHPlayerState* PairOwner, ASHCard* SelectedCardA, ASHCard* SelectedCardB)
{
    checkNoEntry();
}
