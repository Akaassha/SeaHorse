// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Frontend/FrontendEnumTypes.h"
#include "FrontendSubsystem.generated.h"

class UWidgetPrimaryLayout;
class UWidgetActivatableBase;
class UFrontendCommonButtonBase;
struct FGameplayTag;

enum class EAsyncPushWdgetState : uint8
{
	OnCreatedBeforePush,
	AfterPush,
	Failed
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnButtonDescriptionTextUpdatedDelegate, UFrontendCommonButtonBase*, BroadcatingButton, FText, DescriptionText);

/**
 * 
 */

UCLASS()
class SEAHORSE_API UFrontendSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	static UFrontendSubsystem* Get(const UObject* WorldContextObject);

	UFUNCTION(BlueprintCallable)
	void RegisterCreatedPrimaryLayoutWidget(UWidgetPrimaryLayout* InCreatedWidget);

	void PushSoftWidgetToStackAsync(const FGameplayTag& InWidgetStackTag, TSoftClassPtr<UWidgetActivatableBase> InSoftWidgetClass, TFunction<void(EAsyncPushWdgetState, UWidgetActivatableBase*)> AsyncPushStateCallback);
	void PushConfirmScreenToModalStackAsync(EConfirmScreenType InScreenType, const FText& InScreenTitle, const FText& InScreenMsg, TFunction<void(EConfirmScreenButtonType)> ButtonClickedCallback);
	
	UPROPERTY(BlueprintAssignable)
	FOnButtonDescriptionTextUpdatedDelegate OnButtonDescriptionTextUpdatedDelegate;

private:
	UPROPERTY(Transient)
	UWidgetPrimaryLayout* CreatedPrimaryLayout;
};
