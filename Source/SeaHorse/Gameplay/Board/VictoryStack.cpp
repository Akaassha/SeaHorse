// Fill out your copyright notice in the Description page of Project Settings.


#include "SeaHorse/Gameplay/Board/VictoryStack.h"
#include "SeaHorse/Gameplay/Cards/SHCard.h"
#include "Net/UnrealNetwork.h"

void AVictoryStack::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AVictoryStack, Cards);
}

// Sets default values
AVictoryStack::AVictoryStack()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

    bReplicates = true;
    SetReplicateMovement(false);

}

void AVictoryStack::AddPair(ASHCard* CardA, ASHCard* CardB)
{
    checkf(HasAuthority(), TEXT("AddPair can only be called on the server"));
    checkf(IsValid(CardA) && IsValid(CardB), TEXT("Invalid cards passed to VictoryStack"));

    Cards.Add(CardA);
    Cards.Add(CardB);

    CardA->SetOwner(this);
    CardB->SetOwner(this);

    CardA->SetCardZone(ECardZone::Victory);
    CardB->SetCardZone(ECardZone::Victory);

    RefreshCardsLayout();
}

// Called when the game starts or when spawned
void AVictoryStack::BeginPlay()
{
	Super::BeginPlay();
	
    LayoutTransform = GetActorTransform();
}

// Called every frame
void AVictoryStack::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

}

void AVictoryStack::OnRep_Cards()
{
	RefreshCardsLayout();
}