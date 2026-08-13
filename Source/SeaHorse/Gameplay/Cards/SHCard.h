// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SHCard.generated.h"

class UCardDefinition;

UCLASS()
class SEAHORSE_API ASHCard : public AActor
{
	GENERATED_BODY()
	
public:	
	// Sets default values for this actor's properties
	ASHCard();

	void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const;

	UFUNCTION(BlueprintImplementableEvent, BlueprintCallable)
	void Initialize();

	UFUNCTION(BlueprintCallable, BlueprintPure)
	TSubclassOf<UCardDefinition> GetCardDefinition();

	void SetCardDefinition(TSubclassOf<UCardDefinition> CardDefinition);

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

	UPROPERTY(ReplicatedUsing = OnRep_CardDefinition, BlueprintReadOnly, EditAnywhere, meta = (ExposeOnSpawn = "true"))
	TSubclassOf<UCardDefinition> CardDefinition;
	
	UFUNCTION()
	void OnRep_CardDefinition();

public:	
	// Called every frame
	virtual void Tick(float DeltaTime) override;

};
