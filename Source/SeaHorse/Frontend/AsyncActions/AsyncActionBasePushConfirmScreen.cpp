// Fill out your copyright notice in the Description page of Project Settings.

#include "Frontend/AsyncActions/AsyncActionBasePushConfirmScreen.h"
#include "Frontend/FrontendSubsystem.h"

UAsyncActionBasePushConfirmScreen* UAsyncActionBasePushConfirmScreen::PushConfirmScreen(const UObject* WorldContextObject, EConfirmScreenType ScreenType, FText InScreenTitle, FText InScreenMessage)
{
    if (GEngine)
    {
        if (UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull))
        {
            UAsyncActionBasePushConfirmScreen* Node = NewObject<UAsyncActionBasePushConfirmScreen>();

            Node->CachedOwningWorld = World;
            Node->CachedScreenType = ScreenType;
            Node->CachedScreenTitle = InScreenTitle;
            Node->CachedScreenMessege = InScreenMessage;

            Node->RegisterWithGameInstance(World);

            return Node;
        }
    }

    return nullptr;
}

void UAsyncActionBasePushConfirmScreen::Activate()
{
    UFrontendSubsystem* Subsystem = UFrontendSubsystem::Get(CachedOwningWorld.Get());
    if (!Subsystem)
    {
        OnButtonClicked.Broadcast(EConfirmScreenButtonType::Canceled);
        SetReadyToDestroy();
        return;
    }
    Subsystem->PushConfirmScreenToModalStackAsync(
        CachedScreenType,
        CachedScreenTitle,
        CachedScreenMessege,
        [this](EConfirmScreenButtonType ClickedButtonType) 
        {
            OnButtonClicked.Broadcast(ClickedButtonType);

            SetReadyToDestroy();
        }
    );

}
