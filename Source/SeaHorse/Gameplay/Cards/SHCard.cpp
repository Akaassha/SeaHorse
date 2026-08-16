// Fill out your copyright notice in the Description page of Project Settings.


#include "SeaHorse/Gameplay/Cards/SHCard.h"
#include "SeaHorse/Gameplay/SHHand.h"
#include "SeaHorse/Gameplay/Cards/CardDefinition.h"
#include "Net/UnrealNetwork.h"

ASHCard::ASHCard()
{
	PrimaryActorTick.bCanEverTick = true;

	bReplicates = true;
	SetReplicateMovement(false);
}

void ASHCard::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME_CONDITION(ASHCard, CardDefinition, COND_OwnerOnly);
	DOREPLIFETIME(ASHCard, RevealedCardDefinition);
}

TSubclassOf<UCardDefinition> ASHCard::GetCardDefinition()
{
	return CardDefinition;
}

void ASHCard::SetCardDefinition(TSubclassOf<UCardDefinition> NewCardDefinition)
{
	CardDefinition = NewCardDefinition;
}

ASHHand* ASHCard::GetOwningHand()
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

	SetFaceUp(true);
}

void ASHCard::BeginPlay()
{
	Super::BeginPlay();
	
}

void ASHCard::OnRep_CardDefinition()
{
	Initialize();

	UpdateCardVisual(bFaceUp);
}

void ASHCard::OnRep_Owner()
{
	Super::OnRep_Owner();

	if (ASHHand* Hand = GetOwningHand())
	{
		Hand->RefreshCardsPresentation();
		Hand->UpdateCardPositions();
	}
}

void ASHCard::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

}

