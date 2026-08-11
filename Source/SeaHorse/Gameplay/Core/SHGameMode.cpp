// Fill out your copyright notice in the Description page of Project Settings.


#include "SeaHorse/Gameplay/Core/SHGameMode.h"
#include "SeaHorse/Gameplay/SHHand.h"
#include "GameFramework/GameState.h"
#include "SeaHorse/Gameplay/Core/SHPlayerState.h"

void ASHGameMode::PostLogin(APlayerController* NewPlayer)
{
	Super::PostLogin(NewPlayer);

}

void ASHGameMode::Logout(AController* Exiting)
{
	Super::Logout(Exiting);

}

void ASHGameMode::HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer)
{
	Super::HandleStartingNewPlayer_Implementation(NewPlayer);

	checkf(HandClass, TEXT("HandClass is not configured in %s"), *GetNameSafe(this));
	checkf(IsValid(NewPlayer), TEXT("HandleStartingNewPlayer received invalid PlayerController"));

	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = NewPlayer;

	ASHHand* Hand = GetWorld()->SpawnActor<ASHHand>(
		HandClass,
		FVector::ZeroVector,
		FRotator::ZeroRotator,
		SpawnParams
	);

	checkf(IsValid(Hand), TEXT("Failed to spawn Hand for player %s"), *GetNameSafe(NewPlayer));

	ASHPlayerState* SHPlayerState = NewPlayer->GetPlayerState<ASHPlayerState>();

	checkf(IsValid(SHPlayerState), TEXT("Player %s does not have a valid ASHPlayerState"), *GetNameSafe(NewPlayer));

	SHPlayerState->SetHand(Hand);
	
}
