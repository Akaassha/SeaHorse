// Fill out your copyright notice in the Description page of Project Settings.


#include "SeaHorse/Gameplay/SHHand.h"
#include "Net/UnrealNetwork.h"
#include "SeaHorse/Gameplay/Cards/SHCard.h"

// Sets default values
ASHHand::ASHHand()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

	bReplicates = true;
}

void ASHHand::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ASHHand, Cards);
}

// Called when the game starts or when spawned
void ASHHand::BeginPlay()
{
	Super::BeginPlay();
	
}

// Called every frame
void ASHHand::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

}

void ASHHand::AddCard(ASHCard* Card, int32 Index)
{
    checkf(HasAuthority(), TEXT("AddCard can only be called on the server"));
    checkf(IsValid(Card), TEXT("Cannot add invalid card to hand"));
    checkf(Index >= 0 && Index <= Cards.Num(), TEXT("Invalid hand index: %d"), Index);

    Card->SetOwner(this);
    Cards.Insert(Card, Index);

    UE_LOG(
        LogTemp,
        Warning,
        TEXT("SERVER: Added %s to %s | Cards: %d"),
        *GetNameSafe(Card),
        *GetName(),
        Cards.Num()
    );
}

TArray<ASHCard*> ASHHand::GetCards()
{
    return Cards;
}

void ASHHand::RemoveCard(ASHCard* Card)
{
    checkf(HasAuthority(), TEXT("RemoveCard can only be called on the server"));
    checkf(IsValid(Card), TEXT("Cannot remove invalid card from hand"));
    checkf(Cards.Contains(Card), TEXT("Card %s is not in this hand"), *GetNameSafe(Card));

    Cards.RemoveSingle(Card);
}

void ASHHand::OnRep_Cards()
{
    UE_LOG(LogTemp, Warning, TEXT("Hand %s updated. Count: %d"), *GetName(), Cards.Num());

    for (int32 Index = 0; Index < Cards.Num(); ++Index)
    {
        UE_LOG(
            LogTemp,
            Warning,
            TEXT("[%d] %s"),
            Index,
            *GetNameSafe(Cards[Index])
        );
    }
}

