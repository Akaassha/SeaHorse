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
#include "EngineUtils.h"

void ASHGameMode::PostLogin(APlayerController* NewPlayer)
{
	Super::PostLogin(NewPlayer);

}

void ASHGameMode::Logout(AController* Exiting)
{
	Super::Logout(Exiting);

}

void ASHGameMode::HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer)
{
    Super::HandleStartingNewPlayer_Implementation(NewPlayer);

    checkf(
        IsValid(NewPlayer),
        TEXT("HandleStartingNewPlayer received invalid PlayerController")
    );

    ASHPlayerState* SHPlayerState =
        NewPlayer->GetPlayerState<ASHPlayerState>();

    checkf(
        IsValid(SHPlayerState),
        TEXT("Player %s does not have a valid ASHPlayerState"),
        *GetNameSafe(NewPlayer)
    );

    ASHHand* Hand = FindAvailableHand();

    checkf(
        IsValid(Hand),
        TEXT("No available Hand found in level for player %s"),
        *GetNameSafe(NewPlayer)
    );

    Hand->SetOwner(NewPlayer);

    SHPlayerState->SetHand(Hand);

    TryStartGame();
}

void ASHGameMode::StartPlay()
{
    Super::StartPlay();

}

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
    
    const TSubclassOf<UCardDefinition> DefinitionA =
        CardA->GetCardDefinition();
    
    const TSubclassOf<UCardDefinition> DefinitionB =
        CardB->GetCardDefinition();
    
    if (!IsValid(DefinitionA) || !IsValid(DefinitionB))
    {
        return false;
    }
    
    return DefinitionA == DefinitionB;
    
}

void ASHGameMode::ShuffleDeck()
{
    Algo::RandomShuffle(Deck);
}

void ASHGameMode::DealCards()
{
    ASHGameState* SHGameState = GetGameState<ASHGameState>();

    checkf(IsValid(SHGameState), TEXT("Invalid SHGameState"));
    checkf(!SHGameState->PlayerArray.IsEmpty(), TEXT("Cannot deal cards without players"));

    int32 PlayerIndex = FMath::RandRange(0, SHGameState->PlayerArray.Num() - 1);

    while (!Deck.IsEmpty())
    {
        ASHPlayerState* PlayerState = Cast<ASHPlayerState>(SHGameState->PlayerArray[PlayerIndex]);

        checkf(IsValid(PlayerState), TEXT("PlayerState at index %d is not ASHPlayerState"), PlayerIndex);

        ASHHand* Hand = PlayerState->GetHand();

        checkf(IsValid(Hand),TEXT("Player %s has no Hand"),*GetNameSafe(PlayerState));

        ASHCard* Card = Deck.Pop();

        checkf(IsValid(Card), TEXT("Deck contains invalid Card"));

        Hand->AddCard(Card, Hand->GetCardCount());

        PlayerIndex = (PlayerIndex + 1) % SHGameState->PlayerArray.Num();

    }
}

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

    SHGameState->SetMatchReady(true);
}

void ASHGameMode::AssignSeats()
{
    ASHGameState* SHGameState = GetGameState<ASHGameState>();

    checkf(IsValid(SHGameState), TEXT("Invalid SHGameState"));
    checkf(!SHGameState->PlayerArray.IsEmpty(), TEXT("Cannot assign seats without players"));

    for (int32 SeatIndex = 0; SeatIndex < SHGameState->PlayerArray.Num(); ++SeatIndex)
    {
        ASHPlayerState* PlayerState = Cast<ASHPlayerState>(SHGameState->PlayerArray[SeatIndex]);

        checkf(IsValid(PlayerState), TEXT("PlayerState at index %d is not ASHPlayerState"), SeatIndex);

        PlayerState->SetSeatIndex(SeatIndex);
    }

   //for (APlayerState* CurrentPlayerState : SHGameState->PlayerArray)
   //{
   //    ASHPlayerState* SHPlayerState =
   //        CastChecked<ASHPlayerState>(CurrentPlayerState);
   //
   //    ASHHand* Hand = SHPlayerState->GetHand();
   //
   //    Hand->Initialize();
   //    Hand->UpdateCardPositions();
   //}

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

        // Rêce ustawione na levelu maj¹ LayoutSeatIndex
        if (Hand->GetLayoutSeatIndex() == INDEX_NONE)
        {
            continue;
        }

        // Jeœli ma Ownera, zosta³a ju¿ przypisana graczowi
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
