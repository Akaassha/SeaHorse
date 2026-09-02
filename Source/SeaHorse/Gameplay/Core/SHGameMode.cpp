// Fill out your copyright notice in the Description page of Project Settings.


#include "SeaHorse/Gameplay/Core/SHGameMode.h"
#include "SeaHorse/Gameplay/SHHand.h"
#include "SeaHorse/Gameplay/Core/SHPlayerState.h"
#include "SeaHorse/Gameplay/Core/SHPlayerController.h"
#include "SeaHorse/Gameplay/Cards/SHCard.h"
#include "SeaHorse/Gameplay/Cards/CardDefinition.h"
#include "SeaHorse/Gameplay/Core/SHGameState.h"
#include "SeaHorse/Gameplay/Board/VictoryStack.h"
#include "SeaHorse/Gameplay/Cards/Fragments/CardEffectFragment.h"
#include "SeaHorse/Gameplay/Cards/Fragments/CardEndGameRulesFragment.h"
#include "SeaHorse/Gameplay/Cards/Tasks/CardEffectTask.h"
#include "SeaHorse/Gameplay/Components/DeckComponent.h"
#include "SeaHorse/Gameplay/Components/TurnComponent.h"
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

    if (SHGameState->PlayerArray.Num() < ExpectedPlayerCount)
    {
        return;
    }

    checkf(
        SHGameState->PlayerArray.Num() == ExpectedPlayerCount,
        TEXT("Expected %d players, but found %d"),
        ExpectedPlayerCount,
        SHGameState->PlayerArray.Num());

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

    checkf(DeckComponentClass, TEXT("DeckComponentClass is not set"));

    DeckComponent = NewObject<UDeckComponent>(this, DeckComponentClass);
    checkf(IsValid(DeckComponent), TEXT("Failed to create DeckComponent"));
    DeckComponent->RegisterComponent();

    DeckComponent->CreateDeck();
    const int32 InitialDeckSize = DeckComponent->GetInitialDeckSize();
    DeckComponent->ShuffleDeck();

    UE_LOG(LogTemp, Warning,
        TEXT("[SH_INIT][%.3f][SERVER] Deck ready | Cards: %d"),
        GetWorld()->GetTimeSeconds(),
        InitialDeckSize);

    ASHPlayerState* StartingPlayer = DeckComponent->DealCards();
    checkf(IsValid(StartingPlayer), TEXT("DeckComponent returned no starting player"));

    ASHGameState* SHGameState = GetGameState<ASHGameState>();
    checkf(IsValid(SHGameState), TEXT("Invalid SHGameState"));

    SHGameState->SetInitialDealtCardCount(InitialDeckSize);

    UE_LOG(LogTemp, Warning,
        TEXT("[SH_INIT][%.3f][SERVER] Setting MatchReady TRUE"),
        GetWorld()->GetTimeSeconds());

    UClass* TurnClass = TurnComponentClass.Get();
    if (!TurnClass)
    {
        TurnClass = UTurnComponent::StaticClass();
    }

    TurnComponent = NewObject<UTurnComponent>(this, TurnClass);
    checkf(IsValid(TurnComponent), TEXT("Failed to create TurnComponent"));
    TurnComponent->RegisterComponent();
    TurnComponent->InitializeTurns(StartingPlayer);

    SHGameState->SetMatchReady(true);
}
// ***** End Match Startup *****


// ***** Begin Card Effects *****
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
    RefreshPlayerScore(PlayerState);
}

void ASHGameMode::CardActivateEffect(ASHPlayerState* InActivatingPlayer, ASHCard* CardA, ASHCard* CardB)
{
    const UCardEffectFragment* NewEffectFragment =
        Cast<UCardEffectFragment>(
            UCardDefinition::FindFragmentByClass(
                CardA->CardDefinition,
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
        CardA,
        CardB
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

void ASHGameMode::RequestPlayerSelection(
    UCardEffectTask* Task,
    ASHPlayerState* SelectingPlayer,
    const TArray<ASHPlayerState*>& Candidates,
    EPlayerSelectionPurpose Purpose)
{
    checkf(IsValid(Task) && ActiveEffectTasks.Contains(Task), TEXT("Selection requested by an inactive effect task"));
    checkf(IsValid(SelectingPlayer), TEXT("Invalid selecting player"));
    checkf(!Candidates.IsEmpty(), TEXT("Player selection requires at least one candidate"));
    checkf(!PendingPlayerSelections.Contains(SelectingPlayer), TEXT("Player already has a pending selection"));

    FPendingPlayerSelection& PendingSelection = PendingPlayerSelections.Add(SelectingPlayer);
    PendingSelection.Task = Task;
    for (ASHPlayerState* Candidate : Candidates)
    {
        if (IsValid(Candidate))
        {
            PendingSelection.Candidates.AddUnique(Candidate);
        }
    }

    checkf(!PendingSelection.Candidates.IsEmpty(), TEXT("Player selection has no valid candidates"));

    ASHPlayerController* SelectingController = Cast<ASHPlayerController>(SelectingPlayer->GetOwner());
    checkf(IsValid(SelectingController), TEXT("Selecting player has no controller"));

    TArray<ASHPlayerState*> ClientCandidates;
    ClientCandidates.Reserve(PendingSelection.Candidates.Num());
    for (ASHPlayerState* Candidate : PendingSelection.Candidates)
    {
        ClientCandidates.Add(Candidate);
    }
    SelectingController->ClientRequestPlayerSelection(ClientCandidates, Purpose);
}

void ASHGameMode::SubmitPlayerSelection(ASHPlayerState* SelectingPlayer, ASHPlayerState* SelectedPlayer)
{
    FPendingPlayerSelection* PendingSelection = PendingPlayerSelections.Find(SelectingPlayer);
    if (!PendingSelection || !IsValid(SelectedPlayer) || !PendingSelection->Candidates.Contains(SelectedPlayer))
    {
        FString CandidateNames;
        if (PendingSelection)
        {
            for (const ASHPlayerState* Candidate : PendingSelection->Candidates)
            {
                if (!CandidateNames.IsEmpty())
                {
                    CandidateNames += TEXT(", ");
                }
                CandidateNames += GetNameSafe(Candidate);
            }
        }

        UE_LOG(LogTemp, Warning,
            TEXT("[SH_SELECTION][REJECTED] SelectingPlayer=%s SelectedPlayer=%s HasPending=%d Candidates=[%s]"),
            *GetNameSafe(SelectingPlayer),
            *GetNameSafe(SelectedPlayer),
            PendingSelection != nullptr,
            *CandidateNames);
        return;
    }

    UE_LOG(LogTemp, Warning,
        TEXT("[SH_SELECTION][ACCEPTED] SelectingPlayer=%s SelectedPlayer=%s"),
        *GetNameSafe(SelectingPlayer),
        *GetNameSafe(SelectedPlayer));

    UCardEffectTask* Task = PendingSelection->Task;
    PendingPlayerSelections.Remove(SelectingPlayer);

    if (IsValid(Task) && ActiveEffectTasks.Contains(Task))
    {
        Task->HandlePlayerSelected(SelectedPlayer);
    }
}
// ***** End Card Effects *****



// ***** Begin Card Rules *****
void ASHGameMode::ActivatePair(ASHPlayerState* PlayerState, ASHCard* CardA, ASHCard* CardB)
{
    checkf(IsValid(PlayerState), TEXT("Invalid PlayerState"));
    checkf(IsValid(CardA) && IsValid(CardB), TEXT("Invalid pair"));

    ASHHand* Hand = PlayerState->GetHand();
    checkf(IsValid(Hand), TEXT("Player has no Hand"));

    Hand->RemoveCard(CardA);
    Hand->RemoveCard(CardB);

    Hand->AddActivationPairToLogicalHand(CardA, CardB);

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

void ASHGameMode::RefreshPlayerScore(ASHPlayerState* PlayerState)
{
    if (!IsValid(PlayerState))
    {
        return;
    }

    ASHHand* Hand = PlayerState->GetHand();
    AVictoryStack* VictoryStack = IsValid(Hand) ? Hand->GetVictoryStack() : nullptr;
    if (IsValid(VictoryStack))
    {
        PlayerState->SetVictoryPoints(VictoryStack->GetPairCount());
    }
}

bool ASHGameMode::TryFinishGame()
{
    checkf(HasAuthority(), TEXT("TryFinishGame can only be called on the server"));

    ASHGameState* SHGameState = GetGameState<ASHGameState>();
    if (!IsValid(SHGameState) || SHGameState->IsGameEnded() || !ActiveEffectTasks.IsEmpty())
    {
        return IsValid(SHGameState) && SHGameState->IsGameEnded();
    }

    int32 CardsRemainingInHands = 0;
    for (APlayerState* PlayerState : SHGameState->PlayerArray)
    {
        ASHPlayerState* SHPlayerState = Cast<ASHPlayerState>(PlayerState);
        ASHHand* Hand = IsValid(SHPlayerState) ? SHPlayerState->GetHand() : nullptr;
        if (!IsValid(Hand))
        {
            return false;
        }

        CardsRemainingInHands += Hand->GetCardCount();
    }

    if (CardsRemainingInHands >= SHGameState->PlayerArray.Num())
    {
        return false;
    }

    TArray<FSHMatchResult> Results;
    Results.Reserve(SHGameState->PlayerArray.Num());

    int32 HighestScore = 0;
    for (APlayerState* PlayerState : SHGameState->PlayerArray)
    {
        ASHPlayerState* SHPlayerState = CastChecked<ASHPlayerState>(PlayerState);
        RefreshPlayerScore(SHPlayerState);

        FSHMatchResult& Result = Results.AddDefaulted_GetRef();
        Result.PlayerState = SHPlayerState;
        Result.Points = SHPlayerState->GetVictoryPoints();
        Result.bAutomaticallyLost = PlayerHasAutomaticLossCard(SHPlayerState);
        if (!Result.bAutomaticallyLost)
        {
            HighestScore = FMath::Max(HighestScore, Result.Points);
        }
    }

    Results.Sort([](const FSHMatchResult& A, const FSHMatchResult& B)
    {
        if (A.bAutomaticallyLost != B.bAutomaticallyLost)
        {
            return !A.bAutomaticallyLost;
        }

        if (A.Points != B.Points)
        {
            return A.Points > B.Points;
        }

        return A.PlayerState->GetSeatIndex() < B.PlayerState->GetSeatIndex();
    });

    for (FSHMatchResult& Result : Results)
    {
        Result.bIsWinner = !Result.bAutomaticallyLost && Result.Points == HighestScore;
    }

    SHGameState->FinishGame(Results);
    return true;
}

bool ASHGameMode::PlayerHasAutomaticLossCard(ASHPlayerState* PlayerState) const
{
    if (!IsValid(PlayerState))
    {
        return false;
    }

    ASHHand* Hand = PlayerState->GetHand();
    if (!IsValid(Hand))
    {
        return false;
    }

    for (ASHCard* Card : Hand->GetCards())
    {
        if (!IsValid(Card))
        {
            continue;
        }

        const UCardEndGameRulesFragment* EndGameRules = Cast<UCardEndGameRulesFragment>(
            UCardDefinition::FindFragmentByClass(
                Card->GetCardDefinition(),
                UCardEndGameRulesFragment::StaticClass()));

        if (IsValid(EndGameRules) && EndGameRules->bOwnerAutomaticallyLoses)
        {
            return true;
        }
    }

    return false;
}
