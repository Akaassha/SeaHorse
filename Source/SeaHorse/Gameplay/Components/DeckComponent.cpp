// Fill out your copyright notice in the Description page of Project Settings.


#include "SeaHorse/Gameplay/Components/DeckComponent.h"
#include "SeaHorse/Gameplay/Cards/SHCard.h"
#include "SeaHorse/Gameplay/Cards/CardDefinition.h"
#include "SeaHorse/Gameplay/Core/SHGameState.h"
#include "SeaHorse/Gameplay/Core/SHPlayerState.h"
#include "SeaHorse/Gameplay/SHHand.h"
#include "Algo/RandomShuffle.h"
#include "Engine/DataTable.h"
#include "Kismet/GameplayStatics.h"

// Sets default values for this component's properties
UDeckComponent::UDeckComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

// ***** Begin Handle Deck *****
void UDeckComponent::CreateDeck()
{
    checkf(GetOwner() && GetOwner()->HasAuthority(), TEXT("Deck can only be created on the server"));
    checkf(DeckDefinition, TEXT("DeckDefinition is not set"));
    checkf(CardClass, TEXT("CardClass is not set"));
    checkf(Deck.IsEmpty(), TEXT("CreateDeck called while Deck is not empty"));

    TArray<FDeckEntry*> DeckEntries;
    DeckDefinition->GetAllRows<FDeckEntry>(
        TEXT("UDeckComponent::CreateDeck"),
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

    InitialDeckSize = Deck.Num();
}

void UDeckComponent::ShuffleDeck()
{
    checkf(GetOwner() && GetOwner()->HasAuthority(), TEXT("Deck can only be shuffled on the server"));
    Algo::RandomShuffle(Deck);
}

ASHPlayerState* UDeckComponent::DealCards()
{
    checkf(GetOwner() && GetOwner()->HasAuthority(), TEXT("Cards can only be dealt on the server"));

    ASHGameState* SHGameState = GetWorld()->GetGameState<ASHGameState>();
    checkf(IsValid(SHGameState), TEXT("Invalid SHGameState"));
    checkf(!SHGameState->PlayerArray.IsEmpty(), TEXT("Cannot deal cards without players"));

    ASHPlayerState* FirstPlayer = ChooseFirstDealtPlayer();
    checkf(IsValid(FirstPlayer), TEXT("ChooseDealingStartPlayer returned invalid player"));

    int32 ParticipantSeat = FirstPlayer->GetSeatIndex();
    checkf(ParticipantSeat != INDEX_NONE, TEXT("Chosen dealing start player has no seat"));
    const int32 ParticipantCount = SHGameState->GetParticipantCount();
    checkf(ParticipantCount >= 2, TEXT("Cannot deal cards without at least two participants"));

    int32 LastDealtSeat = INDEX_NONE;

    while (!Deck.IsEmpty())
    {
        ASHHand* Hand = SHGameState->FindParticipantHandBySeat(ParticipantSeat);
        checkf(IsValid(Hand), TEXT("Participant in seat %d has no card container"), ParticipantSeat);

        ASHCard* Card = Deck.Pop();

        checkf(IsValid(Card), TEXT("Deck contains invalid Card"));

        Hand->AddCard(Card, Hand->GetCardCount());
        LastDealtSeat = ParticipantSeat;

		UE_LOG(LogTemp, Verbose, TEXT("[SH_DEAL] Card=%s Seat=%d Container=%s IsNPC=%d Count=%d"),
			*GetNameSafe(Card), ParticipantSeat, *GetNameSafe(Hand), Hand->IsLogicalNPC(), Hand->GetCardCount());

        ParticipantSeat = (ParticipantSeat + 1) % ParticipantCount;

    }

    checkf(LastDealtSeat != INDEX_NONE, TEXT("No cards were dealt"));

    // BN turns are skipped. Start with the last dealt human, or the next human
    // clockwise when the final card went to a BN.
    for (int32 Offset = 0; Offset < ParticipantCount; ++Offset)
    {
        const int32 CandidateSeat = (LastDealtSeat + Offset) % ParticipantCount;
        for (APlayerState* State : SHGameState->PlayerArray)
        {
            ASHPlayerState* Candidate = Cast<ASHPlayerState>(State);
            if (IsValid(Candidate) && Candidate->GetSeatIndex() == CandidateSeat)
            {
                return Candidate;
            }
        }
    }

    checkNoEntry();
    return nullptr;
}

ASHPlayerState* UDeckComponent::ChooseFirstDealtPlayer_Implementation()
{
    ASHGameState* SHGameState = GetWorld()->GetGameState<ASHGameState>();
    checkf(IsValid(SHGameState), TEXT("Invalid SHGameState"));
    checkf(!SHGameState->PlayerArray.IsEmpty(), TEXT("Cannot choose a player without players"));

    const int32 RandomIndex = FMath::RandRange(0, SHGameState->PlayerArray.Num() - 1);
    return CastChecked<ASHPlayerState>(SHGameState->PlayerArray[RandomIndex]);
}
