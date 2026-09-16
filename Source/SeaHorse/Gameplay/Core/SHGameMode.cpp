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
#include "Gameplay/Cards/Fragments/CardReactionFragment.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/GameSession.h"
#include "Engine/GameInstance.h"
#include "Online/SHSessionSubsystem.h"
#include "Algo/RandomShuffle.h"
#if WITH_EDITOR
#include "Settings/LevelEditorPlaySettings.h"
#endif

void ASHGameMode::InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage)
{
    Super::InitGame(MapName, Options, ErrorMessage);
    if (!ErrorMessage.IsEmpty() || !DiscoverTableSeats(ErrorMessage)) return;
    // Supplied by the authoritative lobby's ServerTravel, never by an individual login URL.
    if (UGameplayStatics::HasOption(Options, TEXT("SHExpectedPlayers")))
    {
        const int32 LobbyPlayerCount = UGameplayStatics::GetIntOption(Options, TEXT("SHExpectedPlayers"), 0);
        if (LobbyPlayerCount < 2 || LobbyPlayerCount > TotalSeatCount)
        {
            ErrorMessage = TEXT("Invalid lobby player count.");
            return;
        }
        ExpectedPlayerCount = LobbyPlayerCount;
    }
#if WITH_EDITOR
    else if (GetWorld()->IsPlayInEditor())
    {
        // Direct PIE tests use the editor's roster; lobby travel always wins.
        int32 PIEPlayers = ExpectedPlayerCount;
        GetDefault<ULevelEditorPlaySettings>()->GetPlayNumberOfClients(PIEPlayers);
        ExpectedPlayerCount = PIEPlayers;
    }
#endif
    if (ExpectedPlayerCount < 2 || ExpectedPlayerCount > TotalSeatCount)
    {
        ErrorMessage = FString::Printf(TEXT("Expected %d players, but this map supports 2-%d."),
            ExpectedPlayerCount, TotalSeatCount);
        return;
    }
    UE_LOG(LogTemp, Log, TEXT("[SH_INIT] Table seats=%d, expected humans=%d"), TotalSeatCount, ExpectedPlayerCount);
    if (GameSession) { GameSession->MaxPlayers = ExpectedPlayerCount; }
}

bool ASHGameMode::DiscoverTableSeats(FString& ErrorMessage)
{
    TSet<int32> Seats;
    for (TActorIterator<ASHHand> It(GetWorld()); It; ++It)
    {
        const int32 Seat = It->GetLayoutSeatIndex();
        if (Seat == INDEX_NONE) continue;
        if (Seat < 0 || Seat >= 6 || Seats.Contains(Seat))
        {
            ErrorMessage = FString::Printf(TEXT("Invalid or duplicate table seat %d on %s."), Seat, *It->GetName());
            return false;
        }
        Seats.Add(Seat);
    }
    for (int32 Seat = 0; Seat < Seats.Num(); ++Seat)
    {
        if (!Seats.Contains(Seat))
        {
            ErrorMessage = TEXT("Table seat indices must be consecutive, starting at zero.");
            return false;
        }
    }
    if (Seats.Num() < 2)
    {
        ErrorMessage = TEXT("Map requires at least two table seats.");
        return false;
    }
    TotalSeatCount = Seats.Num();
    return true;
}

void ASHGameMode::PreLogin(const FString& Options, const FString& Address, const FUniqueNetIdRepl& UniqueId, FString& ErrorMessage)
{
    Super::PreLogin(Options, Address, UniqueId, ErrorMessage);
    if (ErrorMessage.IsEmpty() && (bGameStarted || GetNumPlayers() >= ExpectedPlayerCount))
    {
        ErrorMessage = TEXT("This match is full or has already started.");
    }
}

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

    UE_LOG(LogTemp, Log, TEXT("[SH_INIT] Participant initialized: %s, hand=%s, expected=%d"),
        *GetNameSafe(NewPlayer), *GetNameSafe(Hand), ExpectedPlayerCount);

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
    if (USHSessionSubsystem* Sessions = GetGameInstance()->GetSubsystem<USHSessionSubsystem>())
    {
        Sessions->NotifyMatchReady(GetWorld());
    }
}
// ***** End Match Startup *****


// ***** Begin Card Effects *****

void ASHGameMode::SetPairTargetSelectionPresentation(UCardEffectTask* Task,
	ASHPlayerState* SelectingPlayer, bool bSelectingTarget)
{
	ASHPlayerController* Controller = IsValid(SelectingPlayer)
		? Cast<ASHPlayerController>(SelectingPlayer->GetOwner()) : nullptr;
	if (!IsValid(Task) || !IsValid(Controller))
	{
		return;
	}

	if (bSelectingTarget)
	{
		if (ActiveTargetPresentations.FindRef(SelectingPlayer) == Task)
		{
			return;
		}
		if (!IsValid(Task->GetCardA()) || !IsValid(Task->GetCardB()))
		{
			return;
		}
		ActiveTargetPresentations.Add(SelectingPlayer, Task);
	}
	else
	{
		if (ActiveTargetPresentations.FindRef(SelectingPlayer) != Task)
		{
			return;
		}
		ActiveTargetPresentations.Remove(SelectingPlayer);
	}

	Controller->ClientSetPairTargetSelection(Task->GetCardA(), Task->GetCardB(),
		bSelectingTarget, Task->GetEffectPresentationId());
}

bool ASHGameMode::HasPendingSelection(
	UCardEffectTask* Task, ASHPlayerState* SelectingPlayer) const
{
	const FPendingPlayerSelection* PlayerSelection = PendingPlayerSelections.Find(SelectingPlayer);
	const FPendingParticipantSelection* ParticipantSelection = PendingParticipantSelections.Find(SelectingPlayer);
	const FPendingPairSelection* PairSelection = PendingPairSelections.Find(SelectingPlayer);
	const FPendingPairSelection* HandSelection = PendingHandCardSelections.Find(SelectingPlayer);
	return (PlayerSelection && PlayerSelection->Task == Task) ||
		(ParticipantSelection && ParticipantSelection->Task == Task) ||
		(PairSelection && PairSelection->Task == Task) || (HandSelection && HandSelection->Task == Task);
}

void ASHGameMode::FinishSelectionStep(UCardEffectTask* Task, ASHPlayerState* SelectingPlayer)
{
	if (!HasPendingSelection(Task, SelectingPlayer))
	{
		SetPairTargetSelectionPresentation(Task, SelectingPlayer, false);
	}
}
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

    // Bulk collection (Gnushor) also reaches this function before its task finishes.
    // Keep cards on the table until all active presentation blocks have ended.
    const bool bCaptureWaitingForEffect = PairCaptureRecipients.Contains(CardA) && ActiveEffectTasks.ContainsByPredicate(
        [CardA](const UCardEffectTask* Task) { return IsValid(Task) && Task->GetCardA() == CardA; });
    if ((IsValid(TurnComponent) && TurnComponent->HasNamedTurnTransitionBlocks()) || bCaptureWaitingForEffect)
    {
        if (!Hand->FindActivationPair(CardA)) { return; }
        FCompletedEffectPair* Pending = CompletedEffectPairsWaitingForPresentation.FindByPredicate(
            [CardA, CardB](const FCompletedEffectPair& Entry)
            {
                return (Entry.CardA == CardA && Entry.CardB == CardB) ||
                    (Entry.CardA == CardB && Entry.CardB == CardA);
            });
        if (!Pending)
        {
            Pending = &CompletedEffectPairsWaitingForPresentation.AddDefaulted_GetRef();
        }
        Pending->ActivatingPlayer = PlayerState;
        Pending->CardA = CardA;
        Pending->CardB = CardB;
        Pending->bMoveToVictoryStack = true;
        Hand->SetActivationPairState(CardA, CardB, EActivationPairState::VictoryPresentation);
        return;
    }

    if (TWeakObjectPtr<ASHPlayerState>* RecipientEntry = PairCaptureRecipients.Find(CardA))
    {
        ASHPlayerState* Recipient = RecipientEntry->Get();
        PairCaptureRecipients.Remove(CardA);
        const FActivatedPair* Found = Hand->FindActivationPair(CardA);
        if (Found && IsValid(Recipient) && IsValid(Recipient->GetHand()) && IsValid(Recipient->GetOwner()))
        {
            FActivatedPair Captured = *Found;
            Captured.bActivated = false;
            {
                TGuardValue<bool> Guard(bProcessingPairActivations, true);
                Hand->RemoveActivationPair(CardA, CardB);
                CompleteQueuedPairActivation(CardA, CardB);
                Recipient->GetHand()->ReceiveTransferredPair(Captured);
                RefreshPlayerScore(PlayerState);
                RefreshPlayerScore(Recipient);
            }
            TryProcessQueuedPairActivations();
            return;
        }
    }
    const bool bRemoved = Hand->RemoveActivationPair(CardA, CardB);

    if (!bRemoved)
    {
        return;
    }
    VictoryStack->AddPair(CardA, CardB);
    RefreshPlayerScore(PlayerState);
	CompleteQueuedPairActivation(CardA, CardB);
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
        CardB,
        NewEffectFragment->EffectPresentationId.IsNone()
            ? NewEffectFragment->EffectTaskClass->GetFName()
            : NewEffectFragment->EffectPresentationId
    );
	if (PairCaptureRecipients.Contains(CardA)) { EffectTask->CommitEffect(); }

    if (!EffectTask->RequiresTargetSelection())
    {
        EffectTask->PlayActivationVFX();
    }
    EffectTask->StartEffect();
}
void ASHGameMode::FinishEffectTask(UCardEffectTask* CardEffectTask)
{
    checkf(IsValid(CardEffectTask), TEXT("Invalid EffectTask"));
	if (!ActiveEffectTasks.Contains(CardEffectTask))
	{
		return;
	}

    ASHPlayerState* ActivatingPlayer = CardEffectTask->GetActivatingPlayer();

    ASHCard* CardA = CardEffectTask->GetCardA();
    ASHCard* CardB = CardEffectTask->GetCardB();

    checkf(IsValid(ActivatingPlayer), TEXT("Invalid activating player"));
    checkf(IsValid(CardA) && IsValid(CardB), TEXT("Invalid effect cards"));
	for (auto It = PendingHandCardSelections.CreateIterator(); It; ++It)
	{
		if (It.Value().Task == CardEffectTask)
		{
			SetPairTargetSelectionPresentation(CardEffectTask, It.Key(), false);
			It.RemoveCurrent();
		}
	}

    for (auto It = PendingPlayerSelections.CreateIterator(); It; ++It)
    {
        if (It.Value().Task == CardEffectTask)
        {
            SetPairTargetSelectionPresentation(CardEffectTask, It.Key().Get(), false);
            It.RemoveCurrent();
        }
    }
    for (auto It = PendingParticipantSelections.CreateIterator(); It; ++It)
    {
        if (It.Value().Task == CardEffectTask)
        {
            SetPairTargetSelectionPresentation(CardEffectTask, It.Key().Get(), false);
            It.RemoveCurrent();
        }
    }
    for (auto It = PendingPairSelections.CreateIterator(); It; ++It)
    {
        if (It.Value().Task == CardEffectTask)
        {
            SetPairTargetSelectionPresentation(CardEffectTask, It.Key().Get(), false);
            It.RemoveCurrent();
        }
    }

	// Presentation bookkeeping is intentionally independent of the pending
	// selection containers so an externally completed/cancelled task cannot
	// leave a local targeting session behind.
	TArray<TObjectPtr<ASHPlayerState>> PresentationOwners;
	for (const TPair<TObjectPtr<ASHPlayerState>, TObjectPtr<UCardEffectTask>>& Entry
		: ActiveTargetPresentations)
	{
		if (Entry.Value == CardEffectTask)
		{
			PresentationOwners.Add(Entry.Key);
		}
	}
	for (ASHPlayerState* PresentationOwner : PresentationOwners)
	{
		SetPairTargetSelectionPresentation(CardEffectTask, PresentationOwner, false);
	}

	const ECardEffectPairDisposition PairDisposition = CardEffectTask->GetPairDisposition();
	if (PairDisposition != ECardEffectPairDisposition::MoveToVictoryStack) { PairCaptureRecipients.Remove(CardA); }
	ActiveEffectTasks.Remove(CardEffectTask);

	ASHHand* ActivatingHand = ActivatingPlayer->GetHand();
	if (PairDisposition == ECardEffectPairDisposition::RemoveFromGame)
	{
		RemoveStoredPairFromGame(ActivatingHand, CardA);
		TryProcessQueuedPairActivations();
	}
	else if (PairDisposition == ECardEffectPairDisposition::KeepOnTable)
	{
		if (IsValid(ActivatingHand))
		{
			ActivatingHand->SetActivationPairState(CardA, CardB, EActivationPairState::Ready);
		}
		if (IsValid(TurnComponent) && TurnComponent->HasNamedTurnTransitionBlocks())
		{
			FCompletedEffectPair& PendingCompletion =
				CompletedEffectPairsWaitingForPresentation.AddDefaulted_GetRef();
			PendingCompletion.ActivatingPlayer = ActivatingPlayer;
			PendingCompletion.CardA = CardA;
			PendingCompletion.CardB = CardB;
			PendingCompletion.bMoveToVictoryStack = false;
		}
		else
		{
			CompleteQueuedPairActivation(CardA, CardB);
		}
	}
	else
	{
		if (IsValid(ActivatingHand))
		{
			ActivatingHand->SetActivationPairState(CardA, CardB, EActivationPairState::VictoryPresentation);
			ActivatingHand->MulticastPairReadyForVictory(CardA, CardB);
		}

		MovePairToVictoryStack(ActivatingPlayer, CardA, CardB);
	}

	if (IsValid(TurnComponent))
	{
		TurnComponent->NotifyEffectTaskFinished();
	}
}

bool ASHGameMode::CancelEffectTargetSelection(ASHPlayerState* SelectingPlayer, ASHCard* CardA, ASHCard* CardB)
{
	if (!HasAuthority() || !IsValid(SelectingPlayer) || !IsValid(CardA) || !IsValid(CardB)) { return false; }
	UCardEffectTask* Task = ActiveTargetPresentations.FindRef(SelectingPlayer);
	ASHHand* Hand = SelectingPlayer->GetHand();
	FActivatedPair* Pair = IsValid(Hand) ? Hand->FindActivationPair(CardA) : nullptr;
	if (!IsValid(Task) || Task->GetActivatingPlayer() != SelectingPlayer ||
		Task->GetCardA() != CardA || Task->GetCardB() != CardB || !ActiveEffectTasks.Contains(Task) ||
		!HasPendingSelection(Task, SelectingPlayer) || !Pair ||
		(Pair->CardA != CardB && Pair->CardB != CardB) || !Task->CancelPendingTargetSelection())
	{
		return false;
	}
	PendingPlayerSelections.Remove(SelectingPlayer);
	PendingParticipantSelections.Remove(SelectingPlayer);
	PendingPairSelections.Remove(SelectingPlayer);
	PendingHandCardSelections.Remove(SelectingPlayer);
	ActiveEffectTasks.Remove(Task);
	// Teardown first: restoring old outline snapshots must precede making the pair ready.
	SetPairTargetSelectionPresentation(Task, SelectingPlayer, false);
	PendingPairActivations.RemoveAll([CardA, CardB](const FPendingPairActivation& Entry)
	{
		return Entry.CardA == CardA && Entry.CardB == CardB;
	});
	Hand->SetActivationPairQueued(CardA, CardB, false);
	Hand->SetActivationPairState(CardA, CardB, EActivationPairState::Ready);
	Hand->MulticastPairActivationCancelled(CardA, CardB);
	TryProcessQueuedPairActivations();
	if (IsValid(TurnComponent)) { TurnComponent->NotifyEffectTaskFinished(); }
	return true;
}

void ASHGameMode::RequestStoredPairActivation(ASHPlayerState* ActivatingPlayer, ASHCard* SelectedCard)
{
	if (!HasAuthority() || bReactionWindowOpen || !IsValid(ActivatingPlayer) || !IsValid(SelectedCard) ||
		SelectedCard->GetCardZone() != ECardZone::Activation)
	{
		return;
	}

	ASHHand* Hand = ActivatingPlayer->GetHand();
	FActivatedPair* Pair = IsValid(Hand) ? Hand->FindActivationPair(SelectedCard) : nullptr;
	if (!Pair || Pair->bActivated || Pair->State >= EActivationPairState::AbilityEffect)
	{
		return;
	}
	if (!IsValid(TurnComponent) || !TurnComponent->CanActivatePair(ActivatingPlayer, *Pair))
	{
		return;
	}

	const bool bAlreadyQueued = PendingPairActivations.ContainsByPredicate(
		[Pair](const FPendingPairActivation& Pending)
		{
			return (Pending.CardA == Pair->CardA && Pending.CardB == Pair->CardB) ||
				(Pending.CardA == Pair->CardB && Pending.CardB == Pair->CardA);
		});
	if (!bAlreadyQueued)
	{
		FPendingPairActivation& Pending = PendingPairActivations.AddDefaulted_GetRef();
		Pending.ActivatingPlayer = ActivatingPlayer;
		Pending.CardA = Pair->CardA;
		Pending.CardB = Pair->CardB;
		Hand->SetActivationPairQueued(Pending.CardA, Pending.CardB, true);
	}

	TryProcessQueuedPairActivations();
}

void ASHGameMode::NotifyActivationPairSettled(ASHCard* CardA, ASHCard* CardB)
{
	if (!HasAuthority())
	{
		return;
	}

	ASHGameState* SHGameState = GetGameState<ASHGameState>();
	if (IsValid(SHGameState))
	{
		for (APlayerState* State : SHGameState->PlayerArray)
		{
			ASHPlayerState* PlayerState = Cast<ASHPlayerState>(State);
			ASHHand* Hand = IsValid(PlayerState) ? PlayerState->GetHand() : nullptr;
			if (IsValid(Hand) && Hand->FindActivationPair(CardA))
			{
				Hand->SetActivationPairState(CardA, CardB, EActivationPairState::Ready);
				break;
			}
		}
	}
	TryProcessQueuedPairActivations();
}

void ASHGameMode::TryProcessQueuedPairActivations()
{
	if (!HasAuthority() || !IsValid(TurnComponent) || bProcessingPairActivations || bReactionWindowOpen)
	{
		return;
	}
	TGuardValue<bool> ProcessingGuard(bProcessingPairActivations, true);

	while (!PendingPairActivations.IsEmpty())
	{
		FPendingPairActivation& Pending = PendingPairActivations[0];
		ASHHand* Hand = IsValid(Pending.ActivatingPlayer) ? Pending.ActivatingPlayer->GetHand() : nullptr;
		FActivatedPair* Pair = IsValid(Hand) ? Hand->FindActivationPair(Pending.CardA) : nullptr;
		if (!Pair)
		{
			PendingPairActivations.RemoveAt(0);
			continue;
		}
		if (TurnComponent->HasNamedTurnTransitionBlocks())
		{
			return;
		}

		if (!Pending.bClickPresentationStarted)
		{
			if (Pair->State != EActivationPairState::Ready)
			{
				// Settlement will call us again; retrying the same head here would spin forever.
				return;
			}
			Pending.bClickPresentationStarted = true;
			Hand->SetActivationPairState(Pending.CardA, Pending.CardB,
				EActivationPairState::ClickPresentation);
			Hand->MulticastPairClicked(Pending.CardA, Pending.CardB);
			// Blueprint callbacks can change the queue. Reacquire its head on the next iteration.
			continue;
		}

		if (!Pending.bAbilityStarted)
		{
			if (!Pending.bReactionsChecked)
			{
				Pending.bReactionsChecked = true;
				const FPendingPairActivation ReactionTarget = Pending;
				if (BeginCardReactions(ReactionTarget)) { return; }
				continue; // A synchronous UI response may have removed or changed the queue head.
			}
			Pending.bAbilityStarted = true;
			const FPendingPairActivation ActivationToStart = Pending;
			StartQueuedPairAbility(ActivationToStart);
			// A synchronous completion can remove this entry and expose another ready pair.
			continue;
		}

		// Extra draws wait for a later draw gesture. Preserve their active tasks
		// and draw order, but let another pair (e.g. Paulus) resolve before drawing.
		const bool bDeferredDraw = ActiveEffectTasks.ContainsByPredicate(
			[&Pending](const UCardEffectTask* Task)
			{
				return IsValid(Task) && Task->GetCardA() == Pending.CardA &&
					Task->GetCardB() == Pending.CardB && !Task->BlocksNextPairActivation();
			});
		if (bDeferredDraw)
		{
			const FPendingPairActivation Deferred = Pending;
			PendingPairActivations.RemoveAt(0);
			Hand->SetActivationPairQueued(Deferred.CardA, Deferred.CardB, false);
			continue;
		}
		// Other effects own the queue until their task and final presentation finish.
		return;
	}
}

void ASHGameMode::StartQueuedPairAbility(const FPendingPairActivation& PendingActivation)
{
	ASHHand* Hand = IsValid(PendingActivation.ActivatingPlayer)
		? PendingActivation.ActivatingPlayer->GetHand() : nullptr;
	FActivatedPair* Pair = IsValid(Hand) ? Hand->FindActivationPair(PendingActivation.CardA) : nullptr;
	if (!Pair)
	{
		return;
	}

	Hand->SetActivationPairState(PendingActivation.CardA, PendingActivation.CardB,
		EActivationPairState::AbilityEffect);
	Hand->MulticastPairEffectActivated(PendingActivation.CardA, PendingActivation.CardB);
	CardActivateEffect(PendingActivation.ActivatingPlayer,
		PendingActivation.CardA, PendingActivation.CardB);
}

void ASHGameMode::FlushCompletedEffectPairs()
{
	if (!HasAuthority() || (IsValid(TurnComponent) && TurnComponent->HasNamedTurnTransitionBlocks()))
	{
		return;
	}

	const TArray<FCompletedEffectPair> MovesToApply = MoveTemp(CompletedEffectPairsWaitingForPresentation);
	CompletedEffectPairsWaitingForPresentation.Reset();
	{
		// Finish the entire resolved batch before any remaining activation sees the table.
		// In particular, a captured reaction must already be Ready when the root effect runs.
		TGuardValue<bool> Guard(bProcessingPairActivations, true);
		for (const FCompletedEffectPair& Move : MovesToApply)
		{
			if (Move.bMoveToVictoryStack)
			{
				MovePairToVictoryStack(Move.ActivatingPlayer, Move.CardA, Move.CardB);
			}
			else
			{
				CompleteQueuedPairActivation(Move.CardA, Move.CardB);
			}
		}
	}
	TryProcessQueuedPairActivations();
}

void ASHGameMode::CompleteQueuedPairActivation(ASHCard* CardA, ASHCard* CardB)
{
	const int32 PendingIndex = PendingPairActivations.IndexOfByPredicate(
		[CardA, CardB](const FPendingPairActivation& Pending)
		{
			return (Pending.CardA == CardA && Pending.CardB == CardB) ||
				(Pending.CardA == CardB && Pending.CardB == CardA);
		});
	if (PendingIndex != INDEX_NONE)
	{
		ASHPlayerState* Player = PendingPairActivations[PendingIndex].ActivatingPlayer;
		PendingPairActivations.RemoveAt(PendingIndex);
		if (IsValid(Player) && IsValid(Player->GetHand()))
		{
			Player->GetHand()->SetActivationPairQueued(CardA, CardB, false);
		}
	}
	TryProcessQueuedPairActivations();
}

void ASHGameMode::PassHandsToLeft()
{
	checkf(HasAuthority(), TEXT("Hands can only be passed on the server"));

	ASHGameState* SHGameState = GetGameState<ASHGameState>();
	checkf(IsValid(SHGameState), TEXT("Invalid SHGameState"));

	TArray<ASHHand*> ParticipantHands;
	for (int32 Seat = 0; Seat < SHGameState->GetParticipantCount(); ++Seat)
	{
		ASHHand* Hand = SHGameState->FindParticipantHandBySeat(Seat);
		check(IsValid(Hand));
		if (!Hand->IsProtectedFromCardEffects()) { ParticipantHands.Add(Hand); }
	}
	const int32 ParticipantCount = ParticipantHands.Num();
	if (ParticipantCount < 2) { return; }

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
		if (!IsValid(Hand) || SHPlayerState->IsProtectedFromCardEffects())
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

bool ASHGameMode::RequestHandCardSelection(UCardEffectTask* Task, ASHPlayerState* Player, const TArray<ASHCard*>& Cards)
{
	return RequestHandCardsSelection(Task, Player, IsValid(Player) ? Player->GetHand() : nullptr, Cards, 1, 1);
}

bool ASHGameMode::RequestHandCardsSelection(UCardEffectTask* Task, ASHPlayerState* Player, ASHHand* Source, const TArray<ASHCard*>& Cards, int32 Min, int32 Max)
{
	if (!HasAuthority() || !IsValid(Task) || !ActiveEffectTasks.Contains(Task) || !IsValid(Player) || !IsValid(Source) ||
		Min < 1 || Max < Min || Max > 3 || IsWaitingForPlayerSelection()) { return false; }
	ASHPlayerController* PC = Cast<ASHPlayerController>(Player->GetOwner());
	if (!IsValid(PC)) { return false; }
	FPendingPairSelection Pending;
	Pending.Task = Task;
	Pending.SourceHand = Source;
	Pending.MinCards = Min;
	Pending.MaxCards = Max;
	TArray<ASHCard*> Candidates;
	for (ASHCard* Card : Cards)
	{
		if (IsValid(Card) && Card->GetOwningHand() == Source && Card->GetCardZone() == ECardZone::Hand && Source->ContainsCard(Card))
		{
			Pending.CandidateCards.AddUnique(Card);
			Candidates.AddUnique(Card);
		}
	}
	if (Candidates.Num() < Min) { return false; }
	PendingHandCardSelections.Add(Player, Pending);
	SetPairTargetSelectionPresentation(Task, Player, true);
	PC->ClientRequestHandCardsSelection(Candidates, Min, Max);
	return true;
}
bool ASHGameMode::HasOtherActiveEffects(const UCardEffectTask* Except) const
{
	return ActiveEffectTasks.ContainsByPredicate([Except](const UCardEffectTask* Task)
	{
		return IsValid(Task) && Task != Except && !Task->IsFinished();
	});
}

bool ASHGameMode::TransferStoredPair(ASHHand* Source, ASHHand* Target, ASHCard* Card)
{
	if (!HasAuthority() || !IsValid(Source) || !IsValid(Target) || Source == Target) { return false; }
	const FActivatedPair* Found = Source->FindActivationPair(Card);
	if (!Found || Found->bActivated || Found->State != EActivationPairState::Ready) { return false; }
	const FActivatedPair Pair = *Found;
	// Ownership changes invalidate queued requests from the previous owner.
	TGuardValue<bool> Guard(bProcessingPairActivations, true);
	if (!Source->RemoveActivationPair(Pair.CardA, Pair.CardB)) { return false; }
	CompleteQueuedPairActivation(Pair.CardA, Pair.CardB);
	Target->ReceiveTransferredPair(Pair);
	for (APlayerState* State : GetGameState<ASHGameState>()->PlayerArray)
	{
		RefreshPlayerScore(Cast<ASHPlayerState>(State));
	}
	return true;
}

bool ASHGameMode::RemoveStoredPairFromGame(ASHHand* Hand, ASHCard* Card)
{
	PairCaptureRecipients.Remove(Card);
	if (!HasAuthority() || !IsValid(Hand)) { return false; }
	const FActivatedPair* Found = Hand->FindActivationPair(Card);
	if (!Found) { return false; }
	const FActivatedPair Pair = *Found;
	TGuardValue<bool> Guard(bProcessingPairActivations, true);
	if (!Hand->RemoveActivationPair(Pair.CardA, Pair.CardB)) { return false; }
	CompleteQueuedPairActivation(Pair.CardA, Pair.CardB);
	CompletedEffectPairsWaitingForPresentation.RemoveAll([&Pair](const FCompletedEffectPair& Entry)
	{
		return Entry.CardA == Pair.CardA || Entry.CardB == Pair.CardA;
	});
	for (ASHCard* Removed : {Pair.CardA.Get(), Pair.CardB.Get()})
	{
		if (IsValid(Removed)) { Removed->SetCardZone(ECardZone::None); Removed->Destroy(); }
	}
	for (APlayerState* State : GetGameState<ASHGameState>()->PlayerArray) { RefreshPlayerScore(Cast<ASHPlayerState>(State)); }
	return true;
}

void ASHGameMode::RotateActivationZonesRight(ASHCard* ExcludedCard)
{
	check(HasAuthority());
	ASHGameState* State = GetGameState<ASHGameState>();
	TArray<ASHPlayerState*> Players;
	for (APlayerState* Entry : State->PlayerArray)
	{
		if (ASHPlayerState* Player = Cast<ASHPlayerState>(Entry); IsValid(Player) && !Player->IsProtectedFromCardEffects() && IsValid(Player->GetHand())) { Players.Add(Player); }
	}
	Players.Sort([](const ASHPlayerState& A, const ASHPlayerState& B) { return A.GetSeatIndex() < B.GetSeatIndex(); });
	const int32 Count = Players.Num();
	if (Count < 2) { return; }
	TArray<TArray<FActivatedPair>> Zones;
	Zones.SetNum(Count);
	for (int32 Seat = 0; Seat < Count; ++Seat)
	{
		ASHHand* Hand = Players[Seat]->GetHand();
		if (!IsValid(Hand)) { return; }
		Zones[Seat] = Hand->GetLogicalActivationPairs();
	}
	TGuardValue<bool> Guard(bProcessingPairActivations, true);
	for (int32 Seat = 0; Seat < Count; ++Seat)
	{
		ASHHand* Source = Players[Seat]->GetHand();
		ASHHand* Target = Players[(Seat + Count - 1) % Count]->GetHand();
		for (const FActivatedPair& Pair : Zones[Seat])
		{
			if (Pair.CardA != ExcludedCard && Pair.CardB != ExcludedCard) { TransferStoredPair(Source, Target, Pair.CardA); }
		}
	}
}

void ASHGameMode::ShuffleAndRedealHands()
{
	check(HasAuthority());
	ASHGameState* State = GetGameState<ASHGameState>();
	TArray<ASHHand*> Hands = State->GetParticipantHands();
	Hands.RemoveAll([](const ASHHand* Hand) { return IsValid(Hand) && Hand->IsProtectedFromCardEffects(); });
	if (Hands.IsEmpty()) { return; }
	TArray<ASHCard*> Cards;
	for (ASHHand* Hand : Hands) { if (!IsValid(Hand)) { return; } }
	for (ASHHand* Hand : Hands)
	{
		const TArray<ASHCard*> HandCards = Hand->GetCards();
		for (ASHCard* Card : HandCards) { Hand->RemoveCard(Card); Cards.Add(Card); }
	}
	Algo::RandomShuffle(Cards);
	const int32 StartSeat = FMath::RandRange(0, Hands.Num() - 1);
	for (int32 Index = 0; Index < Cards.Num(); ++Index)
	{
		ASHHand* Hand = Hands[(StartSeat + Index) % Hands.Num()];
		Hand->AddCard(Cards[Index], Hand->GetCardCount());
	}
	for (APlayerState* Player : State->PlayerArray)
	{
		if (ASHPlayerController* PC = Cast<ASHPlayerController>(Player->GetOwner())) { PC->ClientReconcileRotatedHands(); }
	}
}

void ASHGameMode::SubmitHandCardSelection(ASHPlayerState* Player, ASHCard* Card)
{
	SubmitHandCardsSelection(Player, {Card});
}

void ASHGameMode::SubmitHandCardsSelection(ASHPlayerState* Player, const TArray<ASHCard*>& Cards)
{
	FPendingPairSelection* Pending = PendingHandCardSelections.Find(Player);
	if (!HasAuthority() || !Pending || !IsValid(Pending->SourceHand) || Cards.Num() < Pending->MinCards || Cards.Num() > Pending->MaxCards) { return; }
	TSet<ASHCard*> Unique;
	for (ASHCard* Card : Cards)
	{
		if (!IsValid(Card) || Unique.Contains(Card) || !Pending->CandidateCards.Contains(Card) || Card->GetOwningHand() != Pending->SourceHand ||
			Card->GetCardZone() != ECardZone::Hand || !Pending->SourceHand->ContainsCard(Card)) { return; }
		Unique.Add(Card);
	}
	UCardEffectTask* Task = Pending->Task;
	PendingHandCardSelections.Remove(Player);
	if (IsValid(Task) && ActiveEffectTasks.Contains(Task)) { Task->HandleHandCardsSelected(Cards); }
	FinishSelectionStep(Task, Player);
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
        if (IsValid(Candidate) && !Candidate->IsProtectedFromCardEffects() && ParticipantHands.Contains(Candidate))
        {
            Pending.Candidates.AddUnique(Candidate);
        }
    }

    if (Pending.Candidates.IsEmpty()) { PendingParticipantSelections.Remove(SelectingPlayer); Task->FinishEffect(); return; }
    ASHPlayerController* Controller = Cast<ASHPlayerController>(SelectingPlayer->GetOwner());
    checkf(IsValid(Controller), TEXT("Selecting player has no controller"));

    TArray<ASHHand*> ClientCandidates;
    for (ASHHand* Candidate : Pending.Candidates)
    {
        ClientCandidates.Add(Candidate);
    }
	SetPairTargetSelectionPresentation(Task, SelectingPlayer, true);
    Controller->ClientRequestParticipantSelection(ClientCandidates, Purpose);
}

void ASHGameMode::SubmitParticipantSelection(ASHPlayerState* SelectingPlayer, ASHHand* SelectedHand)
{
    FPendingParticipantSelection* Pending = PendingParticipantSelections.Find(SelectingPlayer);
    if (!Pending || !IsValid(SelectedHand) || SelectedHand->IsProtectedFromCardEffects() || !Pending->Candidates.Contains(SelectedHand))
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
	FinishSelectionStep(Task, SelectingPlayer);
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
	PendingSelection.Purpose = Purpose;
    for (ASHPlayerState* Candidate : Candidates)
    {
        if (IsValid(Candidate) && !Candidate->IsProtectedFromCardEffects())
        {
            PendingSelection.Candidates.AddUnique(Candidate);
        }
    }

    if (PendingSelection.Candidates.IsEmpty()) { PendingPlayerSelections.Remove(SelectingPlayer); Task->FinishEffect(); return; }

    ASHPlayerController* SelectingController = Cast<ASHPlayerController>(SelectingPlayer->GetOwner());
    checkf(IsValid(SelectingController), TEXT("Selecting player has no controller"));

    TArray<ASHPlayerState*> ClientCandidates;
    ClientCandidates.Reserve(PendingSelection.Candidates.Num());
    for (ASHPlayerState* Candidate : PendingSelection.Candidates)
    {
        ClientCandidates.Add(Candidate);
    }
	SetPairTargetSelectionPresentation(Task, SelectingPlayer, true);
	SelectingController->ClientRequestPlayerSelection(ClientCandidates, Purpose);
}

void ASHGameMode::SubmitPlayerSelection(ASHPlayerState* SelectingPlayer,
	ASHPlayerState* SelectedPlayer)
{
    FPendingPlayerSelection* PendingSelection = PendingPlayerSelections.Find(SelectingPlayer);
	if (!PendingSelection || !IsValid(SelectedPlayer) ||
		SelectedPlayer->IsProtectedFromCardEffects() || !PendingSelection->Candidates.Contains(SelectedPlayer))
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

		// The client may have optimistically cleared its pickers, or a duplicate
		// click from the previous step may arrive after a multi-step transition.
		// Re-send the authoritative current request so the game cannot deadlock.
		if (PendingSelection)
		{
			if (ASHPlayerController* Controller =
				Cast<ASHPlayerController>(SelectingPlayer->GetOwner()))
			{
				TArray<ASHPlayerState*> ClientCandidates;
				for (ASHPlayerState* Candidate : PendingSelection->Candidates)
				{
					if (IsValid(Candidate))
					{
						ClientCandidates.Add(Candidate);
					}
				}
				Controller->ClientRequestPlayerSelection(
					ClientCandidates, PendingSelection->Purpose);
			}
		}
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
	FinishSelectionStep(Task, SelectingPlayer);
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
        if (IsValid(Card) && Card->GetCardZone() == ECardZone::Activation && IsValid(Card->GetOwningHand()) && !Card->GetOwningHand()->IsProtectedFromCardEffects())
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
	SetPairTargetSelectionPresentation(Task, SelectingPlayer, true);
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

    if (!IsValid(PairOwner) || PairOwner->IsProtectedFromCardEffects())
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
	FinishSelectionStep(Task, SelectingPlayer);
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

    const auto* ImmediateA = UCardDefinition::FindFragmentByClass(CardA->GetCardDefinition(), UImmediateVictoryPairFragment::StaticClass());
    const auto* ImmediateB = UCardDefinition::FindFragmentByClass(CardB->GetCardDefinition(), UImmediateVictoryPairFragment::StaticClass());
    if (ImmediateA || ImmediateB)
    {
        if (!AreCardsPairCompatible(CardA, CardB) || !IsValid(Hand->GetVictoryStack())) { return; }
        ASHCard* Partner = ImmediateA ? CardB : CardA;
        // Pairing is not an activation: it neither runs Paulus nor opens a task.
        Hand->RemoveCard(CardA);
        Hand->RemoveCard(CardB);
        Hand->GetVictoryStack()->AddPair(CardA, CardB);
        CardA->Reveal(); CardB->Reveal();
        for (TActorIterator<ASHCard> It(GetWorld()); It; ++It)
        {
            ASHCard* Other = *It;
            if (Other == CardA || Other == CardB || Other->GetCardDefinition() != Partner->GetCardDefinition()) { continue; }
            if (ASHHand* OtherHand = Other->GetOwningHand())
            {
                if (FActivatedPair* Pair = OtherHand->FindActivationPair(Other))
                {
                    const FActivatedPair Copy = *Pair;
                    ASHCard* Survivor = Copy.CardA == Other ? Copy.CardB.Get() : Copy.CardA.Get();
                    OtherHand->RemoveActivationPair(Copy.CardA, Copy.CardB);
                    CompleteQueuedPairActivation(Copy.CardA, Copy.CardB);
                    if (IsValid(Survivor)) { OtherHand->AddCard(Survivor, OtherHand->GetCardCount()); }
                }
                else { OtherHand->RemoveCard(Other); }
            }
            else if (AVictoryStack* Stack = Cast<AVictoryStack>(Other->GetOwner())) { Stack->RemoveCard(Other); }
            Other->SetCardZone(ECardZone::None);
            Other->Destroy();
            break;
        }
        for (APlayerState* State : GetGameState<ASHGameState>()->PlayerArray) { RefreshPlayerScore(Cast<ASHPlayerState>(State)); }
        if (TurnComponent) { TurnComponent->NotifyPairSettled(CardA, CardB); }
        return;
    }

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

    return UCardDefinition::ArePairDefinitionsCompatible(DefinitionA.Get(), DefinitionB.Get());

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
    if (!IsValid(SHGameState) || SHGameState->IsGameEnded() || bReactionWindowOpen || !ActiveEffectTasks.IsEmpty())
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

    int32 HighestScore = MIN_int32;
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
