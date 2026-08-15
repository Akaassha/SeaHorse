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
	
	//UPROPERTY(EditDefaultsOnly, Category = "Gameplay")
	//TSubclassOf<ASHHand> HandClass;

public:
	UFUNCTION(BlueprintCallable)
	void TrySetupTableView();

	bool IsTableViewInitialized() { return bTableViewInitialized; }
protected:
	UFUNCTION()
	void SetupTableView();

	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

	UFUNCTION(BlueprintCallable)
	void DebugHands();

	UFUNCTION(BlueprintCallable)
	void DebugCardDefinitions();

	UFUNCTION(BlueprintCallable)
	int32 GetVisualSeatIndex(int32 PlayerSeatIndex, int32 PlayerCount) const;

	UFUNCTION()
	ASHHand* FindLayoutHand(int32 LayoutSeatIndex) const;

	UFUNCTION(BlueprintCallable, Server, Reliable)
	void ServerTakeCard(ASHCard* Card, int32 InsertIndex);

protected:
	bool bTableViewInitialized = false;
};
