// Fill out your copyright notice in the Description page of Project Settings.


#include "SeaHorse/Gameplay/SHHand.h"
#include "Net/UnrealNetwork.h"
#include "SeaHorse/Gameplay/Cards/SHCard.h"
#include "SeaHorse/Gameplay/Cards/CardDefinition.h"
#include "SeaHorse/Gameplay/Cards/Fragments/CardEndGameRulesFragment.h"
#include "SeaHorse/Gameplay/Core/SHPlayerController.h"
#include "SeaHorse/Gameplay/Core/SHGameState.h"
#include "SeaHorse/Gameplay/Core/SHGameMode.h"
#include "SeaHorse/Gameplay/Components/TurnComponent.h"
#include "Kismet/GameplayStatics.h"
#include "SeaHorse/Gameplay/Core/SHPlayerState.h"
#include "SeaHorse/Gameplay/Player/SHPlayerRepresentation.h"
#include "Engine/EngineBaseTypes.h"
#include "Algo/RandomShuffle.h"

// Sets default values
ASHHand::ASHHand()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = false;

	bReplicates = true;
    SetReplicateMovement(false);


}

void ASHHand::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ASHHand, Cards);
    DOREPLIFETIME(ASHHand, ActivationPairs);
	DOREPLIFETIME(ASHHand, LayoutSeatIndex);
	DOREPLIFETIME(ASHHand, bIsNPC);
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
    ForceNetUpdate();
    OnRep_ActivationPairs();

    return true;
}

// Called when the game starts or when spawned
void ASHHand::BeginPlay()
{
	Super::BeginPlay();
	
    LayoutTransform = GetActorTransform();
	RefreshPlayerPicker();
}

void ASHHand::SetRepresentedPlayerState(ASHPlayerState* InPlayerState)
{
	if (RepresentedPlayerState != InPlayerState || IsValid(RepresentedLogicalHand))
	{
		PresentedActivationPairs.Reset();
		SettledActivationPairs.Reset();
		PresentedEffectActivations.Reset();
	}
	RepresentedPlayerState = InPlayerState;
	RepresentedLogicalHand = nullptr;
	RefreshPlayerPicker();
}

void ASHHand::SetRepresentedHand(ASHHand* InHand)
{
	if (RepresentedLogicalHand != InHand || IsValid(RepresentedPlayerState))
	{
		PresentedActivationPairs.Reset();
		SettledActivationPairs.Reset();
		PresentedEffectActivations.Reset();
	}
	RepresentedPlayerState = nullptr;
	RepresentedLogicalHand = InHand;
	RefreshPlayerPicker();
}

void ASHHand::RefreshPlayerPicker()
{
	if (IsValid(PlayerPicker))
	{
		PlayerPicker->BindToHand(this);
	}
}

void ASHHand::SetShowCardFronts(bool bShow)
{
    bShowCardFronts = bShow;
    RefreshCardsPresentation();
}

void ASHHand::MulticastPairEffectActivated_Implementation(ASHCard* CardA, ASHCard* CardB)
{
	if (!IsValid(CardA) || !IsValid(CardB))
	{
		return;
	}

	ASHHand* PresentationHand = this;
	ASHPlayerController* LocalController = GetWorld()
		? Cast<ASHPlayerController>(GetWorld()->GetFirstPlayerController())
		: nullptr;
	if (IsValid(LocalController) && LocalController->IsLocalController())
	{
		if (ASHHand* VisualHand = LocalController->FindVisualHandForLogicalHand(this))
		{
			PresentationHand = VisualHand;
		}
	}

	PresentationHand->PresentStoredPairActivated(CardA, CardB);
	if (HasAuthority())
	{
		SendPairPresentationToPlayerControllers(CardA, CardB, true);
	}
}

void ASHHand::MulticastPairCreated_Implementation(ASHCard* CardA, ASHCard* CardB)
{
	if (!IsValid(CardA) || !IsValid(CardB))
	{
		return;
	}

	ASHHand* PresentationHand = this;
	ASHPlayerController* LocalController = GetWorld()
		? Cast<ASHPlayerController>(GetWorld()->GetFirstPlayerController())
		: nullptr;
	if (IsValid(LocalController) && LocalController->IsLocalController())
	{
		if (ASHHand* VisualHand = LocalController->FindVisualHandForLogicalHand(this))
		{
			PresentationHand = VisualHand;
		}
	}

	PresentationHand->PresentPairCreated(CardA, CardB);
	if (HasAuthority())
	{
		SendPairPresentationToPlayerControllers(CardA, CardB, false);
	}
}

void ASHHand::MulticastPairClicked_Implementation(ASHCard* CardA, ASHCard* CardB)
{
	if (!IsValid(CardA) || !IsValid(CardB))
	{
		return;
	}

	ASHHand* PresentationHand = this;
	ASHPlayerController* LocalController = GetWorld()
		? Cast<ASHPlayerController>(GetWorld()->GetFirstPlayerController()) : nullptr;
	if (IsValid(LocalController) && LocalController->IsLocalController())
	{
		if (ASHHand* VisualHand = LocalController->FindVisualHandForLogicalHand(this))
		{
			PresentationHand = VisualHand;
		}
	}
	PresentationHand->OnPairClicked(CardA, CardB);
}

void ASHHand::MulticastPairReadyForVictory_Implementation(ASHCard* CardA, ASHCard* CardB)
{
	if (!IsValid(CardA) || !IsValid(CardB))
	{
		return;
	}

	ASHHand* PresentationHand = this;
	ASHPlayerController* LocalController = GetWorld()
		? Cast<ASHPlayerController>(GetWorld()->GetFirstPlayerController()) : nullptr;
	if (IsValid(LocalController) && LocalController->IsLocalController())
	{
		if (ASHHand* VisualHand = LocalController->FindVisualHandForLogicalHand(this))
		{
			PresentationHand = VisualHand;
		}
	}
	PresentationHand->OnPairReadyForVictory(CardA, CardB);
}

void ASHHand::PresentPairCreated(ASHCard* CardA, ASHCard* CardB)
{
	const FActivatedPair Pair{CardA, CardB, false};
	if (!IsValid(CardA) || !IsValid(CardB) || PresentedActivationPairs.Contains(Pair))
	{
		return;
	}

	PresentedActivationPairs.Add(Pair);
	OnPairCreated(CardA, CardB);
	// Legacy hook retained for existing BP_Hand implementations.
	OnPairActivated(CardA, CardB);
}

void ASHHand::NotifyPairSettled(ASHCard* CardA, ASHCard* CardB)
{
	const FActivatedPair Pair{CardA, CardB, false};
	if (!IsValid(CardA) || !IsValid(CardB) || SettledActivationPairs.Contains(Pair))
	{
		return;
	}

	SettledActivationPairs.Add(Pair);
	OnPairSettled(CardA, CardB);
	if (HasAuthority())
	{
		ASHGameMode* GameMode = GetWorld()->GetAuthGameMode<ASHGameMode>();
		if (IsValid(GameMode) && IsValid(GameMode->GetTurnComponent()))
		{
			GameMode->GetTurnComponent()->NotifyPairSettled(CardA, CardB);
		}
	}
}

void ASHHand::BeginTurnBlockingEffect(FName EffectId)
{
	if (EffectId.IsNone())
	{
		return;
	}

	++LocalPresentationBlocks.FindOrAdd(EffectId);
	if (HasAuthority())
	{
		if (ASHGameMode* GameMode = GetWorld()->GetAuthGameMode<ASHGameMode>())
		{
			GameMode->GetTurnComponent()->BeginTurnTransitionBlock(EffectId);
		}
	}
}

void ASHHand::FinishTurnBlockingEffect(FName EffectId)
{
	if (int32* Count = LocalPresentationBlocks.Find(EffectId))
	{
		if (--(*Count) <= 0)
		{
			LocalPresentationBlocks.Remove(EffectId);
		}
	}

	if (HasAuthority())
	{
		if (ASHGameMode* GameMode = GetWorld()->GetAuthGameMode<ASHGameMode>())
		{
			GameMode->GetTurnComponent()->FinishTurnTransitionBlock(EffectId);
		}
	}
}

void ASHHand::PresentStoredPairActivated(ASHCard* CardA, ASHCard* CardB)
{
	const FActivatedPair Pair{CardA, CardB, true};
	if (!IsValid(CardA) || !IsValid(CardB) || PresentedEffectActivations.Contains(Pair))
	{
		return;
	}

	PresentedEffectActivations.Add(Pair);
	OnStoredPairActivated(CardA, CardB);
	// Legacy hook retained for existing BP_Hand implementations.
	OnPairEffectActivated(CardA, CardB);
}

void ASHHand::SendPairPresentationToPlayerControllers(
	ASHCard* CardA, ASHCard* CardB, bool bEffectActivation) const
{
	const ASHGameState* GameState = GetWorld() ? GetWorld()->GetGameState<ASHGameState>() : nullptr;
	if (!IsValid(GameState))
	{
		return;
	}

	for (APlayerState* State : GameState->PlayerArray)
	{
		const ASHPlayerState* PlayerState = Cast<ASHPlayerState>(State);
		ASHPlayerController* Controller = IsValid(PlayerState)
			? Cast<ASHPlayerController>(PlayerState->GetOwner())
			: nullptr;
		if (IsValid(Controller))
		{
			Controller->ClientNotifyPairPresentation(
				const_cast<ASHHand*>(this), CardA, CardB, bEffectActivation);
		}
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
	if (LogicalHand->IsLogicalNPC())
	{
		return;
	}
    LogicalHand->AddActivationPairToLogicalHand(CardA, CardB);
}

void ASHHand::AddActivationPairToLogicalHand(ASHCard* CardA, ASHCard* CardB)
{
    checkf(HasAuthority(), TEXT("Activation pairs can only be added on the server"));

	if (bIsNPC)
	{
		return;
	}
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
	MulticastPairCreated(CardA, CardB);
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
        if (IsValid(Pair.CardA) && IsValid(Pair.CardB) &&
			!PresentedActivationPairs.Contains(Pair))
        {
			PresentPairCreated(Pair.CardA, Pair.CardB);
        }
    }

	PresentedActivationPairs.Reset(PairsToPresent.Num());
	for (const FActivatedPair& Pair : PairsToPresent)
	{
		if (IsValid(Pair.CardA) && IsValid(Pair.CardB))
		{
			PresentedActivationPairs.Add(Pair);
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
	if (bIsNPC)
	{
		Cards.Add(Card);
	}
	else
	{
		Cards.Insert(Card, Index);
	}

    Card->ForceNetUpdate();
    ForceNetUpdate();

    RefreshLocalCardsPresentation();
	OnHandCardsChanged.Broadcast(GetCardCount());
	if (bIsNPC && ShouldShuffleForCard(Card))
	{
		ShuffleStack();
	}
}

void ASHHand::SetIsNPC(bool bNewIsNPC)
{
	checkf(HasAuthority(), TEXT("Hand control mode can only be changed on the server"));
	bIsNPC = bNewIsNPC;
	OnRep_IsNPC();
	ForceNetUpdate();
}

bool ASHHand::IsNPC() const
{
	// Blueprint calls this on a physical BP_Hand used as a local visual slot.
	// Report the participant displayed in that slot, not the slot's own logical
	// role on the authoritative table.
	if (IsValid(RepresentedLogicalHand))
	{
		return RepresentedLogicalHand->bIsNPC;
	}
	if (IsValid(RepresentedPlayerState))
	{
		return false;
	}
	return bIsNPC;
}

ASHCard* ASHHand::GetTopCard() const
{
	return bIsNPC && !Cards.IsEmpty() ? Cards.Last() : nullptr;
}

bool ASHHand::HasSeaHorseCard() const
{
	for (const ASHCard* Card : Cards)
	{
		if (!IsValid(Card))
		{
			continue;
		}
		const UCardEndGameRulesFragment* Rules = Cast<UCardEndGameRulesFragment>(
			UCardDefinition::FindFragmentByClass(Card->CardDefinition, UCardEndGameRulesFragment::StaticClass()));
		if (IsValid(Rules) && Rules->bOwnerAutomaticallyLoses)
		{
			return true;
		}
	}
	return false;
}

bool ASHHand::ReorderCard(ASHCard* Card, int32 InsertIndex)
{
	checkf(HasAuthority(), TEXT("Cards can only be reordered on the server"));
	if (bIsNPC || !IsValid(Card) || Card->GetOwningHand() != this || !HasSeaHorseCard())
	{
		return false;
	}

	const int32 CurrentIndex = Cards.IndexOfByKey(Card);
	if (CurrentIndex == INDEX_NONE)
	{
		return false;
	}
	Cards.RemoveAt(CurrentIndex);
	if (InsertIndex < 0 || InsertIndex > Cards.Num())
	{
		Cards.Insert(Card, CurrentIndex);
		return false;
	}
	Cards.Insert(Card, InsertIndex);
	ForceNetUpdate();
	RefreshLocalCardsPresentation();
	OnHandCardsChanged.Broadcast(GetCardCount());
	return true;
}

ASHCard* ASHHand::TakeTopCard()
{
	checkf(HasAuthority(), TEXT("Cards can only be taken on the server"));
	ASHCard* Card = GetTopCard();
	if (IsValid(Card))
	{
		RemoveCard(Card);
	}
	return Card;
}

void ASHHand::ShuffleStack()
{
	checkf(HasAuthority(), TEXT("Stacks can only be shuffled on the server"));
	if (!bIsNPC) return;
	Algo::RandomShuffle(Cards);
	ForceNetUpdate();
	OnRep_Cards();
	OnNPCStackShuffled.Broadcast();
}

void ASHHand::RevealStack()
{
	checkf(HasAuthority(), TEXT("Stacks can only be revealed on the server"));
	if (!bIsNPC) return;
	for (ASHCard* Card : Cards) if (IsValid(Card)) Card->Reveal();
}

bool ASHHand::ShouldShuffleForCard(const ASHCard* Card) const
{
	if (!bIsNPC || !IsValid(Card)) return false;
	const UCardEndGameRulesFragment* Rules = Cast<UCardEndGameRulesFragment>(
		UCardDefinition::FindFragmentByClass(Card->CardDefinition, UCardEndGameRulesFragment::StaticClass()));
	return IsValid(Rules) && Rules->bOwnerAutomaticallyLoses;
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
    if (RemovedCount == 0)
    {
        return;
    }

    Card->SetFaceUp(false);

    UE_LOG(LogTemp, Warning,
        TEXT("AddCard: Hand=%s ShowFronts=%s Card=%s"),
        *GetName(),
        bShowCardFronts ? TEXT("TRUE") : TEXT("FALSE"),
        *GetNameSafe(Card));

	ForceNetUpdate();
    RefreshLocalCardsPresentation();
	OnHandCardsChanged.Broadcast(GetCardCount());
}

void ASHHand::OnRep_Cards()
{
    RefreshLocalCardsPresentation();
	OnHandCardsChanged.Broadcast(GetCardCount());
	if (ASHPlayerController* PC = Cast<ASHPlayerController>(GetWorld()->GetFirstPlayerController());
		IsValid(PC) && PC->IsLocalController())
	{
		PC->TrySetupTableView();
	}
}

void ASHHand::OnRep_IsNPC()
{
	RefreshLocalCardsPresentation();
	if (ASHPlayerController* PC = Cast<ASHPlayerController>(GetWorld()->GetFirstPlayerController());
		IsValid(PC) && PC->IsLocalController())
	{
		PC->TrySetupTableView();
	}
}

void ASHHand::RefreshLocalCardsPresentation()
{
    ASHPlayerController* LocalPC =
        Cast<ASHPlayerController>(
            GetWorld()->GetFirstPlayerController()
        );

    if (!IsValid(LocalPC) || !LocalPC->IsLocalController())
    {
        return;
    }

    ASHHand* VisualHand =
        LocalPC->FindVisualHandForLogicalHand(this);

    if (!IsValid(VisualHand))
    {
        return;
    }

	if (bIsNPC)
	{
		VisualHand->LayoutNPCStack(this);
	}
	else
	{
		VisualHand->RefreshCardsPresentation();
		VisualHand->UpdateCardPositions();
	}
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

void ASHHand::SetLayoutSeatIndex(int32 NewLayoutSeatIndex)
{
    checkf(HasAuthority(), TEXT("Layout seats can only be assigned on the server"));
    LayoutSeatIndex = NewLayoutSeatIndex;
	ForceNetUpdate();
}

void ASHHand::OnRep_LayoutSeatIndex()
{
	ASHPlayerController* LocalPC = Cast<ASHPlayerController>(GetWorld()->GetFirstPlayerController());
	if (IsValid(LocalPC) && LocalPC->IsLocalController())
	{
		LocalPC->TrySetupTableView();
	}
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

        // A card can previously have belonged to an NPC stack, where only the
        // logical top card is hit-testable. Restore normal interaction when it
        // is presented in a human player's hand.
        Card->SetActorEnableCollision(true);
        Card->SetFaceUp(bShowCardFronts);
    }
}

void ASHHand::LayoutNPCStack(ASHHand* LogicalNPCStack)
{
    if (!IsValid(LogicalNPCStack))
    {
        return;
    }

    const TArray<ASHCard*> StackCards = LogicalNPCStack->GetCards();
    const int32 TopCardIndex = StackCards.Num() - 1;
    for (int32 CardIndex = 0; CardIndex < StackCards.Num(); ++CardIndex)
    {
        ASHCard* Card = StackCards[CardIndex];
        if (!IsValid(Card))
        {
            continue;
        }

        // Overlapping cards can otherwise win the cursor trace in a different
        // order on each client. Gameplay defines Cards.Last() as the top, so
        // make that the only card in an NPC stack that can be hit.
        Card->SetActorEnableCollision(CardIndex == TopCardIndex);
        Card->SetFaceUp(false);
    }

	// The hand layout component is the sole transform writer. Previously this
	// method snapped cards to the actor while the component interpolated them to
	// its spline, producing visible oscillation during reconciliation/shuffles.
	UpdateCardPositions();
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

void ASHHand::SetActivationPairState(ASHCard* CardA, ASHCard* CardB, EActivationPairState NewState)
{
	checkf(HasAuthority(), TEXT("Activation pair state can only be changed on the server"));
	FActivatedPair* Pair = ActivationPairs.FindByPredicate(
		[CardA, CardB](const FActivatedPair& Candidate)
		{
			return (Candidate.CardA == CardA && Candidate.CardB == CardB) ||
				(Candidate.CardA == CardB && Candidate.CardB == CardA);
		});
	if (Pair)
	{
		Pair->State = NewState;
		Pair->bActivated = NewState >= EActivationPairState::AbilityEffect;
		ForceNetUpdate();
	}
}

ASHHand* ASHHand::GetRepresentedHand() const
{
	if (IsValid(RepresentedLogicalHand))
	{
		return RepresentedLogicalHand;
	}

	if (!IsValid(RepresentedPlayerState))
    {
        // Before the client-relative table view is ready, every physical hand
        // is also its own logical hand. This keeps Blueprint interaction and
        // replication callbacks valid during initialization.
        return const_cast<ASHHand*>(this);
    }

    ASHHand* PlayerHand = RepresentedPlayerState->GetHand();
	return IsValid(PlayerHand) ? PlayerHand : const_cast<ASHHand*>(this);
}

ASHPlayerState* ASHHand::GetRepresentedPlayerState() const
{
    return RepresentedPlayerState;
}
