// Fill out your copyright notice in the Description page of Project Settings.


#include "Frontend/AsyncActions/AsyncActionPushSoftWidget.h"
#include "Frontend/Widgets/WidgetActivatableBase.h"
#include "Frontend/FrontendSubsystem.h"
#include "HAL/PlatformTime.h"
#include "TimerManager.h"

namespace
{
	constexpr float LayoutRetryIntervalSeconds = 0.05f;
	constexpr double LayoutWaitTimeoutSeconds = 10.0;
}

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
	LayoutWaitStartedAtSeconds = FPlatformTime::Seconds();
	TryPush();
}

void UAsyncActionPushSoftWidget::TryPush()
{
	if (bCompleted || bPushStarted) { return; }

	UWorld* OwningWorld = CachedOwningWorld.Get();
	if (!OwningWorld || CachedSoftWidgetClass.IsNull())
	{
		CompleteFailed();
		return;
	}

	APlayerController* OwningPlayerController = CachedOwningPlayerController.Get();
	if (!OwningPlayerController || OwningPlayerController->GetWorld() != OwningWorld ||
		!OwningPlayerController->IsLocalController())
	{
		OwningPlayerController = OwningWorld->GetFirstPlayerController();
		if (OwningPlayerController && OwningPlayerController->GetWorld() == OwningWorld &&
			OwningPlayerController->IsLocalController())
		{
			CachedOwningPlayerController = OwningPlayerController;
		}
		else
		{
			CachedOwningPlayerController.Reset();
			RetryOrFail(TEXT("local player controller"));
			return;
		}
	}

	UFrontendSubsystem* FrontendSubsystem = UFrontendSubsystem::Get(OwningWorld);
	if (!FrontendSubsystem)
	{
		CompleteFailed();
		return;
	}

	if (!FrontendSubsystem->PrepareWidgetStackForPlayer(
		OwningWorld, OwningPlayerController, CachedWidgetStackTag))
	{
		RetryOrFail(TEXT("current-world primary layout and widget stack"));
		return;
	}

	bPushStarted = true;
	FrontendSubsystem->PushSoftWidgetToStackAsync(CachedWidgetStackTag, CachedSoftWidgetClass, 
		[WeakThis = TWeakObjectPtr<ThisClass>(this)](EAsyncPushWdgetState InPushState, UWidgetActivatableBase* PushedWidget)
		{
			ThisClass* Node = WeakThis.Get();
			if (!Node || Node->bCompleted) { return; }
			if (!Node->CachedOwningWorld.IsValid() || !Node->CachedOwningPlayerController.IsValid() ||
				Node->CachedOwningPlayerController->GetWorld() != Node->CachedOwningWorld.Get())
			{
				Node->CompleteFailed();
				return;
			}
			switch (InPushState)
			{
			case EAsyncPushWdgetState::Failed:
				Node->CompleteFailed();
				break;
			case EAsyncPushWdgetState::OnCreatedBeforePush:
				if (PushedWidget)
				{
					PushedWidget->SetOwningPlayer(Node->CachedOwningPlayerController.Get());
					Node->OnWidgetCreatedBedorePush.Broadcast(PushedWidget);
				}

				break;
			case EAsyncPushWdgetState::AfterPush:
				if (!PushedWidget)
				{
					Node->CompleteFailed();
					break;
				}

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

void UAsyncActionPushSoftWidget::RetryOrFail(const TCHAR* WaitingFor)
{
	UWorld* OwningWorld = CachedOwningWorld.Get();
	if (!OwningWorld || FPlatformTime::Seconds() - LayoutWaitStartedAtSeconds >= LayoutWaitTimeoutSeconds)
	{
		UE_LOG(LogTemp, Error, TEXT("Timed out waiting for %s before opening frontend stack %s in world %s"),
			WaitingFor, *CachedWidgetStackTag.ToString(), *GetNameSafe(OwningWorld));
		CompleteFailed();
		return;
	}

	OwningWorld->GetTimerManager().SetTimer(
		LayoutRetryTimer, this, &ThisClass::TryPush, LayoutRetryIntervalSeconds, false);
}

void UAsyncActionPushSoftWidget::CompleteFailed()
{
	if (bCompleted) { return; }
	bCompleted = true;
	if (UWorld* OwningWorld = CachedOwningWorld.Get())
	{
		OwningWorld->GetTimerManager().ClearTimer(LayoutRetryTimer);
	}
	AfterPush.Broadcast(nullptr);
	SetReadyToDestroy();
}
