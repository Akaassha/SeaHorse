// Fill out your copyright notice in the Description page of Project Settings.


#include "Frontend/LoadingScreenSubsystem.h"
#include "PreLoadScreenManager.h"
#include "Engine/GameInstance.h"
#include "Engine/GameViewportClient.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "Frontend/Settings/LoadingScreenSettings.h"
#include "Blueprint/UserWidget.h"
#include "Frontend/Interfaces/LoadingScreenInterface.h"

bool ULoadingScreenSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	return !IsRunningDedicatedServer() && !IsRunningCommandlet();
}

void ULoadingScreenSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	FCoreUObjectDelegates::PreLoadMapWithContext.AddUObject(this, &ThisClass::OnMapPreLoaded);
	FCoreUObjectDelegates::PostLoadMapWithWorld.AddUObject(this, &ThisClass::OnMapPostLoaded);
}

void ULoadingScreenSubsystem::Deinitialize()
{
	TryRemoveLoadingScreen();
	if (GetGameInstance() && GetGameInstance()->GetGameViewportClient())
	{
		GetGameInstance()->GetGameViewportClient()->bDisableWorldRendering = false;
	}
	Super::Deinitialize();
	FCoreUObjectDelegates::PreLoadMapWithContext.RemoveAll(this);
	FCoreUObjectDelegates::PostLoadMapWithWorld.RemoveAll(this);
}

void ULoadingScreenSubsystem::OnMapPreLoaded(const FWorldContext& WorldContext, const FString& MapName)
{
	if (WorldContext.OwningGameInstance != GetGameInstance())
	{
		return;
	}

	SetTickableTickType(ETickableTickType::Conditional);

	bIsCurrentlyLoadingMap = true;

	TryUpdateLoadingScreen();
}

void ULoadingScreenSubsystem::OnMapPostLoaded(UWorld* LoadedWorld)
{
	if (LoadedWorld && LoadedWorld->GetGameInstance() == GetGameInstance())
	{
		bIsCurrentlyLoadingMap = false;
	}


}

void ULoadingScreenSubsystem::TryUpdateLoadingScreen()
{
	if (!GetGameInstance() || !GetGameInstance()->GetGameViewportClient()) { return; }
	if (IsPreLoadScreenActive())
	{
		return;
	}

	if (ShouldShowLoadingScreen())
	{
		TryDisplayLoadingScreenIfNone();

		OnLoadingReasonUpdatedDelegate.Broadcast(CurrentLoadingReason);
	}
	else
	{
		TryRemoveLoadingScreen();

		HoldLoadingScreenStartUpTime = -1.f;

		NotifyLoadingScreenVisibilityChanged(false);

		SetTickableTickType(ETickableTickType::Never);
	}
}

bool ULoadingScreenSubsystem::IsPreLoadScreenActive() const
{
	if (FPreLoadScreenManager* PreLoadScreenManager = FPreLoadScreenManager::Get())
	{
		return PreLoadScreenManager->HasValidActivePreLoadScreen();
	}

	return false;
}

bool ULoadingScreenSubsystem::ShouldShowLoadingScreen()
{
	const ULoadingScreenSettings* LoadingScreenSettings = GetDefault<ULoadingScreenSettings>();

	if (LoadingScreenSettings->SoftLoadingScreenWidgetClass.IsNull() ||
		(GIsEditor && !LoadingScreenSettings->bShouldLoadingScreenInEditor))
	{
		return false;
	}

	if (CheckTheNeedToShowLoadingScree())
	{
		// Keep the world visible if the optional loading widget cannot be created.
		GetGameInstance()->GetGameViewportClient()->bDisableWorldRendering = CachedCreatedLoadingWidget.IsValid();

		return true;
	}

	CurrentLoadingReason = TEXT("Waiting for Texture Streaming");

	GetGameInstance()->GetGameViewportClient()->bDisableWorldRendering = false;

	const float CurrentTime = FPlatformTime::Seconds();

	if (HoldLoadingScreenStartUpTime < 0.f)
	{
		HoldLoadingScreenStartUpTime = CurrentTime;
	}

	const float ElapsedTime = CurrentTime - HoldLoadingScreenStartUpTime;

	if (ElapsedTime < LoadingScreenSettings->HoldLoadingScreenExtraSeconds)
	{
		return true;
	}

	return false;
}

bool ULoadingScreenSubsystem::CheckTheNeedToShowLoadingScree()
{
	if (bIsCurrentlyLoadingMap)
	{
		CurrentLoadingReason = TEXT("Loading Level");

		return true;
	}

	UWorld* OwningWorld = GetGameInstance()->GetWorld();

	if (!OwningWorld)
	{
		CurrentLoadingReason = TEXT("Initializing World");

		return true;
	}

	if (!OwningWorld->HasBegunPlay())
	{
		CurrentLoadingReason = TEXT("World hasn't begun play yet");

		return true;
	}

	if (!OwningWorld->GetFirstPlayerController())
	{
		CurrentLoadingReason = TEXT("Player controller is not valid yet");

		return true;
	}

	//TO DO: check if the game states, player states, player character or others are ready

	return false;
}

void ULoadingScreenSubsystem::TryDisplayLoadingScreenIfNone()
{
	
	if (CachedCreatedLoadingWidget)
	{
		return;
	}

	const ULoadingScreenSettings* LoadingScreenSettings = GetDefault<ULoadingScreenSettings>();

	TSubclassOf<UUserWidget> LoadedWidgetClass = LoadingScreenSettings->GetLoadingScreenWidgetClassChecked();

	if (!LoadedWidgetClass) { return; }

	UUserWidget* CreatedWidget = UUserWidget::CreateWidgetInstance(*GetGameInstance(), LoadedWidgetClass, NAME_None);

	if (!CreatedWidget) { return; }
	LoadingWidget = CreatedWidget;

	CachedCreatedLoadingWidget = CreatedWidget->TakeWidget();

	GetGameInstance()->GetGameViewportClient()->AddViewportWidgetContent(
		CachedCreatedLoadingWidget.ToSharedRef(),
		1000
	);

	NotifyLoadingScreenVisibilityChanged(true);
}

void ULoadingScreenSubsystem::TryRemoveLoadingScreen()
{
	if (!CachedCreatedLoadingWidget)
	{
		return;
	}

	if (GetGameInstance() && GetGameInstance()->GetGameViewportClient())
	{
		GetGameInstance()->GetGameViewportClient()->RemoveViewportWidgetContent(CachedCreatedLoadingWidget.ToSharedRef());
	}

	CachedCreatedLoadingWidget.Reset();
	LoadingWidget = nullptr;
}

void ULoadingScreenSubsystem::NotifyLoadingScreenVisibilityChanged(bool bIsVisible)
{
	for (ULocalPlayer* ExistingLocalPLayer : GetGameInstance()->GetLocalPlayers())
	{
		if (!ExistingLocalPLayer)
		{
			continue;
		}

		if (APlayerController* PC = ExistingLocalPLayer->GetPlayerController(GetGameInstance()->GetWorld()))
		{
			if (PC->Implements<ULoadingScreenInterface>())
			{
				if (bIsVisible)
				{
					ILoadingScreenInterface::Execute_OnLoadingScreenActivated(PC);
				}
				else
				{
					ILoadingScreenInterface::Execute_OnLoadingScreenDeActivated(PC);
				}
				
			}

			if (APawn* OwningPawn = PC->GetPawn())
			{
				if (OwningPawn->Implements<ULoadingScreenInterface>())
				{
					if (bIsVisible)
					{
						ILoadingScreenInterface::Execute_OnLoadingScreenActivated(OwningPawn);
					}
					else
					{
						ILoadingScreenInterface::Execute_OnLoadingScreenDeActivated(OwningPawn);
					}

				}
			}


		}
	}
}

void ULoadingScreenSubsystem::Tick(float DeltaTime)
{
	TryUpdateLoadingScreen();
}

ETickableTickType ULoadingScreenSubsystem::GetTickableTickType() const
{
	if (IsTemplate())
	{
		return ETickableTickType::Never;
	}

	return ETickableTickType::Conditional;
}

bool ULoadingScreenSubsystem::IsTickable() const
{
	return GetGameInstance() && GetGameInstance()->GetGameViewportClient();
}

TStatId ULoadingScreenSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(ULoadingScreenSubsystem, STATGROUP_Tickables);
}

UWorld* ULoadingScreenSubsystem::GetTickableGameObjectWorld() const
{
	if (UGameInstance* OwningGameInstance = GetGameInstance())
	{
		return OwningGameInstance->GetWorld();
	}

	return nullptr;
}
