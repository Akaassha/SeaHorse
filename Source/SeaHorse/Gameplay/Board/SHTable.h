// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SHTable.generated.h"

UCLASS()
class SEAHORSE_API ASHTable : public APawn
{
	GENERATED_BODY()
	
public:	
	// Sets default values for this actor's properties
	ASHTable();

	UFUNCTION(BlueprintImplementableEvent, BlueprintCallable)
	USceneComponent* GetHandRoot(int32 PlayerCount, int32 VisualSeatIndex) const;

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

public:	
	// Called every frame
	virtual void Tick(float DeltaTime) override;

};
