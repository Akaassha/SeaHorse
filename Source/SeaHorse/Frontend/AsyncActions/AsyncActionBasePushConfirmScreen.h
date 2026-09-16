// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintAsyncActionBase.h"
#include "Frontend/FrontendEnumTypes.h"
#include "AsyncActionBasePushConfirmScreen.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FConfirmScreenButtonClickedDelegate, EConfirmScreenButtonType, ClickedButtonType);

/**
 * 
 */
UCLASS()
class SEAHORSE_API UAsyncActionBasePushConfirmScreen : public UBlueprintAsyncActionBase
{
	GENERATED_BODY()
	
public:
	UFUNCTION(BlueprintCallable, meta = (WorldContext = "WorldContextObject", HidePin = "WorldContextObject", BlueprintInternalUseOnly = "true", DisplayName = "Show Confirmation Screen"))
	static UAsyncActionBasePushConfirmScreen* PushConfirmScreen(const UObject* WorldContextObject, EConfirmScreenType ScreenType, FText InScreenTitle, FText InScreenMessage);

	//~Begin UBlueprintAsyncActionBase Interface
	virtual void Activate() override;
	//~End UBlueprintAsyncActionBase Interface

	UPROPERTY(BlueprintAssignable)
	FConfirmScreenButtonClickedDelegate OnButtonClicked;

private:
	bool bActivated = false;
	bool bCompleted = false;
	TWeakObjectPtr<UWorld> CachedOwningWorld;
	EConfirmScreenType CachedScreenType;
	FText CachedScreenTitle;
	FText CachedScreenMessege;
};
