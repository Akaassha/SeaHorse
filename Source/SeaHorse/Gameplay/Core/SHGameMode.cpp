// Fill out your copyright notice in the Description page of Project Settings.


#include "SeaHorse/Gameplay/Core/SHGameMode.h"
#include "GameFramework/GameState.h"

void ASHGameMode::PostLogin(APlayerController* NewPlayer)
{
	Super::PostLogin(NewPlayer);

}

void ASHGameMode::Logout(AController* Exiting)
{
	Super::Logout(Exiting);

}
