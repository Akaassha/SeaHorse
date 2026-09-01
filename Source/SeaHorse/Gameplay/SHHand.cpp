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
    DOREPLIFETIME(ASHHand, ActivationPairs);
}

bool ASHHand::RemoveActivationPair(ASHCard* CardA, ASHCard* CardB)
{
    checkf(HasAuthority(), TEXT("RemoveActivationPair must be called on server"));

    if (!IsValid(CardA) || !IsValid(CardB))
    {
        return false;
    }

    const int32 PairIndex = ActivationPairs.IndexOfByPredicate(
        [CardA, CardB](const FActivatedPair& Pair)
        {
            return
                (Pair.CardA == CardA && Pair.CardB == CardB) ||
                (Pair.CardA == CardB && Pair.CardB == CardA);
        });

    if (PairIndex == INDEX_NONE)
    {
        return false;
    }

    ActivationPairs.RemoveAt(PairIndex);

    //UpdateActivationCardsLayout();

    return true;
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
    RefreshCardsPresentation();
}

void ASHHand::MulticastPairEffectActivated_Implementation(ASHCard* CardA, ASHCard* CardB)
{
    if (IsValid(CardA) && IsValid(CardB))
    {
        OnPairEffectActivated(CardA, CardB);
    }
   
}

void ASHHand::AddActivationPair(ASHCard* CardA, ASHCard* CardB)
{
    if (!HasAuthority())
    {
        return;
    }

    ASHHand* RepresentedHand = GetRepresentedHand();
    ASHHand* LogicalHand = IsValid(RepresentedHand) ? RepresentedHand : this;
    LogicalHand->AddActivationPairToLogicalHand(CardA, CardB);
}

void ASHHand::AddActivationPairToLogicalHand(ASHCard* CardA, ASHCard* CardB)
{
    checkf(HasAuthority(), TEXT("Activation pairs can only be added on the server"));

    checkf(IsValid(CardA) && IsValid(CardB) && CardA != CardB, TEXT("Invalid activation pair"));

    const bool bPairAlreadyExists = ActivationPairs.ContainsByPredicate(
        [CardA, CardB](const FActivatedPair& ExistingPair)
        {
            return
                (ExistingPair.CardA == CardA && ExistingPair.CardB == CardB) ||
                (ExistingPair.CardA == CardB && ExistingPair.CardB == CardA);
        });

    if (bPairAlreadyExists)
    {
        return;
    }

    FActivatedPair Pair;

    CardA->SetCardZone(ECardZone::Activation);
    CardB->SetCardZone(ECardZone::Activation);

    Pair.CardA = CardA;
    Pair.CardB = CardB;
    ActivationPairs.Add(Pair);

    UE_LOG(
        LogTemp,
        Warning,
        TEXT("[SH_ACTIVATION][SERVER_ADD] LogicalHand=%s LayoutSeat=%d CardA=%s CardB=%s PairCount=%d"),
        *GetNameSafe(this),
        LayoutSeatIndex,
        *GetNameSafe(CardA),
        *GetNameSafe(CardB),
        ActivationPairs.Num());

    ForceNetUpdate();
    OnRep_ActivationPairs();
}

void ASHHand::RefreshActivationPairsPresentation()
{
    const ASHHand* RepresentedHand = GetRepresentedHand();
    const TArray<FActivatedPair>& PairsToPresent = IsValid(RepresentedHand)
        ? RepresentedHand->ActivationPairs
        : ActivationPairs;

    UE_LOG(
        LogTemp,
        Warning,
        TEXT("[SH_ACTIVATION][PRESENT] VisualHand=%s VisualLayoutSeat=%d RepresentedHand=%s RepresentedLayoutSeat=%d PairCount=%d"),
        *GetNameSafe(this),
        LayoutSeatIndex,
        *GetNameSafe(RepresentedHand),
        IsValid(RepresentedHand) ? RepresentedHand->GetLayoutSeatIndex() : INDEX_NONE,
        PairsToPresent.Num());

    for (const FActivatedPair& Pair : PairsToPresent)
    {
        if (IsValid(Pair.CardA) && IsValid(Pair.CardB))
        {
            OnPairActivated(Pair.CardA, Pair.CardB);
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

    UE_LOG(LogTemp, Warning,
        TEXT("INSIDE ADD: this=%s CardOwnerBefore=%s Card=%s"),
        *GetNameSafe(this),
        *GetNameSafe(Card ? Card->GetOwner() : nullptr),
        *GetNameSafe(Card));

    Card->SetOwner(this);
    Card->SetCardZone(ECardZone::Hand);
    Cards.Insert(Card, Index);

    Card->ForceNetUpdate();
    ForceNetUpdate();

    RefreshCardsPresentation();
    UpdateCardPositions();
}

TArray<ASHCard*> ASHHand::GetCards()
{
    return Cards;
}

TArray<FActivatedPair> ASHHand::GetActivationPairs()
{
    const ASHHand* RepresentedHand = GetRepresentedHand();
    return IsValid(RepresentedHand)
        ? RepresentedHand->ActivationPairs
        : ActivationPairs;
}

TArray<ASHCard*> ASHHand::GetActivationCards()
{
    const ASHHand* RepresentedHand = GetRepresentedHand();
    const TArray<FActivatedPair>& PairsToRead = IsValid(RepresentedHand)
        ? RepresentedHand->ActivationPairs
        : ActivationPairs;

    TArray<ASHCard*> ReturnCards;
    ReturnCards.Reserve(PairsToRead.Num() * 2);

    for (const FActivatedPair& Pair : PairsToRead)
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
    ASHPlayerController* LocalPC =
        Cast<ASHPlayerController>(
            GetWorld()->GetFirstPlayerController()
        );

    if (!IsValid(LocalPC))
    {
        return;
    }

    ASHHand* VisualHand =
        LocalPC->FindVisualHandForLogicalHand(this);

    if (!IsValid(VisualHand))
    {
        return;
    }

    VisualHand->RefreshCardsPresentation();
    VisualHand->UpdateCardPositions();
}

void ASHHand::OnRep_ActivationPairs()
{
    ASHPlayerController* LocalPC = Cast<ASHPlayerController>(GetWorld()->GetFirstPlayerController());
    if (!IsValid(LocalPC))
    {
        return;
    }

    ASHHand* VisualHand = LocalPC->FindVisualHandForLogicalHand(this);

    UE_LOG(
        LogTemp,
        Warning,
        TEXT("[SH_ACTIVATION][ROUTE] LogicalHand=%s LogicalLayoutSeat=%d VisualHand=%s VisualLayoutSeat=%d PairCount=%d"),
        *GetNameSafe(this),
        LayoutSeatIndex,
        *GetNameSafe(VisualHand),
        IsValid(VisualHand) ? VisualHand->GetLayoutSeatIndex() : INDEX_NONE,
        ActivationPairs.Num());

    if (IsValid(VisualHand))
    {
        VisualHand->RefreshActivationPairsPresentation();
    }
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
    if (!IsValid(RepresentedPlayerState))
    {
        return;
    }

    ASHHand* RepresentedHand = RepresentedPlayerState->GetHand();

    if (!IsValid(RepresentedHand))
    {
        return;
    }

    for (ASHCard* Card : RepresentedHand->GetCards())
    {
        if (!IsValid(Card))
        {
            continue;
        }

        UE_LOG(LogTemp, Warning,
            TEXT("FACE: VisualHand=%s RepresentedHand=%s Card=%s -> %s"),
            *GetNameSafe(this),
            *GetNameSafe(RepresentedHand),
            *GetNameSafe(Card),
            bShowCardFronts ? TEXT("FRONT") : TEXT("BACK"));

        Card->SetFaceUp(bShowCardFronts);
    }
}

bool ASHHand::ContainsCard(ASHCard* CardB)
{
    return Cards.Contains(CardB);
}

FActivatedPair* ASHHand::FindActivationPair(ASHCard* Card)
{
    return ActivationPairs.FindByPredicate(
        [Card](const FActivatedPair& Pair)
        {
            return Pair.CardA == Card || Pair.CardB == Card;
        });
}

ASHHand* ASHHand::GetRepresentedHand() const
{
    if (!IsValid(RepresentedPlayerState))
    {
        return nullptr;
    }

    return RepresentedPlayerState->GetHand();
}

ASHPlayerState* ASHHand::GetRepresentedPlayerState() const
{
    return RepresentedPlayerState;
}
