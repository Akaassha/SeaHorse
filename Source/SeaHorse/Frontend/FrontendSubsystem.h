// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Frontend/FrontendEnumTypes.h"
#include "FrontendSubsystem.generated.h"

class UWidgetPrimaryLayout;
class UWidgetActivatableBase;
class UFrontendCommonButtonBase;
class APlayerController;
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

	bool IsWidgetStackReady(const UWorld* ExpectedWorld, const FGameplayTag& InWidgetStackTag) const;
	bool PrepareWidgetStackForPlayer(const UWorld* ExpectedWorld, APlayerController* OwningPlayer,
		const FGameplayTag& InWidgetStackTag);
	bool DoesWidgetStackContainClass(const UWorld* ExpectedWorld, const FGameplayTag& InWidgetStackTag,
		const UClass* WidgetClass) const;

	void PushSoftWidgetToStackAsync(const FGameplayTag& InWidgetStackTag, TSoftClassPtr<UWidgetActivatableBase> InSoftWidgetClass, TFunction<void(EAsyncPushWdgetState, UWidgetActivatableBase*)> AsyncPushStateCallback);
	void PushConfirmScreenToModalStackAsync(EConfirmScreenType InScreenType, const FText& InScreenTitle, const FText& InScreenMsg, TFunction<void(EConfirmScreenButtonType)> ButtonClickedCallback);
	
	UPROPERTY(BlueprintAssignable)
	FOnButtonDescriptionTextUpdatedDelegate OnButtonDescriptionTextUpdatedDelegate;

private:
	UPROPERTY(Transient)
	UWidgetPrimaryLayout* CreatedPrimaryLayout;

	// UUserWidget::GetWorld() can start returning the destination world after its
	// owning local player travels. Keep the world captured when the layout was
	// registered so an old menu layout is never mistaken for the lobby layout.
	TWeakObjectPtr<UWorld> CreatedPrimaryLayoutWorld;
};
