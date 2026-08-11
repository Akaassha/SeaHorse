// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameMode.h"
#include "SHGameMode.generated.h"

class ASHHand;
/**
 * 
 */
UCLASS()
class SEAHORSE_API ASHGameMode : public AGameMode
{
	GENERATED_BODY()
	
public:
	//Begin AGameMode Interface
	virtual void PostLogin(APlayerController* NewPlayer) override;
	virtual void Logout(AController* Exiting) override;

	virtual void HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer) override;
	//End AGameMode Interface

protected:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cards")
	TSubclassOf<ASHHand> HandClass;
};
