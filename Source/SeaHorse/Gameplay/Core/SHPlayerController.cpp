// Fill out your copyright notice in the Description page of Project Settings.


#include "SeaHorse/Gameplay/Core/SHPlayerController.h"
#include "SeaHorse/Gameplay/Core/SHGameState.h"
#include "SeaHorse/Gameplay/Core/SHPlayerState.h"
#include "SeaHorse/Gameplay/SHHand.h"
#include "SeaHorse/Gameplay/Cards/CardDefinition.h"
#include "SeaHorse/Gameplay/Cards/SHCard.h"
#include "SeaHorse/Gameplay/Board/SHTable.h"

#include "Kismet/GameplayStatics.h"

void ASHPlayerController::SetupTableView()
{
    ASHGameState* SHGameState = GetWorld()->GetGameState<ASHGameState>();
    checkf(IsValid(SHGameState), TEXT("Invalid SHGameState"));

    ASHPlayerState* LocalPlayerState = GetPlayerState<ASHPlayerState>();
    checkf(IsValid(LocalPlayerState), TEXT("Invalid local PlayerState"));

    const int32 PlayerCount = SHGameState->PlayerArray.Num();

    ASHTable* Table = Cast<ASHTable>(UGameplayStatics::GetActorOfClass(GetWorld(), ASHTable::StaticClass()));

    checkf(IsValid(Table), TEXT("No SHTable found in level"));

    for (APlayerState* CurrentPlayerState : SHGameState->PlayerArray)
    {
        ASHPlayerState* SHPlayerState = Cast<ASHPlayerState>(CurrentPlayerState);

        checkf(IsValid(SHPlayerState),TEXT("PlayerState is not ASHPlayerState"));

        ASHHand* Hand = SHPlayerState->GetHand();

        checkf(IsValid(Hand), TEXT("Player %s has no Hand"), *GetNameSafe(SHPlayerState));

        const int32 VisualSeatIndex = GetVisualSeatIndex(SHPlayerState->GetSeatIndex(), PlayerCount);

        USceneComponent* HandRoot = Table->GetHandRoot(PlayerCount, VisualSeatIndex);

        checkf(IsValid(HandRoot), TEXT("No HandRoot for PlayerCount %d, VisualSeatIndex %d"), PlayerCount, VisualSeatIndex);

        Hand->SetActorTransform(HandRoot->GetComponentTransform());

        UE_LOG(
            LogTemp,
            Warning,
            TEXT("LocalSeat: %d | Player: %s | LogicalSeat: %d -> VisualSeat: %d | HandRoot: %s"),
            LocalPlayerState->GetSeatIndex(),
            *SHPlayerState->GetPlayerName(),
            SHPlayerState->GetSeatIndex(),
            VisualSeatIndex,
            *GetNameSafe(HandRoot)
        );

        Hand->UpdateCardPositions();
    }
}

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

int32 ASHPlayerController::GetVisualSeatIndex(
    int32 PlayerSeatIndex,
    int32 PlayerCount) const
{
    ASHPlayerState* LocalPlayerState = GetPlayerState<ASHPlayerState>();

    checkf(IsValid(LocalPlayerState), TEXT("Invalid local PlayerState"));

    const int32 LocalSeatIndex = LocalPlayerState->GetSeatIndex();

    return (PlayerSeatIndex - LocalSeatIndex + PlayerCount) % PlayerCount;
}