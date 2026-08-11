// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "SHPlayerController.generated.h"

class ASHHand;
/**
 * 
 */
UCLASS()
class SEAHORSE_API ASHPlayerController : public APlayerController
{
	GENERATED_BODY()
	
	UPROPERTY(EditDefaultsOnly, Category = "Gameplay")
	TSubclassOf<ASHHand> HandClass;

protected:
	UFUNCTION(BlueprintCallable)
	void DebugHands();

	UFUNCTION(BlueprintCallable)
	void DebugCardDefinitions();
};
