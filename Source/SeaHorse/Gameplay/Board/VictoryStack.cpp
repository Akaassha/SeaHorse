// Fill out your copyright notice in the Description page of Project Settings.


#include "SeaHorse/Gameplay/Board/VictoryStack.h"
#include "SeaHorse/Gameplay/Cards/SHCard.h"
#include "SeaHorse/Gameplay/Core/SHGameState.h"
#include "SeaHorse/Gameplay/Core/SHPlayerController.h"
#include "SeaHorse/Gameplay/Core/SHPlayerState.h"
#include "SeaHorse/Gameplay/SHHand.h"
#include "Net/UnrealNetwork.h"
#include "Components/SceneComponent.h"

void AVictoryStack::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AVictoryStack, ReplicatedCards);
}

// Sets default values
AVictoryStack::AVictoryStack()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;

    bReplicates = true;
    SetReplicateMovement(false);

}

void AVictoryStack::AddPair(ASHCard* CardA, ASHCard* CardB)
{
    checkf(HasAuthority(), TEXT("AddPair can only be called on the server"));
    checkf(IsValid(CardA) && IsValid(CardB), TEXT("Invalid cards passed to VictoryStack"));

    ReplicatedCards.Add(CardA);
    ReplicatedCards.Add(CardB);

    CardA->SetOwner(this);
    CardB->SetOwner(this);

    CardA->SetCardZone(ECardZone::Victory);
    CardB->SetCardZone(ECardZone::Victory);

    ForceNetUpdate();
    RefreshCardsPresentation();
}

// Called when the game starts or when spawned
void AVictoryStack::BeginPlay()
{
	Super::BeginPlay();
	
    LayoutTransform = GetActorTransform();
	ResolveCardPlaceholder();
}

// Called every frame
void AVictoryStack::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	UpdateCardsLayout(DeltaTime);
}

void AVictoryStack::OnRep_ReplicatedCards()
{
	RefreshCardsPresentation();
}

void AVictoryStack::RefreshCardsPresentation()
{
    ASHPlayerController* LocalPC = Cast<ASHPlayerController>(GetWorld()->GetFirstPlayerController());
    if (!IsValid(LocalPC) || !LocalPC->IsLocalController())
    {
        return;
    }

    ASHHand* LogicalHand = FindOwningLogicalHand();
    if (!IsValid(LogicalHand))
    {
        return;
    }

    ASHHand* VisualHand = LocalPC->FindVisualHandForLogicalHand(LogicalHand);
    if (!IsValid(VisualHand))
    {
        return;
    }

    AVictoryStack* VisualStack = VisualHand->GetVictoryStack();
    if (!IsValid(VisualStack))
    {
        return;
    }

    VisualStack->SetPresentedCards(ReplicatedCards);
}

ASHHand* AVictoryStack::FindOwningLogicalHand() const
{
    const ASHGameState* GameState = GetWorld()->GetGameState<ASHGameState>();
    if (!IsValid(GameState))
    {
        return nullptr;
    }

    for (APlayerState* PlayerState : GameState->PlayerArray)
    {
        ASHPlayerState* SHPlayerState = Cast<ASHPlayerState>(PlayerState);
        ASHHand* Hand = IsValid(SHPlayerState) ? SHPlayerState->GetHand() : nullptr;

        if (IsValid(Hand) && Hand->GetVictoryStack() == this)
        {
            return Hand;
        }
    }

    return nullptr;
}

void AVictoryStack::SetPresentedCards(const TArray<TObjectPtr<ASHCard>>& NewCards)
{
    Cards = NewCards;
    RefreshCardsLayout();
}

void AVictoryStack::RefreshCardsLayout()
{
	ResolveCardPlaceholder();
}

USceneComponent* AVictoryStack::ResolveCardPlaceholder()
{
	if (IsValid(CardPlaceholder))
	{
		return CardPlaceholder;
	}

	TInlineComponentArray<USceneComponent*> SceneComponents(this);
	for (USceneComponent* Component : SceneComponents)
	{
		if (IsValid(Component) && Component->GetFName() == TEXT("SM_CardPlaceHolder"))
		{
			CardPlaceholder = Component;
			break;
		}
	}
	if (!IsValid(CardPlaceholder))
	{
		CardPlaceholder = GetRootComponent();
	}
	return CardPlaceholder;
}

void AVictoryStack::UpdateCardsLayout(float DeltaTime)
{
	USceneComponent* Anchor = ResolveCardPlaceholder();
	if (!IsValid(Anchor))
	{
		return;
	}

	const FTransform AnchorTransform = Anchor->GetComponentTransform();
	for (int32 CardIndex = 0; CardIndex < Cards.Num(); ++CardIndex)
	{
		ASHCard* Card = Cards[CardIndex];
		if (!IsValid(Card))
		{
			continue;
		}
		const FVector TargetLocation = AnchorTransform.TransformPosition(
			StackCardOffset * static_cast<double>(CardIndex));
		Card->SetActorLocation(FMath::VInterpTo(
			Card->GetActorLocation(), TargetLocation, DeltaTime, LayoutInterpolationSpeed));
	}
}
