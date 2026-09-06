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
	UFrontendSubsystem* FrontendSubsystem = UFrontendSubsystem::Get(CachedOwningWorld.Get());

	

	if (!FrontendSubsystem || !CachedOwningPlayerController.IsValid() ||
		!CachedOwningPlayerController->IsLocalController())
	{
		AfterPush.Broadcast(nullptr);
		SetReadyToDestroy();
		return;
	}
	FrontendSubsystem->PushSoftWidgetToStackAsync(CachedWidgetStackTag, CachedSoftWidgetClass, 
		[this](EAsyncPushWdgetState InPushState, UWidgetActivatableBase* PushedWidget)
		{
			switch (InPushState)
			{
			case EAsyncPushWdgetState::Failed:
				AfterPush.Broadcast(nullptr);
				SetReadyToDestroy();
				break;
			case EAsyncPushWdgetState::OnCreatedBeforePush:

				PushedWidget->SetOwningPlayer(CachedOwningPlayerController.Get());
				OnWidgetCreatedBedorePush.Broadcast(PushedWidget);

				break;
			case EAsyncPushWdgetState::AfterPush:

				AfterPush.Broadcast(PushedWidget);

				if (bCachedFocusOnNewlyPushedWidget)
				{
					if (UWidget* WidgetToFocus = PushedWidget->GetDesiredFocusTarget())
					{
						WidgetToFocus->SetFocus();
					}
				}

				SetReadyToDestroy();

				break;
			default:
				break;
			}
		}
	);
}
