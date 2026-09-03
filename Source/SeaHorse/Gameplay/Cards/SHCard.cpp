// Fill out your copyright notice in the Description page of Project Settings.


#include "SeaHorse/Gameplay/Cards/SHCard.h"
#include "SeaHorse/Gameplay/SHHand.h"
#include "SeaHorse/Gameplay/Cards/CardDefinition.h"
#include "SeaHorse/Gameplay/Core/SHPlayerController.h"
#include "Blueprint/UserWidget.h"
#include "Net/UnrealNetwork.h"

ASHCard::ASHCard()
{
	PrimaryActorTick.bCanEverTick = false;

	bReplicates = true;
	SetReplicateMovement(false);
}

void ASHCard::Initialize_Implementation()
{
	if (!WidgetRenderer)
	{
		WidgetRenderer = MakeUnique<FWidgetRenderer>(true, true);
	}

    if (!IsValid(CardFaceWidget))
    {
        checkf(CardFaceWidgetClass, TEXT("CardFaceWidgetClass is not set"));
        CardFaceWidget = CreateWidget<UUserWidget>(GetWorld(), CardFaceWidgetClass);
    }

	checkf(IsValid(CardFaceWidget), TEXT("Failed to create CardFaceWidget"));

	RefreshCardFace();
}

void ASHCard::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME_CONDITION(ASHCard, CardDefinition, COND_OwnerOnly);
	DOREPLIFETIME(ASHCard, RevealedCardDefinition);
	DOREPLIFETIME(ASHCard, CardZone);
}

TSubclassOf<UCardDefinition> ASHCard::GetCardDefinition()
{
	return CardDefinition;
}

void ASHCard::SetCardDefinition(TSubclassOf<UCardDefinition> NewCardDefinition)
{
	CardDefinition = NewCardDefinition;
}

ASHHand* ASHCard::GetOwningHand() const
{
	return Cast<ASHHand>(GetOwner());
}

void ASHCard::SetFaceUp(bool bNewFaceUp)
{
	bFaceUp = bNewFaceUp;

	const bool bShouldActuallyBeFaceUp = IsValid(RevealedCardDefinition) || bFaceUp;

	UE_LOG(LogTemp, Warning,
		TEXT("[SET FACE] World=%s Card=%s Requested=%d Revealed=%s Final=%d"),
		*GetWorld()->GetName(),
		*GetNameSafe(this),
		bNewFaceUp,
		*GetNameSafe(RevealedCardDefinition.Get()),
		bShouldActuallyBeFaceUp);

	UpdateCardVisual(bShouldActuallyBeFaceUp);
}

void ASHCard::Reveal()
{
	checkf(HasAuthority(), TEXT("Reveal can only be called on server"));

	RevealedCardDefinition = CardDefinition;

	UE_LOG(LogTemp, Warning,
		TEXT("[REVEAL SERVER] World=%s Card=%s Revealed=%s"),
		*GetWorld()->GetName(),
		*GetNameSafe(this),
		*GetNameSafe(RevealedCardDefinition.Get()));


	OnRep_RevealedCardDefinition();
}

void ASHCard::OnRep_RevealedCardDefinition()
{
	UE_LOG(LogTemp, Warning,
		TEXT("[REVEAL ONREP] World=%s Card=%s Revealed=%s bFaceUp=%d"),
		*GetWorld()->GetName(),
		*GetNameSafe(this),
		*GetNameSafe(RevealedCardDefinition.Get()),
		bFaceUp);

	RefreshCardFace();

	SetFaceUp(true);
}

void ASHCard::ApplyOwnerCardDefinition(TSubclassOf<UCardDefinition> InCardDefinition)
{
	CardDefinition = InCardDefinition;

	RefreshCardFace();
	SetFaceUp(bFaceUp);
}

ECardZone ASHCard::GetCardZone() const
{
	return CardZone;
}

void ASHCard::RefreshCardFace()
{
	checkf(IsValid(CardFaceWidget), TEXT("CardFaceWidget is invalid"));

	if (!WidgetRenderer)
	{
		WidgetRenderer = MakeUnique<FWidgetRenderer>(true, true);
	}

	const FVector2D DrawSize(512.0f, 768.0f);

	CardFaceRenderTarget = WidgetRenderer->DrawWidget(
		CardFaceWidget->TakeWidget(),
		DrawSize
	);

	//checkf(
	//	IsValid(CardFaceRenderTarget),
	//	TEXT("Failed to render CardFaceWidget")
	//);

	OnCardFaceRendered(CardFaceRenderTarget);
}

void ASHCard::SetCardZone(ECardZone NewZone)
{
	checkf(HasAuthority(), TEXT("CardZone can only be changed on server"));

	CardZone = NewZone;
	OnCardZoneChanged();
}

void ASHCard::OnRep_CardZone()
{
	OnCardZoneChanged();
}

void ASHCard::OnRep_CardDefinition()
{
	Initialize();
	UpdateCardVisual(bFaceUp);
}

void ASHCard::OnRep_Owner()
{
    Super::OnRep_Owner();

    ASHHand* LogicalHand = GetOwningHand();
    ASHPlayerController* LocalPC = Cast<ASHPlayerController>(GetWorld()->GetFirstPlayerController());

    if (IsValid(LogicalHand) && IsValid(LocalPC))
    {
        ASHHand* VisualHand = LocalPC->FindVisualHandForLogicalHand(LogicalHand);
        if (IsValid(VisualHand))
        {
            if (LogicalHand->IsLogicalNPC())
            {
                // Owner and hand-array replication can arrive in either order.
                // Never let the regular Blueprint fan layout overwrite an NPC
                // stack when the card's replicated owner arrives last.
                VisualHand->LayoutNPCStack(LogicalHand);
            }
            else
            {
                SetActorEnableCollision(true);
                VisualHand->RefreshCardsPresentation();
                VisualHand->UpdateCardPositions();
            }
        }
    }
}

void ASHCard::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

}

void ASHCard::NotifyActorOnClicked(FKey ButtonPressed)
{
    ASHPlayerController* LocalPC = Cast<ASHPlayerController>(GetWorld()->GetFirstPlayerController());
    if (IsValid(LocalPC) && LocalPC->IsLocalController() &&
        LocalPC->TrySubmitParticipantSelectionForCard(this))
    {
        return;
    }

    Super::NotifyActorOnClicked(ButtonPressed);
}

void ASHCard::OnCardZoneChanged()
{
}

