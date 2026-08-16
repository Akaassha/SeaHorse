// Fill out your copyright notice in the Description page of Project Settings.


#include "SeaHorse/Gameplay/SHHand.h"
#include "Net/UnrealNetwork.h"
#include "SeaHorse/Gameplay/Cards/SHCard.h"
#include "SeaHorse/Gameplay/Core/SHPlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "SeaHorse/Gameplay/Core/SHPlayerState.h"
#include "Engine/EngineBaseTypes.h"

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
    DOREPLIFETIME(ASHHand, ActivatonCards);
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

void ASHHand::AddActivationPair(ASHCard* CardA, ASHCard* CardB)
{
    FActivatedPair Pair;
    Pair.CardA = CardA;
    Pair.CardB = CardB;
    ActivatonCards.Add(Pair);
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

    UE_LOG(LogTemp, Warning,
        TEXT("INSIDE ADD: this=%s CardOwnerBefore=%s Card=%s"),
        *GetNameSafe(this),
        *GetNameSafe(Card ? Card->GetOwner() : nullptr),
        *GetNameSafe(Card));

    Card->SetOwner(this);
    Cards.Insert(Card, Index);

    RefreshCardsPresentation();
    UpdateCardPositions();
}

TArray<ASHCard*> ASHHand::GetCards()
{
    return Cards;
}

TArray<FActivatedPair> ASHHand::GetActivationPairs()
{
    return ActivatonCards;
}

TArray<ASHCard*> ASHHand::GetActivationCards()
{
    TArray<ASHCard*> ReturnCards;
    ReturnCards.Reserve(ActivatonCards.Num() * 2);

    for (const FActivatedPair& Pair : ActivatonCards)
    {
        if (IsValid(Pair.CardA))
        {
            ReturnCards.Add(Pair.CardA);
        }

        if (IsValid(Pair.CardB))
        {
            ReturnCards.Add(Pair.CardB);
        }
    }

    return ReturnCards;
}

void ASHHand::RemoveCard(ASHCard* Card)
{
    checkf(HasAuthority(), TEXT("RemoveCard can only be called on the server"));
    checkf(IsValid(Card), TEXT("Cannot remove invalid card from hand"));
    //checkf(Cards.Contains(Card), TEXT("Card %s is not in this hand"), *GetNameSafe(Card));

    const int32 RemovedCount = Cards.RemoveSingle(Card);
    Card->SetFaceUp(false);

    UE_LOG(LogTemp, Warning,
        TEXT("AddCard: Hand=%s ShowFronts=%s Card=%s"),
        *GetName(),
        bShowCardFronts ? TEXT("TRUE") : TEXT("FALSE"),
        *GetNameSafe(Card));

    RefreshCardsPresentation();
    UpdateCardPositions();
}

void ASHHand::OnRep_Cards()
{
    ASHPlayerController* PC = Cast<ASHPlayerController>( UGameplayStatics::GetPlayerController(this, 0));

    if (IsValid(PC) && !PC->IsTableViewInitialized())
    {
        PC->TrySetupTableView();
        return;
    }

    RefreshCardsPresentation();
    UpdateCardPositions();

    PreviousCards = Cards;
}

void ASHHand::OnRep_ActivatonCards()
{

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

void ASHHand::RefreshCardsPresentation()
{
    APlayerController* LocalPC = GetWorld()->GetFirstPlayerController();
    ASHPlayerState* LocalPS = LocalPC
        ? LocalPC->GetPlayerState<ASHPlayerState>()
        : nullptr;

    ASHHand* LocalHand = LocalPS
        ? LocalPS->GetHand()
        : nullptr;

    const bool bIsListenServer = GetNetMode() == NM_ListenServer;
    UE_LOG(LogTemp, Warning,
        TEXT("[%s] Hand=%s LocalHand=%s ShowFronts=%s"),
        bIsListenServer ? TEXT("LISTEN SERVER") : TEXT("CLIENT"),
        *GetNameSafe(this),
        *GetNameSafe(LocalHand),
        bShowCardFronts ? TEXT("TRUE") : TEXT("FALSE"));

    for (ASHCard* Card : Cards)
    {
        if (IsValid(Card))
        {
            UE_LOG(LogTemp, Warning,
                TEXT("FACE: Hand=%s Card=%s -> %s"),
                *GetNameSafe(this),
                *GetNameSafe(Card),
                bShowCardFronts ? TEXT("FRONT") : TEXT("BACK"));

            Card->SetFaceUp(bShowCardFronts);
        }
    }
}

bool ASHHand::ContainsCard(ASHCard* CardB)
{
    return Cards.Contains(CardB);
}

bool ASHHand::ShouldShowCardFronts()
{
    const APlayerController* LocalPC = GetWorld()->GetFirstPlayerController();

    if (!IsValid(LocalPC))
    {
        return false;
    }

    ASHPlayerState* LocalPS = LocalPC->GetPlayerState<ASHPlayerState>();

    if (!IsValid(LocalPS))
    {
        return false;
    }

    return (LocalPS->GetHand() == this);
}
