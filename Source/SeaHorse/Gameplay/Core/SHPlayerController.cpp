// Fill out your copyright notice in the Description page of Project Settings.


#include "SeaHorse/Gameplay/Core/SHPlayerController.h"
#include "SeaHorse/Gameplay/Core/SHGameState.h"
#include "SeaHorse/Gameplay/Core/SHPlayerState.h"
#include "SeaHorse/Gameplay/SHHand.h"
#include "SeaHorse/Gameplay/Cards/CardDefinition.h"
#include "SeaHorse/Gameplay/Cards/SHCard.h"

void ASHPlayerController::DebugHands()
{
    ASHGameState* SHGameState = GetWorld()->GetGameState<ASHGameState>();

    if (!IsValid(SHGameState))
    {
        return;
    }

    UE_LOG(LogTemp, Warning, TEXT("=== HANDS ON %s ==="), *GetName());

    for (APlayerState* CurrentPlayerState : SHGameState->PlayerArray)
    {
        ASHPlayerState* SHPlayerState = Cast<ASHPlayerState>(CurrentPlayerState);

        if (!IsValid(SHPlayerState))
        {
            continue;
        }

        UE_LOG(
            LogTemp,
            Warning,
            TEXT("PlayerId: %d | PlayerState: %s | Hand: %s"),
            SHPlayerState->GetPlayerId(),
            *GetNameSafe(SHPlayerState),
            *GetNameSafe(SHPlayerState->GetHand())
        );
    }
}

void ASHPlayerController::DebugCardDefinitions()
{
    const ASHGameState* SHGameState = GetWorld()->GetGameState<ASHGameState>();
    checkf(IsValid(SHGameState), TEXT("Invalid SHGameState"));

    const ASHPlayerState* LocalPlayerState = GetPlayerState<ASHPlayerState>();
    checkf(IsValid(LocalPlayerState), TEXT("Invalid local SHPlayerState"));

    UE_LOG(
        LogTemp,
        Warning,
        TEXT("=== LOCAL PLAYER ID: %d ==="),
        LocalPlayerState->GetPlayerId()
    );

    for (APlayerState* CurrentPlayerState : SHGameState->PlayerArray)
    {
        ASHPlayerState* SHPlayerState = Cast<ASHPlayerState>(CurrentPlayerState);

        if (!IsValid(SHPlayerState))
        {
            continue;
        }

        ASHHand* Hand = SHPlayerState->GetHand();

        if (!IsValid(Hand))
        {
            continue;
        }

        UE_LOG(
            LogTemp,
            Warning,
            TEXT("Hand owner PlayerId: %d"),
            SHPlayerState->GetPlayerId()
        );

        TArray<ASHCard*> Cards = Hand->GetCards();

        for (int32 Index = 0; Index < Cards.Num(); ++Index)
        {
            ASHCard* Card = Cards[Index];

            if (!IsValid(Card))
            {
                UE_LOG(LogTemp, Warning, TEXT("[%d] Card: None"), Index);
                continue;
            }

            TSubclassOf<UCardDefinition> Definition = Card->GetCardDefinition();

            UE_LOG(
                LogTemp,
                Warning,
                TEXT("[%d] Card: %s | Definition: %s"),
                Index,
                *GetNameSafe(Card),
                *GetNameSafe(Definition.Get())
            );
        }
    }
}
