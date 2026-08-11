// Fill out your copyright notice in the Description page of Project Settings.


#include "SeaHorse/Gameplay/Core/SHPlayerState.h"
#include "Net/UnrealNetwork.h"

void ASHPlayerState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ASHPlayerState, Hand);
}

void ASHPlayerState::SetHand(ASHHand* NewHand)
{
	Hand = NewHand;
}

ASHHand* ASHPlayerState::GetHand()
{
	return Hand;
}
