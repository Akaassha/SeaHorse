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
#include "SeaHorse/Gameplay/Components/TurnComponent.h"
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

    if (!IsValid(SHGameMode) || SHGameMode->IsWaitingForPlayerSelection())
    {
        return;
    }

    UTurnComponent* TurnComponent = SHGameMode->GetTurnComponent();
    if (IsValid(TurnComponent))
    {
        TurnComponent->SkipCurrentPhase(SHPlayerState);
    }
}

void ASHPlayerController::ClientRequestPlayerSelection_Implementation(
    const TArray<ASHPlayerState*>& Candidates,
    EPlayerSelectionPurpose Purpose)
{
    UE_LOG(LogTemp, Warning,
        TEXT("[SH_SELECTION][CLIENT_REQUEST] Controller=%s Purpose=%s CandidateCount=%d"),
        *GetNameSafe(this),
        *UEnum::GetValueAsString(Purpose),
        Candidates.Num());

    for (const ASHPlayerState* Candidate : Candidates)
    {
        ASHHand* CandidateHand = IsValid(Candidate) ? Candidate->GetHand() : nullptr;

        UE_LOG(LogTemp, Warning,
            TEXT("[SH_SELECTION][CLIENT_CANDIDATE] Player=%s Hand=%s Cards=%d"),
            *GetNameSafe(Candidate),
            *GetNameSafe(CandidateHand),
            IsValid(CandidateHand) ? CandidateHand->GetCardCount() : INDEX_NONE);
    }

    OnPlayerSelectionRequested(Candidates, Purpose);
}

void ASHPlayerController::ClientRequestAdditionalCardDraw_Implementation(const TArray<ASHPlayerState*>& ValidSources)
{
    OnAdditionalCardDrawRequested(ValidSources);
}

void ASHPlayerController::ClientUpdateCardDrawGuidance_Implementation(
    const TArray<ASHPlayerState*>& ValidSources,
    ECardDrawGuidanceType GuidanceType)
{
    UE_LOG(LogTemp, Warning,
        TEXT("[SH_DRAW_GUIDANCE][CLIENT] Controller=%s Type=%s SourceCount=%d"),
        *GetNameSafe(this),
        *UEnum::GetValueAsString(GuidanceType),
        ValidSources.Num());

    OnCardDrawGuidanceUpdated(ValidSources, GuidanceType);
}

void ASHPlayerController::ServerSubmitPlayerSelection_Implementation(ASHPlayerState* SelectedPlayer)
{
    ASHGameMode* GameMode = GetWorld()->GetAuthGameMode<ASHGameMode>();
    ASHPlayerState* SelectingPlayer = GetPlayerState<ASHPlayerState>();

    UE_LOG(LogTemp, Warning,
        TEXT("[SH_SELECTION][SERVER_SUBMIT] Controller=%s SelectingPlayer=%s SelectedPlayer=%s"),
        *GetNameSafe(this),
        *GetNameSafe(SelectingPlayer),
        *GetNameSafe(SelectedPlayer));

    if (IsValid(GameMode) && IsValid(SelectingPlayer))
    {
        GameMode->SubmitPlayerSelection(SelectingPlayer, SelectedPlayer);
    }
}

ASHPlayerState* ASHPlayerController::FindPlayerStateForCard(const ASHCard* Card) const
{
    if (!IsValid(Card))
    {
        return nullptr;
    }

    ASHHand* CardHand = Card->GetOwningHand();
    const ASHGameState* GameState = GetWorld()->GetGameState<ASHGameState>();

    if (!IsValid(CardHand) || !IsValid(GameState))
    {
        return nullptr;
    }

    for (APlayerState* CandidatePlayerState : GameState->PlayerArray)
    {
        ASHPlayerState* SHPlayerState = Cast<ASHPlayerState>(CandidatePlayerState);
        if (IsValid(SHPlayerState) && SHPlayerState->GetHand() == CardHand)
        {
            return SHPlayerState;
        }
    }

    return nullptr;
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
        Hand->RefreshActivationPairsPresentation();
    }

    for (APlayerState* CurrentPlayerState : SHGameState->PlayerArray)
    {
        ASHPlayerState* SHPlayerState = Cast<ASHPlayerState>(CurrentPlayerState);
        ASHHand* LogicalHand = IsValid(SHPlayerState) ? SHPlayerState->GetHand() : nullptr;
        AVictoryStack* LogicalStack = IsValid(LogicalHand) ? LogicalHand->GetVictoryStack() : nullptr;

        if (IsValid(LogicalStack))
        {
            LogicalStack->RefreshCardsPresentation();
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
    Card->SetFaceUp(true);
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

    if (!SHGameMode || SHGameMode->IsWaitingForPlayerSelection())
    {
        return;
    }

    UTurnComponent* TurnComponent = SHGameMode->GetTurnComponent();
    if (!IsValid(TurnComponent) || !TurnComponent->CanActivatePair(SHPlayerState, *Pair))
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
    Hand->ForceNetUpdate();
    Hand->MulticastPairEffectActivated(CardA, CardB);

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

    if (!IsValid(SHGameMode) || SHGameMode->IsWaitingForPlayerSelection())
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

    UTurnComponent* TurnComponent = SHGameMode->GetTurnComponent();
    if (!IsValid(TurnComponent) || TurnComponent->IsPairingActionUsed())
    {
        return;
    }

    SHGameMode->ActivatePair(SHPlayerState, CardA, CardB);

    TurnComponent->MarkPairingActionUsed();

    if (Phase == ETurnPhase::FirstPairing)
    {
        TurnComponent->CompleteCurrentPhase(ETurnPhaseEndReason::PairCreated);
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

    if (!IsValid(SHGameMode) || SHGameMode->IsWaitingForPlayerSelection())
    {
        return;
    }

    UE_LOG(LogTemp, Warning,
        TEXT("BEFORE ADD: TargetHand=%s Card=%s"),
        *GetNameSafe(TargetHand),
        *GetNameSafe(Card));

    ASHGameState* SHGameState = GetWorld()->GetGameState<ASHGameState>();

    if (!IsValid(SHGameState))
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

    ASHPlayerState* SourcePlayerState = nullptr;
    for (APlayerState* CandidatePlayerState : SHGameState->PlayerArray)
    {
        ASHPlayerState* Candidate = Cast<ASHPlayerState>(CandidatePlayerState);
        if (IsValid(Candidate) && Candidate->GetHand() == SourceHand)
        {
            SourcePlayerState = Candidate;
            break;
        }
    }

    UTurnComponent* TurnComponent = SHGameMode->GetTurnComponent();
    if (!IsValid(TurnComponent) || !TurnComponent->CanDrawCard(SHPlayerState, SourcePlayerState))
    {
        return;
    }

    SourceHand->RemoveCard(Card);
    TargetHand->AddCard(Card, InsertIndex);

    ClientReceiveCardDefinition(
        Card,
        Card->GetCardDefinition()
    );

    TurnComponent->HandleCardDrawn(SHPlayerState, SourcePlayerState);
}

