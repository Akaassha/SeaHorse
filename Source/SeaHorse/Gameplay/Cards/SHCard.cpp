// Fill out your copyright notice in the Description page of Project Settings.


#include "SeaHorse/Gameplay/Cards/SHCard.h"
#include "Net/UnrealNetwork.h"

// Sets default values
ASHCard::ASHCard()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

	bReplicates = true;
	SetReplicateMovement(false);
}

void ASHCard::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME_CONDITION(
		ASHCard,
		CardDefinition,
		COND_OwnerOnly
	);
}

TSubclassOf<UCardDefinition> ASHCard::GetCardDefinition()
{
	return CardDefinition;
}

void ASHCard::SetCardDefinition(TSubclassOf<UCardDefinition> NewCardDefinition)
{
	CardDefinition = NewCardDefinition;
}

// Called when the game starts or when spawned
void ASHCard::BeginPlay()
{
	Super::BeginPlay();
	
}

void ASHCard::OnRep_CardDefinition()
{
	Initialize();
}

// Called every frame
void ASHCard::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

}

