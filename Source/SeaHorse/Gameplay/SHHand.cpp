// Fill out your copyright notice in the Description page of Project Settings.


#include "SeaHorse/Gameplay/SHHand.h"
#include "Net/UnrealNetwork.h"
#include "SeaHorse/Gameplay/Cards/SHCard.h"
#include "SeaHorse/Gameplay/Core/SHPlayerController.h"
#include "Kismet/GameplayStatics.h"

// Sets default values
ASHHand::ASHHand()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

	bReplicates = true;
    SetReplicateMovement(false);


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
	
    LayoutTransform = GetActorTransform();
}

void ASHHand::SetShowCardFronts(bool bShow)
{
    bShowCardFronts = bShow;

    for (ASHCard* Card : Cards)
    {
        if (IsValid(Card))
        {
            Card->SetFaceUp(bShowCardFronts);
        }
    }
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

    UpdateCardPositions();
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

    UpdateCardPositions();
}

void ASHHand::OnRep_Cards()
{
    ASHPlayerController* PC =
        Cast<ASHPlayerController>(
            UGameplayStatics::GetPlayerController(this, 0)
        );

    if (IsValid(PC) && !PC->IsTableViewInitialized())
    {
        PC->TrySetupTableView();
        return;
    }

    int32 ValidCards = 0;

    for (ASHCard* Card : Cards)
    {
        if (IsValid(Card))
        {
            ++ValidCards;
        }
    }

    UE_LOG(LogTemp, Warning,
        TEXT("[SH_INIT][%.3f][HAND] OnRep_Cards | Hand=%s LayoutSeat=%d Cards=%d Valid=%d"),
        GetWorld()->GetTimeSeconds(),
        *GetNameSafe(this),
        LayoutSeatIndex,
        Cards.Num(),
        ValidCards);

    // Normalne zmiany rêki ju¿ podczas gry.
    UpdateCardPositions();
}

int32 ASHHand::GetCardCount() const
{
    return Cards.Num();
}

int32 ASHHand::GetLayoutSeatIndex() const
{
    return LayoutSeatIndex;
}

const FTransform& ASHHand::GetLayoutTransform() const
{
    return LayoutTransform;
}