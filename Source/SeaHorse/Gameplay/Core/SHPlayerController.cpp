// Fill out your copyright notice in the Description page of Project Settings.


#include "SeaHorse/Gameplay/Core/SHPlayerController.h"
#include "SeaHorse/Gameplay/Board/VictoryStack.h"
#include "SeaHorse/Gameplay/Core/SHGameState.h"
#include "SeaHorse/Gameplay/Core/SHPlayerState.h"
#include "SeaHorse/Gameplay/SHHand.h"
#include "SeaHorse/Gameplay/Cards/CardDefinition.h"
#include "SeaHorse/Gameplay/Cards/SHCard.h"
#include "SeaHorse/Gameplay/Board/SHTable.h"
#include "Kismet/GameplayStatics.h"
#include "SeaHorse/Gameplay/Core/SHGameMode.h"
#include "SeaHorse/Gameplay/Cards/Fragments/CardEffectFragment.h"

void ASHPlayerController::TrySetupTableView()
{
    UE_LOG(LogTemp, Warning,
        TEXT("[SH_INIT][%.3f][PC:%s] TryInitialTableSetup BEGIN | Initialized=%d"),
        GetWorld()->GetTimeSeconds(),
        *GetNameSafe(this),
        bTableViewInitialized);

    if (bTableViewInitialized)
    {
        UE_LOG(LogTemp, Warning,
            TEXT("[SH_INIT] -> SKIP: already initialized"));
        return;
    }

    if (!IsLocalController())
    {
        UE_LOG(LogTemp, Warning,
            TEXT("[SH_INIT] -> WAIT: not local controller"));
        return;
    }

    ASHGameState* SHGameState = GetWorld()->GetGameState<ASHGameState>();

    if (!IsValid(SHGameState))
    {
        UE_LOG(LogTemp, Warning,
            TEXT("[SH_INIT] -> WAIT: no GameState"));
        return;
    }

    if (!SHGameState->IsMatchReady())
    {
        UE_LOG(LogTemp, Warning,
            TEXT("[SH_INIT] -> WAIT: MatchReady=false"));
        return;
    }

    UE_LOG(LogTemp, Warning,
        TEXT("[SH_INIT] MatchReady=true | Players=%d | ExpectedCards=%d"),
        SHGameState->PlayerArray.Num(),
        SHGameState->GetInitialDealtCardCount());

    ASHPlayerState* LocalPlayerState = GetPlayerState<ASHPlayerState>();

    if (!IsValid(LocalPlayerState) ||
        LocalPlayerState->GetSeatIndex() == INDEX_NONE)
    {
        return;
    }

    if (SHGameState->PlayerArray.IsEmpty())
    {
        return;
    }

    int32 ReceivedCardCount = 0;

    for (APlayerState* CurrentPlayerState : SHGameState->PlayerArray)
    {
        ASHPlayerState* SHPlayerState = Cast<ASHPlayerState>(CurrentPlayerState);

        if (!IsValid(SHPlayerState))
        {
            UE_LOG(LogTemp, Warning,
                TEXT("[SH_INIT] -> WAIT: invalid PlayerState"));
            return;
        }

        if (SHPlayerState->GetSeatIndex() == INDEX_NONE)
        {
            UE_LOG(LogTemp, Warning,
                TEXT("[SH_INIT] -> WAIT: %s has no SeatIndex"), *GetNameSafe(SHPlayerState));
            return;
        }

        ASHHand* Hand = SHPlayerState->GetHand();

        if (!IsValid(Hand))
        {
            UE_LOG(LogTemp, Warning,
                TEXT("[SH_INIT] -> WAIT: %s has no Hand"),
                *GetNameSafe(SHPlayerState));
            return;
        }

        int32 ValidCards = 0;

        for (ASHCard* Card : Hand->GetCards())
        {
            if (IsValid(Card))
            {
                ++ValidCards;
            }
        }


        UE_LOG(LogTemp, Warning,
            TEXT("[SH_INIT] Player=%s LogicalSeat=%d Hand=%s LayoutSeat=%d Cards=%d Valid=%d"),
            *GetNameSafe(SHPlayerState),
            SHPlayerState->GetSeatIndex(),
            *GetNameSafe(Hand),
            Hand->GetLayoutSeatIndex(),
            Hand->GetCardCount(),
            ValidCards);


        if (ValidCards != Hand->GetCardCount())
        {
            UE_LOG(LogTemp, Warning,
                TEXT("[SH_INIT] -> WAIT: unresolved Card actors"));
            return;
        }

        ReceivedCardCount += Hand->GetCardCount();
    }

    UE_LOG(LogTemp, Warning,
        TEXT("[SH_INIT] ReceivedCards=%d ExpectedCards=%d"),
        ReceivedCardCount,
        SHGameState->GetInitialDealtCardCount());

    if (ReceivedCardCount != SHGameState->GetInitialDealtCardCount())
    {
        UE_LOG(LogTemp, Warning,
            TEXT("[SH_INIT] -> WAIT: incomplete initial deal"));
        return;
    }

    UE_LOG(LogTemp, Warning,
        TEXT("[SH_INIT][%.3f] >>> READY - calling SetupTableView"),
        GetWorld()->GetTimeSeconds());

    SetupTableView();
    bTableViewInitialized = true;

    UE_LOG(LogTemp, Warning,
        TEXT("[SH_INIT][%.3f] <<< TABLE INITIALIZED"),
        GetWorld()->GetTimeSeconds());
}

void ASHPlayerController::ServerSkipCurrentPhase_Implementation()
{
    ASHPlayerState* SHPlayerState = GetPlayerState<ASHPlayerState>();

    if (!IsValid(SHPlayerState))
    {
        return;
    }

    ASHGameMode* SHGameMode =
        GetWorld()->GetAuthGameMode<ASHGameMode>();

    if (!IsValid(SHGameMode))
    {
        return;
    }

    SHGameMode->SkipCurrentPhase(SHPlayerState);
}

ASHHand* ASHPlayerController::FindVisualHandForLogicalHand(const ASHHand* LogicalHand) const
{
    if (!IsValid(LogicalHand))
    {
        return nullptr;
    }

    ASHGameState* GameState =
        GetWorld()->GetGameState<ASHGameState>();

    if (!IsValid(GameState))
    {
        return nullptr;
    }

    for (APlayerState* CurrentPlayerState : GameState->PlayerArray)
    {
        ASHPlayerState* SHPlayerState =
            Cast<ASHPlayerState>(CurrentPlayerState);

        if (!IsValid(SHPlayerState) ||
            SHPlayerState->GetHand() != LogicalHand)
        {
            continue;
        }

        return FindVisualHandForPlayer(SHPlayerState);
    }

    return nullptr;
}

void ASHPlayerController::SetupTableView()
{
    ASHGameState* SHGameState =
        GetWorld()->GetGameState<ASHGameState>();

    checkf(IsValid(SHGameState), TEXT("Invalid SHGameState"));

    ASHPlayerState* LocalPlayerState =
        GetPlayerState<ASHPlayerState>();

    checkf(IsValid(LocalPlayerState), TEXT("Invalid local PlayerState"));

    const int32 PlayerCount = SHGameState->PlayerArray.Num();

    TArray<ASHHand*> HandsToUpdate;

    for (APlayerState* CurrentPlayerState : SHGameState->PlayerArray)
    {
        ASHPlayerState* SHPlayerState =
            CastChecked<ASHPlayerState>(CurrentPlayerState);

        const int32 VisualSeatIndex =
            GetVisualSeatIndex(
                SHPlayerState->GetSeatIndex(),
                PlayerCount
            );

        ASHHand* VisualHand =
            FindLayoutHand(VisualSeatIndex);

        checkf(
            IsValid(VisualHand),
            TEXT("No Hand for VisualSeatIndex %d"),
            VisualSeatIndex
        );

        
        VisualHand->SetRepresentedPlayerState(SHPlayerState);

        const bool bIsLocalPlayer =
            SHPlayerState == LocalPlayerState;

        VisualHand->SetShowCardFronts(bIsLocalPlayer);

        UE_LOG(
            LogTemp,
            Warning,
            TEXT("[HAND VIEW] VisualSeat=%d Hand=%s Player=%s%s"),
            VisualSeatIndex,
            *GetNameSafe(VisualHand),
            *GetNameSafe(SHPlayerState),
            SHPlayerState == LocalPlayerState
            ? TEXT(" [LOCAL]")
            : TEXT("")
        );

        HandsToUpdate.Add(VisualHand);

    }

    for (ASHHand* Hand : HandsToUpdate)
    {
        Hand->Initialize();
        Hand->UpdateCardPositions();
    }

}

void ASHPlayerController::BeginPlay()
{
    Super::BeginPlay();

}

void ASHPlayerController::DebugHands()
{
    ASHGameState* SHGameState = GetWorld()->GetGameState<ASHGameState>();

    if (!IsValid(SHGameState))
    {
        return;
    }

    for (APlayerState* CurrentPlayerState : SHGameState->PlayerArray)
    {
        ASHPlayerState* SHPlayerState = Cast<ASHPlayerState>(CurrentPlayerState);

        if (!IsValid(SHPlayerState))
        {
            continue;
        }

    }
}

void ASHPlayerController::DebugCardDefinitions()
{
    const ASHGameState* SHGameState = GetWorld()->GetGameState<ASHGameState>();
    checkf(IsValid(SHGameState), TEXT("Invalid SHGameState"));

    const ASHPlayerState* LocalPlayerState = GetPlayerState<ASHPlayerState>();
    checkf(IsValid(LocalPlayerState), TEXT("Invalid local SHPlayerState"));

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

        TArray<ASHCard*> Cards = Hand->GetCards();

        for (int32 Index = 0; Index < Cards.Num(); ++Index)
        {
            ASHCard* Card = Cards[Index];

            if (!IsValid(Card))
            {
                continue;
            }

            TSubclassOf<UCardDefinition> Definition = Card->GetCardDefinition();

        }
    }
}

int32 ASHPlayerController::GetVisualSeatIndex(int32 PlayerSeatIndex, int32 PlayerCount) const
{
    ASHPlayerState* LocalPlayerState = GetPlayerState<ASHPlayerState>();

    checkf(IsValid(LocalPlayerState), TEXT("Invalid local PlayerState"));

    const int32 LocalSeatIndex = LocalPlayerState->GetSeatIndex();

    return (PlayerSeatIndex - LocalSeatIndex + PlayerCount) % PlayerCount;
}

ASHHand* ASHPlayerController::FindLayoutHand(int32 LayoutSeatIndex) const
{
    TArray<AActor*> Hands;

    UGameplayStatics::GetAllActorsOfClass(
        GetWorld(),
        ASHHand::StaticClass(),
        Hands
    );

    for (AActor* Actor : Hands)
    {
        ASHHand* Hand = Cast<ASHHand>(Actor);

        if (IsValid(Hand) &&
            Hand->GetLayoutSeatIndex() == LayoutSeatIndex)
        {
            return Hand;
        }
    }

    return nullptr;
}

void ASHPlayerController::ClientReceiveCardDefinition_Implementation(ASHCard* Card, TSubclassOf<UCardDefinition> CardDefinition)
{
    if (!IsValid(Card) || !CardDefinition)
    {
        return;
    }

    Card->ApplyOwnerCardDefinition(CardDefinition);
}

ASHHand* ASHPlayerController::FindVisualHandForPlayer(const ASHPlayerState* InPlayerState) const
{
    if (!IsValid(InPlayerState))
    {
        return nullptr;
    }

    TArray<AActor*> Hands;

    UGameplayStatics::GetAllActorsOfClass(
        GetWorld(),
        ASHHand::StaticClass(),
        Hands
    );

    for (AActor* Actor : Hands)
    {
        ASHHand* Hand = Cast<ASHHand>(Actor);

        if (IsValid(Hand) &&
            Hand->GetRepresentedPlayerState() == InPlayerState)
        {
            return Hand;
        }
    }

    return nullptr;
}

void ASHPlayerController::ServerActivateStoredPair_Implementation(ASHCard* Card)
{


    if (!IsValid(Card))
    {
        return;
    }

    if (Card->GetCardZone() != ECardZone::Activation)
    {
        return;
    }

    ASHPlayerState* SHPlayerState = GetPlayerState<ASHPlayerState>();
    if (!IsValid(SHPlayerState))
    {
        return;
    }

    ASHHand* Hand = SHPlayerState->GetHand();
    if (!IsValid(Hand))
    {
        return;
    }

    FActivatedPair* Pair = Hand->FindActivationPair(Card);

    if (!Pair)
    {
        return;
    }

    if (Pair->bActivated)
    {
        return;
    }

    ASHGameMode* SHGameMode = GetWorld()->GetAuthGameMode<ASHGameMode>();

    if (!SHGameMode)
    {
        return;
    }

    ASHCard* CardA = Pair->CardA;
    ASHCard* CardB = Pair->CardB;

    if (!IsValid(CardA) || !IsValid(CardB))
    {
        return;
    }

    Pair->bActivated = true;
    Hand->MulticastPairActivated(CardA, CardB);

    SHGameMode->CardActivateEffect(SHPlayerState, CardA, CardB);
    
}

void ASHPlayerController::ServerCreatePair_Implementation(ASHCard* CardA, ASHCard* CardB)
{
    if (!IsValid(CardA) || !IsValid(CardB) || CardA == CardB)
    {
        return;
    }

    ASHPlayerState* SHPlayerState = GetPlayerState<ASHPlayerState>();
    if (!IsValid(SHPlayerState))
    {
        return;
    }

    ASHHand* Hand = SHPlayerState->GetHand();
    if (!IsValid(Hand))
    {
        return;
    }

    if (CardA->GetOwningHand() != Hand || CardB->GetOwningHand() != Hand)
    {
        return;
    }

    if (!Hand->ContainsCard(CardA) || !Hand->ContainsCard(CardB))
    {
        return;
    }

    ASHGameMode* SHGameMode = GetWorld()->GetAuthGameMode<ASHGameMode>();

    if (!IsValid(SHGameMode))
    {
        return;
    }

    if (!SHGameMode->AreCardsPairCompatible(CardA, CardB))
    {
        return;
    }

    ASHGameState* SHGameState = GetWorld()->GetGameState<ASHGameState>();

    if (!IsValid(SHGameState))
    {
        return;
    }

    if (SHGameState->GetCurrentPlayer() != SHPlayerState)
    {
        return;
    }

    const ETurnPhase Phase = SHGameState->GetTurnPhase();

    if (Phase != ETurnPhase::FirstPairing && Phase != ETurnPhase::SecondPairing)
    {
        return;
    }

    if (SHGameMode->IsPairingActionUsed())
    {
        return;
    }

   SHGameMode->ActivatePair(SHPlayerState, CardA, CardB);
   OnPairActivated(CardA, CardB);

    SHGameMode->SetPairingActionUsed(true);

    if (Phase == ETurnPhase::FirstPairing)
    {
        SHGameMode->CompleteCurrentPhase(ETurnPhaseEndReason::PairCreated);
    }
}

void ASHPlayerController::ServerTakeCard_Implementation(ASHCard* Card, int32 InsertIndex)
{
    if (!IsValid(Card))
    {
        return;
    }

    ASHPlayerState* SHPlayerState = GetPlayerState<ASHPlayerState>();

    if (!IsValid(SHPlayerState))
    {
        return;
    }

    ASHHand* TargetHand = SHPlayerState->GetHand();
    ASHHand* SourceHand = Card->GetOwningHand();

    UE_LOG(LogTemp, Warning,
        TEXT("ServerTakeCard: PC=%s PS=%s TargetHand=%s IsLocalController=%s"),
        *GetNameSafe(this),
        *GetNameSafe(SHPlayerState),
        *GetNameSafe(TargetHand),
        IsLocalController() ? TEXT("TRUE") : TEXT("FALSE"));

    if (!IsValid(SourceHand) || !IsValid(TargetHand))
    {
        return;
    }

    if (SourceHand == TargetHand)
    {
        return;
    }

    if (InsertIndex < 0 || InsertIndex > TargetHand->GetCardCount())
    {
        return;
    }

    ASHGameMode* SHGameMode = GetWorld()->GetAuthGameMode<ASHGameMode>();

    UE_LOG(LogTemp, Warning,
        TEXT("BEFORE ADD: TargetHand=%s Card=%s"),
        *GetNameSafe(TargetHand),
        *GetNameSafe(Card));

    ASHGameState* SHGameState = GetWorld()->GetGameState<ASHGameState>();

    if (!IsValid(SHGameState))
    {
        return;
    }

    if (SHGameState->GetCurrentPlayer() != SHPlayerState)
    {
        return;
    }

    if ((SHGameState->GetTurnPhase() == ETurnPhase::SecondPairing) || SHGameState->GetTurnPhase() == ETurnPhase::None)
    {
        return;
    }

    if (Card->GetCardZone() != ECardZone::Hand)
    {
        return;
    }

    if (!SourceHand->ContainsCard(Card))
    {
        return;
    }

    SourceHand->RemoveCard(Card);
    TargetHand->AddCard(Card, InsertIndex);

    ClientReceiveCardDefinition(
        Card,
        Card->GetCardDefinition()
    );

    SHGameMode->CompleteCurrentPhase(ETurnPhaseEndReason::CardDrawn);
}

