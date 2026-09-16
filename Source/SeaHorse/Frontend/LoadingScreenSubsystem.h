// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Tickable.h"
#include "LoadingScreenSubsystem.generated.h"

/**
 * 
 */
UCLASS()
class SEAHORSE_API ULoadingScreenSubsystem : public UGameInstanceSubsystem, public FTickableGameObject
{
	GENERATED_BODY()
	
public:
	DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnLoadingReasonUpdatedDelegate, const FString&, CurrentLoadingReason);

	UPROPERTY(BlueprintAssignable)
	FOnLoadingReasonUpdatedDelegate OnLoadingReasonUpdatedDelegate;

	//~Begin USubsystem Interface
	virtual bool ShouldCreateSubsystem(UObject* Outer) const;
	virtual void Initialize(FSubsystemCollectionBase& Collection);
	virtual void Deinitialize();
	//~End USubsystem Interface

	//~Begin FTickableGameObject Interface
	virtual UWorld* GetTickableGameObjectWorld() const override;
	virtual void Tick(float DeltaTime) override;
	virtual ETickableTickType GetTickableTickType() const override;
	virtual bool IsTickable() const override;
	virtual TStatId GetStatId() const override;
	//~End FTickableGameObject Interface

private:
	void OnMapPreLoaded(const FWorldContext& WorldContext, const FString& MapName);
	void OnMapPostLoaded(UWorld*  LoadedWorld);

	void TryUpdateLoadingScreen();

	bool IsPreLoadScreenActive() const;

	bool ShouldShowLoadingScreen();

	bool CheckTheNeedToShowLoadingScree();

	void TryDisplayLoadingScreenIfNone();

	void TryRemoveLoadingScreen();

	void NotifyLoadingScreenVisibilityChanged(bool bIsVisible);

	bool bIsCurrentlyLoadingMap = false;

	double HoldLoadingScreenStartUpTime = -1.0;

	FString CurrentLoadingReason;

	UPROPERTY(Transient)
	TObjectPtr<class UUserWidget> LoadingWidget;

	TSharedPtr<SWidget> CachedCreatedLoadingWidget;
};


