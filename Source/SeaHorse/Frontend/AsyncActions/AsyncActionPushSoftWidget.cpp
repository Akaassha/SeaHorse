// Fill out your copyright notice in the Description page of Project Settings.


#include "Frontend/AsyncActions/AsyncActionPushSoftWidget.h"
#include "Frontend/Widgets/WidgetActivatableBase.h"
#include "Frontend/FrontendSubsystem.h"

UAsyncActionPushSoftWidget* UAsyncActionPushSoftWidget::PushSoftWidget(const UObject* WorldContextObject, APlayerController* OwningPlayerController, TSoftClassPtr<UWidgetActivatableBase> InSoftWidgetClass, UPARAM(meta = (Categories = "Frontend.WidgetStack")) FGameplayTag InWidgetStackTag, bool bFocusOnNewlyPushedWidget)
{
	

	if (GEngine)
	{
		if (UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull))
		{
			UAsyncActionPushSoftWidget* Node = NewObject<UAsyncActionPushSoftWidget>();

			Node->RegisterWithGameInstance(World);
			Node->CachedOwningWorld = World;
			Node->CachedOwningPlayerController = OwningPlayerController;
			Node->CachedSoftWidgetClass = InSoftWidgetClass;
			Node->CachedWidgetStackTag = InWidgetStackTag;
			Node->bCachedFocusOnNewlyPushedWidget = bFocusOnNewlyPushedWidget;

			return Node;
		}
	}
	return nullptr;
}

void UAsyncActionPushSoftWidget::Activate()
{
	if (bActivated) { return; }
	bActivated = true;
	UFrontendSubsystem* FrontendSubsystem = UFrontendSubsystem::Get(CachedOwningWorld.Get());

	

	if (!FrontendSubsystem || !CachedOwningPlayerController.IsValid() ||
		!CachedOwningPlayerController->IsLocalController())
	{
		bCompleted = true;
		AfterPush.Broadcast(nullptr);
		SetReadyToDestroy();
		return;
	}
	FrontendSubsystem->PushSoftWidgetToStackAsync(CachedWidgetStackTag, CachedSoftWidgetClass, 
		[WeakThis = TWeakObjectPtr<ThisClass>(this)](EAsyncPushWdgetState InPushState, UWidgetActivatableBase* PushedWidget)
		{
			ThisClass* Node = WeakThis.Get();
			if (!Node || Node->bCompleted) { return; }
			if (!Node->CachedOwningWorld.IsValid() || !Node->CachedOwningPlayerController.IsValid() ||
				Node->CachedOwningPlayerController->GetWorld() != Node->CachedOwningWorld.Get())
			{
				Node->bCompleted = true;
				Node->AfterPush.Broadcast(nullptr);
				Node->SetReadyToDestroy();
				return;
			}
			switch (InPushState)
			{
			case EAsyncPushWdgetState::Failed:
				Node->bCompleted = true;
				Node->AfterPush.Broadcast(nullptr);
				Node->SetReadyToDestroy();
				break;
			case EAsyncPushWdgetState::OnCreatedBeforePush:

				PushedWidget->SetOwningPlayer(Node->CachedOwningPlayerController.Get());
				Node->OnWidgetCreatedBedorePush.Broadcast(PushedWidget);

				break;
			case EAsyncPushWdgetState::AfterPush:

				Node->bCompleted = true;
				if (Node->bCachedFocusOnNewlyPushedWidget)
				{
					if (UWidget* WidgetToFocus = PushedWidget->GetDesiredFocusTarget())
					{
						WidgetToFocus->SetFocus();
					}
				}

				Node->AfterPush.Broadcast(PushedWidget);
				Node->SetReadyToDestroy();

				break;
			default:
				break;
			}
		}
	);
}
