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

	const TArray<ASHHand*> ParticipantHands = SHGameState->GetParticipantHands();
	if (ParticipantHands.Num() != 4)
	{
		UE_LOG(LogTemp, Warning, TEXT("[SH_INIT] -> WAIT: expected 4 participant hands, received %d"),
			ParticipantHands.Num());
		return;
	}

	TSet<ASHHand*> HumanHands;
	for (APlayerState* State : SHGameState->PlayerArray)
	{
		ASHPlayerState* ReplicatedPlayerState = Cast<ASHPlayerState>(State);
		if (!IsValid(ReplicatedPlayerState) || ReplicatedPlayerState->GetSeatIndex() == INDEX_NONE ||
			!IsValid(ReplicatedPlayerState->GetHand()))
		{
			UE_LOG(LogTemp, Warning, TEXT("[SH_INIT] -> WAIT: incomplete replicated player/hand assignment"));
			return;
		}
		HumanHands.Add(ReplicatedPlayerState->GetHand());
	}

	for (ASHHand* Hand : ParticipantHands)
	{
		if (IsValid(Hand) && !Hand->IsLogicalNPC() && !HumanHands.Contains(Hand))
		{
			UE_LOG(LogTemp, Warning,
				TEXT("[SH_INIT] -> WAIT: human hand %s has no replicated PlayerState"), *GetNameSafe(Hand));
			return;
		}
	}

    int32 ReceivedCardCount = 0;

    for (ASHHand* Hand : ParticipantHands)
    {
        if (!IsValid(Hand))
        {
            UE_LOG(LogTemp, Warning, TEXT("[SH_INIT] -> WAIT: invalid participant hand"));
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
            TEXT("[SH_INIT] Hand=%s LogicalSeat=%d IsNPC=%d Cards=%d Valid=%d"),
            *GetNameSafe(Hand),
            Hand->GetLayoutSeatIndex(),
            Hand->IsLogicalNPC(),
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

void ASHPlayerController::ClientRequestActivationPairSelection_Implementation(
    const TArray<ASHCard*>& CandidateCards)
{
    OnActivationPairSelectionRequested(CandidateCards);
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

    TArray<AActor*> Hands;
    UGameplayStatics::GetAllActorsOfClass(GetWorld(), ASHHand::StaticClass(), Hands);
    for (AActor* Actor : Hands)
    {
        ASHHand* VisualHand = Cast<ASHHand>(Actor);
        if (IsValid(VisualHand) && VisualHand->GetRepresentedHand() == LogicalHand)
        {
            return VisualHand;
        }
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

    const int32 PlayerCount = SHGameState->GetParticipantCount();

    TArray<ASHHand*> HandsToUpdate;

	for (ASHHand* LogicalHand : SHGameState->GetParticipantHands())
    {
        const int32 VisualSeatIndex =
            GetVisualSeatIndex(
                LogicalHand->GetLayoutSeatIndex(),
                PlayerCount
            );

        ASHHand* VisualHand =
            FindLayoutHand(VisualSeatIndex);

        checkf(
            IsValid(VisualHand),
            TEXT("No Hand for VisualSeatIndex %d"),
            VisualSeatIndex
        );

        
        ASHPlayerState* ControllingPlayer = nullptr;
		for (APlayerState* State : SHGameState->PlayerArray)
		{
			ASHPlayerState* Candidate = Cast<ASHPlayerState>(State);
			if (IsValid(Candidate) && Candidate->GetHand() == LogicalHand)
			{
				ControllingPlayer = Candidate;
				break;
			}
		}
		if (IsValid(ControllingPlayer)) VisualHand->SetRepresentedPlayerState(ControllingPlayer);
		else VisualHand->SetRepresentedHand(LogicalHand);

		const bool bIsLocalPlayer = ControllingPlayer == LocalPlayerState;

        VisualHand->SetShowCardFronts(bIsLocalPlayer);

        UE_LOG(
            LogTemp,
            Warning,
            TEXT("[HAND VIEW] VisualSeat=%d Hand=%s LogicalHand=%s IsNPC=%d Player=%s%s"),
            VisualSeatIndex,
            *GetNameSafe(VisualHand),
			*GetNameSafe(LogicalHand), LogicalHand->IsLogicalNPC(),
			*GetNameSafe(ControllingPlayer),
            ControllingPlayer == LocalPlayerState
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

	// UpdateCardPositions is Blueprint-defined for regular hands. Apply the
	// native NPC stack layout afterwards so every client gets the same stacked
	// presentation at its locally rotated visual seat.
	for (ASHHand* NPCHand : SHGameState->GetNPCHands())
	{
		const int32 VisualSeatIndex = GetVisualSeatIndex(NPCHand->GetLayoutSeatIndex(), PlayerCount);
		if (ASHHand* VisualHand = FindLayoutHand(VisualSeatIndex))
		{
			VisualHand->LayoutNPCStack(NPCHand);
		}
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

        if (IsValid(Hand) && Hand->GetLayoutSeatIndex() == LayoutSeatIndex)
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

    ASHPlayerState* SHPlayerState = GetPlayerState<ASHPlayerState>();
    ASHGameMode* SHGameMode = GetWorld()->GetAuthGameMode<ASHGameMode>();
    if (IsValid(SHPlayerState) && IsValid(SHGameMode) &&
        SHGameMode->SubmitActivationPairSelection(SHPlayerState, Card))
    {
        return;
    }

    if (Card->GetCardZone() != ECardZone::Activation)
    {
        return;
    }

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

    const bool bSourceIsNPC = SourceHand->IsLogicalNPC();
    if (bSourceIsNPC)
    {
        ASHCard* TopCard = SourceHand->GetTopCard();
        if (!IsValid(TopCard))
        {
            return;
        }

        // An NPC pile is one interaction target. Its cards overlap and a local
        // cursor trace may identify a covered card while replication/layout is
        // settling. Always resolve that click to the authoritative stack top.
        if (Card != TopCard)
        {
            UE_LOG(LogTemp, Log,
                TEXT("[SH_DRAW][NPC_TOP] Requested=%s ResolvedTop=%s Source=%s"),
                *GetNameSafe(Card), *GetNameSafe(TopCard), *GetNameSafe(SourceHand));
            Card = TopCard;
        }
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

    UE_LOG(LogTemp, Log,
        TEXT("[SH_DRAW] Card=%s Source=%s SourceType=%s SourceCards=%d TopCard=%s Target=%s InsertIndex=%d"),
        *GetNameSafe(Card), *GetNameSafe(SourceHand),
        bSourceIsNPC ? TEXT("NPC") : TEXT("Player"),
        SourceHand->GetCardCount(),
        bSourceIsNPC ? *GetNameSafe(SourceHand->GetTopCard()) : TEXT("N/A"),
        *GetNameSafe(TargetHand), InsertIndex);

    UTurnComponent* TurnComponent = SHGameMode->GetTurnComponent();
    if (!IsValid(TurnComponent) || !TurnComponent->CanDrawCardFromHand(SHPlayerState, SourceHand))
    {
        UE_LOG(LogTemp, Warning,
            TEXT("[SH_DRAW][REJECT] Draw rules rejected Card=%s Source=%s Player=%s"),
            *GetNameSafe(Card), *GetNameSafe(SourceHand), *GetNameSafe(SHPlayerState));
        return;
    }

    if (bSourceIsNPC)
    {
        SourceHand->TakeTopCard();
    }
    else
    {
        SourceHand->RemoveCard(Card);
    }
    TargetHand->AddCard(Card, InsertIndex);

    UE_LOG(LogTemp, Log,
        TEXT("[SH_DRAW][ACCEPT] Card=%s Source=%s SourceCardsAfter=%d Target=%s TargetCardsAfter=%d"),
        *GetNameSafe(Card), *GetNameSafe(SourceHand), SourceHand->GetCardCount(),
        *GetNameSafe(TargetHand), TargetHand->GetCardCount());

    ClientReceiveCardDefinition(
        Card,
        Card->GetCardDefinition()
    );

    TurnComponent->HandleCardDrawnFromHand(SHPlayerState, SourceHand);
}

