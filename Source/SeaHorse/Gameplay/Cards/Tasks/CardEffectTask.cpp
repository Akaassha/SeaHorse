// Fill out your copyright notice in the Description page of Project Settings.


#include "SeaHorse/Gameplay/Cards/Tasks/CardEffectTask.h"
#include "SeaHorse/Gameplay/Cards/SHCard.h"
#include "SeaHorse/Gameplay/Core/SHPlayerState.h"
#include "SeaHorse/Gameplay/Core/SHGameMode.h"

void UCardEffectTask::Initialize(ASHPlayerState* InActivatingPlayer, ASHCard* InCardA, ASHCard* InCardB)
{
    ActivatingPlayer = InActivatingPlayer;
    CardA = InCardA;
    CardB = InCardB;
}


void UCardEffectTask::StartEffect_Implementation()
{

}

void UCardEffectTask::FinishEffect()
{
    ASHGameMode* GameMode = GetTypedOuter<ASHGameMode>();
    checkf(IsValid(GameMode), TEXT("CardEffectTask has no valid GameMode"));

    GameMode->FinishEffectTask(this);
}

void UCardEffectTask::RequestPlayerSelection(
    const TArray<ASHPlayerState*>& Candidates,
    EPlayerSelectionPurpose Purpose)
{
    ASHGameMode* GameMode = GetTypedOuter<ASHGameMode>();
    checkf(IsValid(GameMode), TEXT("CardEffectTask has no valid GameMode"));
    GameMode->RequestPlayerSelection(this, ActivatingPlayer, Candidates, Purpose);
}

void UCardEffectTask::HandlePlayerSelected(ASHPlayerState* SelectedPlayer)
{
    checkNoEntry();
}
