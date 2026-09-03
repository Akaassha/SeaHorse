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
#include "Kismet/GameplayStatics.h"

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

    TSet<int32> AssignedSeats;
    for (APlayerState* State : SHGameState->PlayerArray)
    {
        ASHPlayerState* PlayerState = CastChecked<ASHPlayerState>(State);
        ASHHand* Hand = PlayerState->GetHand();
        checkf(IsValid(Hand), TEXT("Player has no assigned hand"));

		const int32 SeatIndex = Hand->GetLayoutSeatIndex();
		checkf(SeatIndex >= 0 && SeatIndex < TotalSeatCount,
			TEXT("Player hand %s has invalid LayoutSeatIndex %d"), *GetNameSafe(Hand), SeatIndex);
		checkf(!AssignedSeats.Contains(SeatIndex), TEXT("Seat %d is assigned more than once"), SeatIndex);

		PlayerState->SetSeatIndex(SeatIndex);
		AssignedSeats.Add(SeatIndex);
    }

}

void ASHGameMode::InitializeParticipantHands()
{
    checkf(HasAuthority(), TEXT("Participant hands can only be initialized on the server"));

    ASHGameState* SHGameState = GetGameState<ASHGameState>();
    checkf(IsValid(SHGameState), TEXT("Invalid SHGameState"));

    const int32 HumanCount = SHGameState->PlayerArray.Num();
    checkf(HumanCount >= 2 && HumanCount <= TotalSeatCount,
        TEXT("SeaHorse requires 2-%d human players; found %d"), TotalSeatCount, HumanCount);

    TArray<ASHHand*> Hands;
	Hands.SetNum(TotalSeatCount);
	for (TActorIterator<ASHHand> It(GetWorld()); It; ++It)
	{
		ASHHand* Hand = *It;
		const int32 Seat = IsValid(Hand) ? Hand->GetLayoutSeatIndex() : INDEX_NONE;
		if (Seat < 0 || Seat >= TotalSeatCount) continue;
		checkf(!IsValid(Hands[Seat]), TEXT("More than one hand uses seat %d"), Seat);
		Hands[Seat] = Hand;
	}

	for (int32 Seat = 0; Seat < TotalSeatCount; ++Seat)
	{
		checkf(IsValid(Hands[Seat]), TEXT("Map must contain one BP_Hand for seat %d"), Seat);
		const bool bHasHuman = SHGameState->PlayerArray.ContainsByPredicate(
			[Seat](const APlayerState* State)
			{
				const ASHPlayerState* Player = Cast<ASHPlayerState>(State);
				return IsValid(Player) && Player->GetSeatIndex() == Seat;
			});
		Hands[Seat]->SetIsNPC(!bHasHuman);
	}

	SHGameState->SetParticipantHands(Hands);
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
    InitializeParticipantHands();

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
    checkf(HasAuthority(), TEXT("Pairs can only be moved on the server"));
    if (!IsValid(PlayerState) || !IsValid(CardA) || !IsValid(CardB))
    {
        return;
    }

    ASHHand* Hand = PlayerState->GetHand();
    if (!IsValid(Hand))
    {
        return;
    }

    AVictoryStack* VictoryStack = Hand->GetVictoryStack();
    if (!IsValid(VictoryStack))
    {
        return;
    }

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

    for (auto It = PendingPlayerSelections.CreateIterator(); It; ++It)
    {
        if (It.Value().Task == CardEffectTask)
        {
            It.RemoveCurrent();
        }
    }
    for (auto It = PendingParticipantSelections.CreateIterator(); It; ++It)
    {
        if (It.Value().Task == CardEffectTask)
        {
            It.RemoveCurrent();
        }
    }
    for (auto It = PendingPairSelections.CreateIterator(); It; ++It)
    {
        if (It.Value().Task == CardEffectTask)
        {
            It.RemoveCurrent();
        }
    }

    ActiveEffectTasks.Remove(CardEffectTask);
}

void ASHGameMode::PassHandsToLeft()
{
	checkf(HasAuthority(), TEXT("Hands can only be passed on the server"));

	ASHGameState* SHGameState = GetGameState<ASHGameState>();
	checkf(IsValid(SHGameState), TEXT("Invalid SHGameState"));

	TArray<ASHHand*> ParticipantHands;
	const int32 ParticipantCount = SHGameState->GetParticipantCount();
	ParticipantHands.SetNum(ParticipantCount);
	for (int32 Seat = 0; Seat < ParticipantCount; ++Seat)
	{
		ParticipantHands[Seat] = SHGameState->FindParticipantHandBySeat(Seat);
		checkf(IsValid(ParticipantHands[Seat]), TEXT("Participant in seat %d has no hand"), Seat);
	}

	TArray<TArray<ASHCard*>> CardsBySeat;
	CardsBySeat.SetNum(ParticipantCount);
	for (int32 Seat = 0; Seat < ParticipantCount; ++Seat)
	{
		CardsBySeat[Seat] = ParticipantHands[Seat]->GetCards();
		for (ASHCard* Card : CardsBySeat[Seat])
		{
			ParticipantHands[Seat]->RemoveCard(Card);
		}
	}

	for (int32 SourceSeat = 0; SourceSeat < ParticipantCount; ++SourceSeat)
	{
		ASHHand* TargetHand = ParticipantHands[(SourceSeat + 1) % ParticipantCount];
		for (ASHCard* Card : CardsBySeat[SourceSeat])
		{
			TargetHand->AddCard(Card, TargetHand->GetCardCount());
		}
	}

	// Cards and their Owner pointers are replicated by different actors/channels.
	// Ask every local view to reconcile several times while that bulk update
	// settles, including the listen-server view where OnRep does not run.
	for (APlayerState* State : SHGameState->PlayerArray)
	{
		ASHPlayerState* PlayerState = Cast<ASHPlayerState>(State);
		ASHPlayerController* Controller = IsValid(PlayerState)
			? Cast<ASHPlayerController>(PlayerState->GetOwner())
			: nullptr;
		if (IsValid(Controller))
		{
			Controller->ClientReconcileRotatedHands();
		}
	}
}

void ASHGameMode::MoveAllActivationPairsToVictoryStacks()
{
	checkf(HasAuthority(), TEXT("Pairs can only be moved on the server"));

	ASHGameState* SHGameState = GetGameState<ASHGameState>();
	checkf(IsValid(SHGameState), TEXT("Invalid SHGameState"));

	for (APlayerState* PlayerState : SHGameState->PlayerArray)
	{
		ASHPlayerState* SHPlayerState = CastChecked<ASHPlayerState>(PlayerState);
		ASHHand* Hand = SHPlayerState->GetHand();
		if (!IsValid(Hand))
		{
			continue;
		}

		const TArray<FActivatedPair> Pairs = Hand->GetLogicalActivationPairs();
		for (const FActivatedPair& Pair : Pairs)
		{
			if (IsValid(Pair.CardA) && IsValid(Pair.CardB))
			{
				MovePairToVictoryStack(SHPlayerState, Pair.CardA, Pair.CardB);
			}
		}
	}
}

bool ASHGameMode::TransferCardToPlayer(
	ASHPlayerState* FromPlayer,
	ASHPlayerState* ToPlayer,
	TSubclassOf<UCardDefinition> CardDefinition)
{
	checkf(HasAuthority(), TEXT("Cards can only be transferred on the server"));

	return TransferCardToHand(
		IsValid(FromPlayer) ? FromPlayer->GetHand() : nullptr,
		IsValid(ToPlayer) ? ToPlayer->GetHand() : nullptr,
		CardDefinition);
}

bool ASHGameMode::TransferCardToHand(
	ASHHand* SourceHand,
	ASHHand* TargetHand,
	TSubclassOf<UCardDefinition> CardDefinition)
{
	checkf(HasAuthority(), TEXT("Cards can only be transferred on the server"));

	if (!IsValid(SourceHand) || !IsValid(TargetHand) || SourceHand == TargetHand || !CardDefinition)
	{
		return false;
	}

	for (ASHCard* Card : SourceHand->GetCards())
	{
		if (IsValid(Card) && Card->GetCardDefinition() == CardDefinition)
		{
			SourceHand->RemoveCard(Card);
			TargetHand->AddCard(Card, TargetHand->GetCardCount());
			return true;
		}
	}

	return false;
}

void ASHGameMode::RequestParticipantSelection(
    UCardEffectTask* Task,
    ASHPlayerState* SelectingPlayer,
    const TArray<ASHHand*>& Candidates,
    EPlayerSelectionPurpose Purpose)
{
    checkf(IsValid(Task) && ActiveEffectTasks.Contains(Task), TEXT("Selection requested by an inactive effect task"));
    checkf(IsValid(SelectingPlayer), TEXT("Invalid selecting player"));
    checkf(!Candidates.IsEmpty(), TEXT("Participant selection requires at least one candidate"));
    checkf(!PendingPlayerSelections.Contains(SelectingPlayer), TEXT("Player already has a pending player selection"));
    checkf(!PendingParticipantSelections.Contains(SelectingPlayer), TEXT("Player already has a pending participant selection"));
    checkf(!PendingPairSelections.Contains(SelectingPlayer), TEXT("Player already has a pending pair selection"));

    FPendingParticipantSelection& Pending = PendingParticipantSelections.Add(SelectingPlayer);
    Pending.Task = Task;
    const ASHGameState* SHGameState = GetGameState<ASHGameState>();
    const TArray<ASHHand*> ParticipantHands = IsValid(SHGameState) ? SHGameState->GetParticipantHands() : TArray<ASHHand*>();
    for (ASHHand* Candidate : Candidates)
    {
        if (IsValid(Candidate) && ParticipantHands.Contains(Candidate))
        {
            Pending.Candidates.AddUnique(Candidate);
        }
    }

    checkf(!Pending.Candidates.IsEmpty(), TEXT("Participant selection has no valid candidates"));
    ASHPlayerController* Controller = Cast<ASHPlayerController>(SelectingPlayer->GetOwner());
    checkf(IsValid(Controller), TEXT("Selecting player has no controller"));

    TArray<ASHHand*> ClientCandidates;
    for (ASHHand* Candidate : Pending.Candidates)
    {
        ClientCandidates.Add(Candidate);
    }
    Controller->ClientRequestParticipantSelection(ClientCandidates, Purpose);
}

void ASHGameMode::SubmitParticipantSelection(ASHPlayerState* SelectingPlayer, ASHHand* SelectedHand)
{
    FPendingParticipantSelection* Pending = PendingParticipantSelections.Find(SelectingPlayer);
    if (!Pending || !IsValid(SelectedHand) || !Pending->Candidates.Contains(SelectedHand))
    {
        UE_LOG(LogTemp, Warning,
            TEXT("[SH_PARTICIPANT_SELECTION][REJECTED] SelectingPlayer=%s SelectedHand=%s HasPending=%d"),
            *GetNameSafe(SelectingPlayer), *GetNameSafe(SelectedHand), Pending != nullptr);
        return;
    }

    UCardEffectTask* Task = Pending->Task;
    PendingParticipantSelections.Remove(SelectingPlayer);
    if (IsValid(Task) && ActiveEffectTasks.Contains(Task))
    {
        Task->HandleParticipantSelected(SelectedHand);
    }
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
    checkf(!PendingParticipantSelections.Contains(SelectingPlayer), TEXT("Player already has a pending participant selection"));
    checkf(!PendingPairSelections.Contains(SelectingPlayer), TEXT("Player already has a pending pair selection"));

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

bool ASHGameMode::RequestActivationPairSelection(
    UCardEffectTask* Task,
    ASHPlayerState* SelectingPlayer,
    const TArray<ASHCard*>& CandidateCards)
{
    checkf(IsValid(Task) && ActiveEffectTasks.Contains(Task), TEXT("Pair selection requested by an inactive task"));
    checkf(IsValid(SelectingPlayer), TEXT("Invalid selecting player"));
    checkf(!PendingPairSelections.Contains(SelectingPlayer), TEXT("Player already has a pending pair selection"));
    checkf(!PendingPlayerSelections.Contains(SelectingPlayer), TEXT("Player already has a pending player selection"));
    checkf(!PendingParticipantSelections.Contains(SelectingPlayer), TEXT("Player already has a pending participant selection"));

    FPendingPairSelection& Pending = PendingPairSelections.Add(SelectingPlayer);
    Pending.Task = Task;
    for (ASHCard* Card : CandidateCards)
    {
        if (IsValid(Card) && Card->GetCardZone() == ECardZone::Activation)
        {
            Pending.CandidateCards.AddUnique(Card);
        }
    }

    if (Pending.CandidateCards.IsEmpty())
    {
        PendingPairSelections.Remove(SelectingPlayer);
        return false;
    }

    ASHPlayerController* Controller = Cast<ASHPlayerController>(SelectingPlayer->GetOwner());
    checkf(IsValid(Controller), TEXT("Selecting player has no controller"));

    TArray<ASHCard*> ClientCandidates;
    ClientCandidates.Reserve(Pending.CandidateCards.Num());
    for (ASHCard* Card : Pending.CandidateCards)
    {
        ClientCandidates.Add(Card);
    }
    Controller->ClientRequestActivationPairSelection(ClientCandidates);
    return true;
}

bool ASHGameMode::SubmitActivationPairSelection(ASHPlayerState* SelectingPlayer, ASHCard* SelectedCard)
{
    FPendingPairSelection* Pending = PendingPairSelections.Find(SelectingPlayer);
    if (!Pending)
    {
        return false;
    }

    // A pending request consumes card clicks, but invalid/stale choices do not finish it.
    if (!IsValid(SelectedCard) || !Pending->CandidateCards.Contains(SelectedCard))
    {
        return true;
    }

    ASHGameState* SHGameState = GetGameState<ASHGameState>();
    ASHPlayerState* PairOwner = nullptr;
    ASHCard* CardA = nullptr;
    ASHCard* CardB = nullptr;
    if (IsValid(SHGameState))
    {
        for (APlayerState* PlayerState : SHGameState->PlayerArray)
        {
            ASHPlayerState* CandidateOwner = Cast<ASHPlayerState>(PlayerState);
            ASHHand* Hand = IsValid(CandidateOwner) ? CandidateOwner->GetHand() : nullptr;
            FActivatedPair* Pair = IsValid(Hand) ? Hand->FindActivationPair(SelectedCard) : nullptr;
            if (Pair && IsValid(Pair->CardA) && IsValid(Pair->CardB) &&
                Pending->CandidateCards.Contains(Pair->CardA) && Pending->CandidateCards.Contains(Pair->CardB))
            {
                PairOwner = CandidateOwner;
                CardA = Pair->CardA;
                CardB = Pair->CardB;
                break;
            }
        }
    }

    if (!IsValid(PairOwner))
    {
        return true;
    }

    UCardEffectTask* Task = Pending->Task;
    PendingPairSelections.Remove(SelectingPlayer);
    if (ASHPlayerController* Controller = Cast<ASHPlayerController>(SelectingPlayer->GetOwner()))
    {
        Controller->ClientRequestActivationPairSelection({});
    }
    if (IsValid(Task) && ActiveEffectTasks.Contains(Task))
    {
        Task->HandleActivationPairSelected(PairOwner, CardA, CardB);
    }
    return true;
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
		int32 Score = VictoryStack->GetPairCount();
		for (const FActivatedPair& Pair : Hand->GetLogicalActivationPairs())
		{
			if (!IsValid(Pair.CardA))
			{
				continue;
			}

			const UCardEndGameRulesFragment* Rules = Cast<UCardEndGameRulesFragment>(
				UCardDefinition::FindFragmentByClass(
					Pair.CardA->GetCardDefinition(),
					UCardEndGameRulesFragment::StaticClass()));

			if (IsValid(Rules))
			{
				Score += Rules->BonusVictoryPointsPerPairInActivationZone;
			}
		}

		PlayerState->SetVictoryPoints(Score);
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
	for (ASHHand* NPCHand : SHGameState->GetNPCHands())
	{
		if (!IsValid(NPCHand))
		{
			return false;
		}
		CardsRemainingInHands += NPCHand->GetCardCount();
	}

    if (CardsRemainingInHands >= SHGameState->GetParticipantCount())
    {
        return false;
    }

    TArray<FSHMatchResult> Results;
    Results.Reserve(SHGameState->PlayerArray.Num());
    TSet<TObjectPtr<ASHPlayerState>> ScoreTieBreakers;

    int32 HighestScore = 0;
    for (APlayerState* PlayerState : SHGameState->PlayerArray)
    {
        ASHPlayerState* SHPlayerState = CastChecked<ASHPlayerState>(PlayerState);
        RefreshPlayerScore(SHPlayerState);

        FSHMatchResult& Result = Results.AddDefaulted_GetRef();
        Result.PlayerState = SHPlayerState;
        Result.Points = SHPlayerState->GetVictoryPoints();
        Result.bAutomaticallyLost = PlayerHasAutomaticLossCard(SHPlayerState);

		ASHHand* ResultHand = SHPlayerState->GetHand();
		if (IsValid(ResultHand))
		{
			for (const FActivatedPair& Pair : ResultHand->GetLogicalActivationPairs())
			{
				if (!IsValid(Pair.CardA))
				{
					continue;
				}

				const UCardEndGameRulesFragment* Rules = Cast<UCardEndGameRulesFragment>(
					UCardDefinition::FindFragmentByClass(
						Pair.CardA->GetCardDefinition(),
						UCardEndGameRulesFragment::StaticClass()));
				if (IsValid(Rules) && Rules->bWinsScoreTies)
				{
					ScoreTieBreakers.Add(SHPlayerState);
					break;
				}
			}
		}

        if (!Result.bAutomaticallyLost)
        {
            HighestScore = FMath::Max(HighestScore, Result.Points);
        }
    }

    Results.Sort([&ScoreTieBreakers](const FSHMatchResult& A, const FSHMatchResult& B)
    {
        if (A.bAutomaticallyLost != B.bAutomaticallyLost)
        {
            return !A.bAutomaticallyLost;
        }

        if (A.Points != B.Points)
        {
            return A.Points > B.Points;
        }

		const bool bATieBreaker = ScoreTieBreakers.Contains(A.PlayerState);
		const bool bBTieBreaker = ScoreTieBreakers.Contains(B.PlayerState);
		if (bATieBreaker != bBTieBreaker)
		{
			return bATieBreaker;
		}

		return A.PlayerState->GetSeatIndex() < B.PlayerState->GetSeatIndex();
    });

	bool bHighestScoreHasTieBreaker = false;
	for (const FSHMatchResult& Result : Results)
	{
		if (!Result.bAutomaticallyLost && Result.Points == HighestScore &&
			ScoreTieBreakers.Contains(Result.PlayerState))
		{
			bHighestScoreHasTieBreaker = true;
			break;
		}
	}

    for (FSHMatchResult& Result : Results)
    {
		Result.bIsWinner = !Result.bAutomaticallyLost && Result.Points == HighestScore &&
			(!bHighestScoreHasTieBreaker || ScoreTieBreakers.Contains(Result.PlayerState));
    }

	TArray<ASHHand*> AutomaticallyLosingNPCs;
	for (ASHHand* NPCHand : SHGameState->GetNPCHands())
	{
		if (HandHasAutomaticLossCard(NPCHand))
		{
			AutomaticallyLosingNPCs.Add(NPCHand);
		}
	}

    SHGameState->FinishGame(Results, AutomaticallyLosingNPCs);
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

    return HandHasAutomaticLossCard(Hand);
}

bool ASHGameMode::HandHasAutomaticLossCard(const ASHHand* Hand) const
{
    if (!IsValid(Hand))
    {
        return false;
    }

    for (ASHCard* Card : const_cast<ASHHand*>(Hand)->GetCards())
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
