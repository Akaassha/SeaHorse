// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "OptionsDataRegistry.generated.h"

class UListDataObjectCollection;
class UListDataObjectBase;
/**
 * 
 */
UCLASS()
class SEAHORSE_API UOptionsDataRegistry : public UObject
{
	GENERATED_BODY()

public:
	void InitOptionsDataRegistry(ULocalPlayer* InOwningLocalPlayer);

	const TArray<UListDataObjectCollection*>& GetRegisteredOptionsTabCollections() const { return RegisteredOptionsTabCollections; }

	TArray<UListDataObjectBase*> GetListSourceItemsBySelectedTabID(const FName& InSelectedTabID) const;
	
private:
	void FindChildListDataRecusively(UListDataObjectBase* InParentData, TArray< UListDataObjectBase*>& OutFoundListData) const;

	void InitGameplayCollectionTab();
	void InitAudioCollectionTab();
	void InitVideoCollectionTab();
	void InitControllCollectionTab(ULocalPlayer* InOwningLocalPlayer);
	void InitGamepadCollectionTab(ULocalPlayer* InOwningLocalPlayer);
	void InitAccesibilityCollectionTab();

	UPROPERTY(Transient)
	TArray<UListDataObjectCollection*> RegisteredOptionsTabCollections;
};
