// Fill out your copyright notice in the Description page of Project Settings.


#include "SeaHorse/Gameplay/Core/SHGameMode.h"
#include "SeaHorse/Gameplay/SHHand.h"
#include "GameFramework/GameState.h"
#include "SeaHorse/Gameplay/Core/SHPlayerState.h"
#include "SeaHorse/Gameplay/Cards/SHCard.h"
#include "Kismet/GameplayStatics.h"
#include "SeaHorse/Gameplay/Cards/CardDefinition.h"
#include "Algo/RandomShuffle.h"
#include "SeaHorse/Gameplay/Core/SHGameState.h"
#include "SeaHorse/Gameplay/Cards/Fragments/CardActivationRulesFragment.h"
#include "SeaHorse/Gameplay/Board/VictoryStack.h"
#include "SeaHorse/Gameplay/Cards/Fragments/CardEffectFragment.h"
#include "SeaHorse/Gameplay/Cards/Tasks/CardEffectTask.h"
#include "EngineUtils.h"

// ***** Begin Player setup *****
void ASHGameMode::HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer)
{
    Super::HandleStartingNewPlayer_Implementation(NewPlayer);

    ASHPlayerState* SHPlayerState = NewPlayer->GetPlayerState<ASHPlayerState>();

    checkf( IsValid(SHPlayerState), TEXT("Player %s does not have a valid ASHPlayerState"), *GetNameSafe(NewPlayer));

    ASHHand* Hand = FindAvailableHand();

    checkf(IsValid(Hand), TEXT("No available Hand found in level for player %s"), *GetNameSafe(NewPlayer));

    Hand->SetOwner(NewPlayer);
    SHPlayerState->SetHand(Hand);

    TryStartGame();
}

ASHHand* ASHGameMode::FindAvailableHand() const
{
    ASHHand* AvailableHand = nullptr;

    for (TActorIterator<ASHHand> It(GetWorld()); It; ++It)
    {
        ASHHand* Hand = *It;

        if (!IsValid(Hand))
        {
            continue;
        }

        if (Hand->GetLayoutSeatIndex() == INDEX_NONE)
        {
            continue;
        }

        if (IsValid(Hand->GetOwner()))
        {
            continue;
        }

        if (!IsValid(AvailableHand) ||
            Hand->GetLayoutSeatIndex() < AvailableHand->GetLayoutSeatIndex())
        {
            AvailableHand = Hand;
        }
    }

    return AvailableHand;
}

void ASHGameMode::AssignSeats()
{
    ASHGameState* SHGameState = GetGameState<ASHGameState>();

    checkf(IsValid(SHGameState), TEXT("Invalid SHGameState"));

    for (int32 SeatIndex = 0; SeatIndex < SHGameState->PlayerArray.Num(); ++SeatIndex)
    {
        ASHPlayerState* PlayerState = Cast<ASHPlayerState>(SHGameState->PlayerArray[SeatIndex]);
        PlayerState->SetSeatIndex(SeatIndex);
    }

}
// ***** End Player setup *****



// ***** Begin Match Startup *****
void ASHGameMode::TryStartGame()
{
    if (bGameStarted)
    {
        return;
    }

    ASHGameState* SHGameState = GetGameState<ASHGameState>();
    checkf(IsValid(SHGameState), TEXT("Invalid SHGameState"));

    if (SHGameState->PlayerArray.Num() != ExpectedPlayerCount)
    {
        return;
    }

    for (APlayerState* PlayerState : SHGameState->PlayerArray)
    {
        ASHPlayerState* SHPlayerState = Cast<ASHPlayerState>(PlayerState);

        if (!IsValid(SHPlayerState) || !IsValid(SHPlayerState->GetHand()))
        {
            return;
        }
    }

    bGameStarted = true;
    StartGame();
}

void ASHGameMode::StartGame()
{
    UE_LOG(LogTemp, Warning,
        TEXT("[SH_INIT][%.3f][SERVER] StartGame BEGIN"),
        GetWorld()->GetTimeSeconds());

    AssignSeats();

    UE_LOG(LogTemp, Warning,
        TEXT("[SH_INIT][%.3f][SERVER] AssignSeats DONE"),
        GetWorld()->GetTimeSeconds());

    CreateDeck();
    ShuffleDeck();

    UE_LOG(LogTemp, Warning,
        TEXT("[SH_INIT][%.3f][SERVER] Deck ready | Cards: %d"),
        GetWorld()->GetTimeSeconds(),
        Deck.Num());

    DealCards();

    ASHGameState* SHGameState = GetGameState<ASHGameState>();
    checkf(IsValid(SHGameState), TEXT("Invalid SHGameState"));

    SHGameState->SetInitialDealtCardCount(DeckSize);

    UE_LOG(LogTemp, Warning,
        TEXT("[SH_INIT][%.3f][SERVER] Setting MatchReady TRUE"),
        GetWorld()->GetTimeSeconds());

    ASHPlayerState* StartingPlayer = ChooseStartingPlayer();
    checkf(IsValid(StartingPlayer), TEXT("No valid starting player"));

    SHGameState->SetCurrentPlayer(StartingPlayer);
    StartTurn();

    SHGameState->SetMatchReady(true);
}
// ***** End Match Startup *****



// ***** Begin Handle Deck *****
void ASHGameMode::CreateDeck()
{
    checkf(DeckDefinition, TEXT("DeckDefinition is not set"));
    checkf(CardClass, TEXT("CardClass is not set"));
    checkf(Deck.IsEmpty(), TEXT("CreateDeck called while Deck is not empty"));

    TArray<FDeckEntry*> DeckEntries;
    DeckDefinition->GetAllRows<FDeckEntry>(
        TEXT("ASHGameMode::CreateDeck"),
        DeckEntries
    );

    for (const FDeckEntry* Entry : DeckEntries)
    {
        checkf(Entry, TEXT("Invalid DeckEntry"));
        checkf(Entry->CardDefinition, TEXT("DeckEntry has no CardDefinition"));
        checkf(Entry->Count > 0, TEXT("DeckEntry has invalid Count: %d"), Entry->Count);

        for (int32 i = 0; i < Entry->Count; ++i)
        {
            const FTransform SpawnTransform = FTransform::Identity;

            ASHCard* Card = GetWorld()->SpawnActorDeferred<ASHCard>(CardClass, SpawnTransform);

            checkf(IsValid(Card), TEXT("Failed to spawn card"));

            Card->SetCardDefinition(Entry->CardDefinition);

            UGameplayStatics::FinishSpawningActor(Card, SpawnTransform);

            Card->Initialize();
            Deck.Add(Card);
        }
    }

    DeckSize = Deck.Num();
}

void ASHGameMode::ShuffleDeck()
{
    Algo::RandomShuffle(Deck);
}

void ASHGameMode::DealCards()
{
    ASHGameState* SHGameState = GetGameState<ASHGameState>();
    checkf(IsValid(SHGameState), TEXT("Invalid SHGameState"));

    ASHPlayerState* FirstPlayer = ChooseFirstDealtPlayer();
    checkf(IsValid(FirstPlayer), TEXT("ChooseDealingStartPlayer returned invalid player"));

    int32 PlayerIndex = SHGameState->PlayerArray.IndexOfByKey(FirstPlayer);
    checkf(PlayerIndex != INDEX_NONE, TEXT("Chosen dealing start player is not in PlayerArray"));

    while (!Deck.IsEmpty())
    {
        ASHPlayerState* PlayerState = Cast<ASHPlayerState>(SHGameState->PlayerArray[PlayerIndex]);
        ASHHand* Hand = PlayerState->GetHand();
        ASHCard* Card = Deck.Pop();

        checkf(IsValid(Card), TEXT("Deck contains invalid Card"));

        Hand->AddCard(Card, Hand->GetCardCount());

        PlayerIndex = (PlayerIndex + 1) % SHGameState->PlayerArray.Num();

    }
}

ASHPlayerState* ASHGameMode::ChooseFirstDealtPlayer_Implementation()
{
    ASHGameState* SHGameState = GetGameState<ASHGameState>();
    checkf(IsValid(SHGameState), TEXT("Invalid GameState"));
    checkf(SHGameState->PlayerArray.Num() > 0, TEXT("No players"));

    const int32 RandomIndex =
        FMath::RandRange(0, SHGameState->PlayerArray.Num() - 1);

    ASHPlayerState* Player =
        Cast<ASHPlayerState>(SHGameState->PlayerArray[RandomIndex]);

    checkf(IsValid(Player), TEXT("Invalid starting player"));

    return Player;
}
// ***** End Handle Deck *****



// ***** Begin Turns *****
void ASHGameMode::StartTurn()
{
    ASHGameState* SHGameState = GetGameState<ASHGameState>();
    checkf(IsValid(SHGameState), TEXT("Invalid GameState"));
    checkf(IsValid(SHGameState->GetCurrentPlayer()), TEXT("No current player"));

    bPairingActionUsed = false;

    SHGameState->SetTurnPhase(ETurnPhase::FirstPairing);
}

ASHPlayerState* ASHGameMode::ChooseStartingPlayer_Implementation()
{
    ASHGameState* SHGameState = GetGameState<ASHGameState>();
    checkf(IsValid(SHGameState), TEXT("Invalid GameState"));
    checkf(SHGameState->PlayerArray.Num() > 0, TEXT("No players"));

    const int32 RandomIndex = FMath::RandRange(0, SHGameState->PlayerArray.Num() - 1);

    ASHPlayerState* Player = Cast<ASHPlayerState>(SHGameState->PlayerArray[RandomIndex]);

    checkf(IsValid(Player), TEXT("Invalid starting player"));

    return Player;
}


ETurnPhase ASHGameMode::GetNextTurnPhase_Implementation(ETurnPhase CurrentPhase, ETurnPhaseEndReason Reason)
{
    switch (CurrentPhase)
    {
    case ETurnPhase::FirstPairing:
    {
        if (Reason == ETurnPhaseEndReason::CardDrawn)
        {
            return ETurnPhase::SecondPairing;
        }

        return ETurnPhase::DrawCard;
    }

    case ETurnPhase::DrawCard:
        return ETurnPhase::SecondPairing;

    case ETurnPhase::SecondPairing:
        return ETurnPhase::None;

    default:
        return ETurnPhase::None;
    }
}

ASHPlayerState* ASHGameMode::ChooseNextPlayer_Implementation(ASHPlayerState* CurrentPlayer)
{
    checkf(IsValid(CurrentPlayer), TEXT("Invalid current player"));

    ASHGameState* SHGameState = GetGameState<ASHGameState>();
    checkf(IsValid(SHGameState), TEXT("Invalid GameState"));

    const int32 PlayerCount = SHGameState->PlayerArray.Num();
    checkf(PlayerCount > 0, TEXT("Cannot choose next player without players"));

    const int32 NextSeatIndex =
        (CurrentPlayer->GetSeatIndex() + 1) % PlayerCount;

    for (APlayerState* PlayerState : SHGameState->PlayerArray)
    {
        ASHPlayerState* SHPlayerState =
            Cast<ASHPlayerState>(PlayerState);

        if (IsValid(SHPlayerState) &&
            SHPlayerState->GetSeatIndex() == NextSeatIndex)
        {
            return SHPlayerState;
        }
    }

    return nullptr;
}

void ASHGameMode::CompleteCurrentPhase(ETurnPhaseEndReason Reason)
{
    ASHGameState* SHGameState = GetGameState<ASHGameState>();
    checkf(IsValid(SHGameState), TEXT("Invalid GameState"));

    const ETurnPhase CurrentPhase = SHGameState->GetTurnPhase();
    const ETurnPhase NextPhase = GetNextTurnPhase(CurrentPhase, Reason);

    if (NextPhase == ETurnPhase::None)
    {
        EndTurn();
        return;
    }

    EnterTurnPhase(NextPhase);
    //SHGameState->SetTurnPhase(NextPhase);
}
void ASHGameMode::SkipCurrentPhase(ASHPlayerState* RequestingPlayer)
{
    ASHGameState* SHGameState = GetGameState<ASHGameState>();
    checkf(IsValid(SHGameState), TEXT("Invalid GameState"));

    if (SHGameState->GetCurrentPlayer() != RequestingPlayer)
    {
        return;
    }

    const ETurnPhase Phase = SHGameState->GetTurnPhase();

    if (Phase != ETurnPhase::FirstPairing &&
        Phase != ETurnPhase::SecondPairing)
    {
        return;
    }

    CompleteCurrentPhase(ETurnPhaseEndReason::PlayerSkipped);
}
void ASHGameMode::EndTurn()
{
    ASHGameState* SHGameState = GetGameState<ASHGameState>();
    checkf(IsValid(SHGameState), TEXT("Invalid GameState"));

    ASHPlayerState* CurrentPlayer = SHGameState->GetCurrentPlayer();
    checkf(IsValid(CurrentPlayer), TEXT("Cannot end turn without current player"));

    ASHPlayerState* NextPlayer = ChooseNextPlayer(CurrentPlayer);

    checkf(
        IsValid(NextPlayer),
        TEXT("ChooseNextPlayer returned invalid player")
    );

    SHGameState->SetCurrentPlayer(NextPlayer);

    StartTurn();
}

void ASHGameMode::EnterTurnPhase(ETurnPhase NewPhase)
{
    if (NewPhase == ETurnPhase::FirstPairing ||
        NewPhase == ETurnPhase::SecondPairing)
    {
        bPairingActionUsed = false;
    }

    GetGameState<ASHGameState>()->SetTurnPhase(NewPhase);
}

bool ASHGameMode::CanActivatePair(ASHPlayerState* RequestingPlayer, FActivatedPair& ActivatedPair)
{
    if (!IsValid(RequestingPlayer) ||
        !IsValid(ActivatedPair.CardA) ||
        !IsValid(ActivatedPair.CardB) ||
        ActivatedPair.bActivated)
    {
        return false;
    }

    ASHGameState* SHGameState = GetGameState<ASHGameState>();
    checkf(IsValid(SHGameState), TEXT("Invalid GameState"));

    bool bIsOwnTurn = SHGameState->GetCurrentPlayer() == RequestingPlayer;
    const UCardActivationRulesFragment* Rules = Cast<UCardActivationRulesFragment>
                                                (UCardDefinition::FindFragmentByClass( ActivatedPair.CardA->CardDefinition, UCardActivationRulesFragment::StaticClass()));
    
    if (!IsValid(Rules))
    {
        return bIsOwnTurn;
    }

    if (!Rules->bCanBeActivated)
    {
        return false;
    }

    switch (Rules->TurnRestriction)
    {
    case ECardActivationRules::OwnTurn:
        if (!bIsOwnTurn)
        {
            return false;
        }
        break;

    case ECardActivationRules::OutsideOwnTurn:
        if (bIsOwnTurn)
        {
            return false;
        }
        break;

    case ECardActivationRules::AnyTurn:
        break;
    }

    if (!Rules->AllowedPhases.IsEmpty() &&
        !Rules->AllowedPhases.Contains(SHGameState->GetTurnPhase()))
    {
        return false;
    }

    return true;
}
void ASHGameMode::MovePairToVictoryStack(ASHPlayerState* PlayerState, ASHCard* CardA, ASHCard* CardB)
{
    checkf(IsValid(PlayerState), TEXT("Invalid PlayerState"));

    ASHHand* Hand = PlayerState->GetHand();
    checkf(IsValid(Hand), TEXT("Player has no Hand"));

    AVictoryStack* VictoryStack = Hand->GetVictoryStack();
    checkf(IsValid(VictoryStack), TEXT("Hand has no VictoryStack"));

    const bool bRemoved = Hand->RemoveActivationPair(CardA, CardB);

    if (!bRemoved)
    {
        return;
    }
    VictoryStack->AddPair(CardA, CardB);
}
void ASHGameMode::CardActivateEffect(ASHPlayerState* InActivatingPlayer, FActivatedPair* Pair)
{
    const UCardEffectFragment* NewEffectFragment =
        Cast<UCardEffectFragment>(
            UCardDefinition::FindFragmentByClass(
                Pair->CardA->CardDefinition,
                UCardEffectFragment::StaticClass()
            )
        );

    checkf(IsValid(NewEffectFragment),
        TEXT("Activated card has no CardEffectFragment"));

    checkf(NewEffectFragment->EffectTaskClass,
        TEXT("CardEffectFragment has no EffectTaskClass"));

    UCardEffectTask* EffectTask = NewObject<UCardEffectTask>(
        this,
        NewEffectFragment->EffectTaskClass
    );

    ActiveEffectTasks.Add(EffectTask);

    EffectTask->Initialize(
        InActivatingPlayer,
        Pair->CardA,
        Pair->CardB
    );

    EffectTask->StartEffect();
}
void ASHGameMode::FinishEffectTask(UCardEffectTask* CardEffectTask)
{
    checkf(IsValid(CardEffectTask), TEXT("Invalid EffectTask"));

    ASHPlayerState* ActivatingPlayer = CardEffectTask->GetActivatingPlayer();

    ASHCard* CardA = CardEffectTask->GetCardA();
    ASHCard* CardB = CardEffectTask->GetCardB();

    checkf(IsValid(ActivatingPlayer), TEXT("Invalid activating player"));
    checkf(IsValid(CardA) && IsValid(CardB), TEXT("Invalid effect cards"));

    MovePairToVictoryStack(ActivatingPlayer, CardA, CardB);

    ActiveEffectTasks.Remove(CardEffectTask);
}
// ***** End Turns *****



// ***** Begin Card Rules *****
void ASHGameMode::ActivatePair(ASHPlayerState* PlayerState, ASHCard* CardA, ASHCard* CardB)
{
    checkf(IsValid(PlayerState), TEXT("Invalid PlayerState"));
    checkf(IsValid(CardA) && IsValid(CardB), TEXT("Invalid pair"));

    ASHHand* Hand = PlayerState->GetHand();
    checkf(IsValid(Hand), TEXT("Player has no Hand"));

    Hand->RemoveCard(CardA);
    Hand->RemoveCard(CardB);

    CardA->Reveal();
    CardB->Reveal();
}


bool ASHGameMode::AreCardsPairCompatible(ASHCard* CardA, ASHCard* CardB)
{

    if (!IsValid(CardA) || !IsValid(CardB))
    {
        return false;
    }

    if (CardA == CardB)
    {
        return false;
    }

    const TSubclassOf<UCardDefinition> DefinitionA = CardA->GetCardDefinition();

    const TSubclassOf<UCardDefinition> DefinitionB = CardB->GetCardDefinition();

    if (!IsValid(DefinitionA) || !IsValid(DefinitionB))
    {
        return false;
    }

    return DefinitionA == DefinitionB;

}
// ***** End Card Rules *****