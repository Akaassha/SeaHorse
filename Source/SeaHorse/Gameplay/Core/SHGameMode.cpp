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

	checkf(HandClass, TEXT("HandClass is not configured in %s"), *GetNameSafe(this));
	checkf(IsValid(NewPlayer), TEXT("HandleStartingNewPlayer received invalid PlayerController"));

	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = NewPlayer;

	ASHHand* Hand = GetWorld()->SpawnActor<ASHHand>(
		HandClass,
		FVector::ZeroVector,
		FRotator::ZeroRotator,
		SpawnParams
	);

	checkf(IsValid(Hand), TEXT("Failed to spawn Hand for player %s"), *GetNameSafe(NewPlayer));

	ASHPlayerState* SHPlayerState = NewPlayer->GetPlayerState<ASHPlayerState>();

	checkf(IsValid(SHPlayerState), TEXT("Player %s does not have a valid ASHPlayerState"), *GetNameSafe(NewPlayer));

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

            UE_LOG(LogTemp, Warning, TEXT("BEFORE SPAWN | Definition: %s"), *GetNameSafe(Entry->CardDefinition.Get()));

            ASHCard* Card = GetWorld()->SpawnActorDeferred<ASHCard>(CardClass, SpawnTransform);

            checkf(IsValid(Card), TEXT("Failed to spawn card"));

            Card->SetCardDefinition(Entry->CardDefinition);

            UE_LOG(
                LogTemp,
                Warning,
                TEXT("AFTER SET | Definition: %s"),
                *GetNameSafe(Card->GetCardDefinition().Get())
            );

            UGameplayStatics::FinishSpawningActor(Card, SpawnTransform);

            UE_LOG(
                LogTemp,
                Warning,
                TEXT("AFTER FINISH SPAWN | Definition: %s"),
                *GetNameSafe(Card->GetCardDefinition().Get())
            );

            Card->Initialize();
            Deck.Add(Card);
        }
    }


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

    StartGame();
}

void ASHGameMode::StartGame()
{
    UE_LOG(LogTemp, Warning, TEXT("StartGame - creating deck"));

    AssignSeats();

    CreateDeck();
    ShuffleDeck();
    DealCards();
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

}
