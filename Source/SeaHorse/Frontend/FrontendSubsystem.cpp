// Fill out your copyright notice in the Description page of Project Settings.


#include "Frontend/FrontendSubsystem.h"
#include "UObject/StrongObjectPtr.h"
#include "Frontend/Widgets/WidgetPrimaryLayout.h"
#include "Frontend/Widgets/WidgetActivatableBase.h"
#include "Widgets/CommonActivatableWidgetContainer.h"
#include "CommonActivatableWidget.h"
#include "GameplayTagContainer.h"
#include "Engine/AssetManager.h"
#include "GameFramework/PlayerController.h"
#include "Frontend/Widgets/WidgetConfirmScreen.h"
#include "NativeGameplayTags.h"
#include "Frontend/FrontendGameplayTags.h"
#include "Frontend/FrontendFunctionLibrary.h"

#include "Frontend/Debug/FrontendDebugHelper.h"

UFrontendSubsystem* UFrontendSubsystem::Get(const UObject* WorldContextObject)
{
	if (GEngine)
	{
		UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull);

		return World ? UGameInstance::GetSubsystem<UFrontendSubsystem>(World->GetGameInstance()) : nullptr;
	}

	return nullptr;
}

void UFrontendSubsystem::RegisterCreatedPrimaryLayoutWidget(UWidgetPrimaryLayout* InCreatedWidget)
{
	check(InCreatedWidget);
	UWorld* LayoutWorld = InCreatedWidget->GetWorld();
	if (LayoutWorld != GetWorld())
	{
		UE_LOG(LogTemp, Warning,
			TEXT("Ignoring primary layout from a world that is no longer current (layout world: %s, current world: %s)"),
			*GetNameSafe(LayoutWorld), *GetNameSafe(GetWorld()));
		return;
	}

	if (IsValid(CreatedPrimaryLayout) && CreatedPrimaryLayout != InCreatedWidget)
	{
		UE_LOG(LogTemp, Log,
			TEXT("[SH_FRONTEND] Removing previous primary layout during world transition (registered world: %s, new world: %s)"),
			*GetNameSafe(CreatedPrimaryLayoutWorld.Get()), *GetNameSafe(LayoutWorld));
		CreatedPrimaryLayout->SetVisibility(ESlateVisibility::Collapsed);
		CreatedPrimaryLayout->RemoveFromParent();
	}

	CreatedPrimaryLayout = InCreatedWidget;
	CreatedPrimaryLayoutWorld = LayoutWorld;

	Debug::Print(TEXT("Primary layout widget stored"));
}

bool UFrontendSubsystem::IsWidgetStackReady(const UWorld* ExpectedWorld, const FGameplayTag& InWidgetStackTag) const
{
	return ExpectedWorld && IsValid(CreatedPrimaryLayout) &&
		CreatedPrimaryLayoutWorld.Get() == ExpectedWorld &&
		CreatedPrimaryLayout->FindWidgetStackByTag(InWidgetStackTag) != nullptr;
}

bool UFrontendSubsystem::PrepareWidgetStackForPlayer(const UWorld* ExpectedWorld,
	APlayerController* OwningPlayer, const FGameplayTag& InWidgetStackTag)
{
	if (!IsWidgetStackReady(ExpectedWorld, InWidgetStackTag) || !IsValid(OwningPlayer) ||
		OwningPlayer->GetWorld() != ExpectedWorld || !OwningPlayer->IsLocalController())
	{
		return false;
	}

	if (CreatedPrimaryLayout->GetOwningPlayer() != OwningPlayer)
	{
		CreatedPrimaryLayout->SetOwningPlayer(OwningPlayer);
	}
	return true;
}

bool UFrontendSubsystem::DoesWidgetStackContainClass(const UWorld* ExpectedWorld,
	const FGameplayTag& InWidgetStackTag, const UClass* WidgetClass) const
{
	if (!WidgetClass || !IsWidgetStackReady(ExpectedWorld, InWidgetStackTag))
	{
		return false;
	}

	const UCommonActivatableWidgetContainerBase* Stack = CreatedPrimaryLayout->FindWidgetStackByTag(InWidgetStackTag);
	for (const UCommonActivatableWidget* Widget : Stack->GetWidgetList())
	{
		if (IsValid(Widget) && Widget->IsA(WidgetClass))
		{
			return true;
		}
	}
	return false;
}

void UFrontendSubsystem::PushSoftWidgetToStackAsync(const FGameplayTag& InWidgetStackTag, TSoftClassPtr<UWidgetActivatableBase> InSoftWidgetClass, TFunction<void(EAsyncPushWdgetState, UWidgetActivatableBase*)> AsyncPushStateCallback)
{
	if (InSoftWidgetClass.IsNull() || !IsWidgetStackReady(GetWorld(), InWidgetStackTag))
	{
		AsyncPushStateCallback(EAsyncPushWdgetState::Failed, nullptr);
		return;
	}
	TWeakObjectPtr<UFrontendSubsystem> WeakThis(this);
	TWeakObjectPtr<UWidgetPrimaryLayout> RequestedLayout(CreatedPrimaryLayout);
	TWeakObjectPtr<UWorld> RequestedLayoutWorld(CreatedPrimaryLayoutWorld);

	UAssetManager::Get().GetStreamableManager().RequestAsyncLoad(
		InSoftWidgetClass.ToSoftObjectPath(),
		FStreamableDelegate::CreateLambda(
			[InSoftWidgetClass, WeakThis, RequestedLayout, RequestedLayoutWorld, InWidgetStackTag, AsyncPushStateCallback]() {
				UClass* LoadedWidgetClass = InSoftWidgetClass.Get();

				if (!WeakThis.IsValid() || !RequestedLayout.IsValid() ||
					RequestedLayoutWorld.Get() != WeakThis->GetWorld() ||
					WeakThis->CreatedPrimaryLayoutWorld != RequestedLayoutWorld ||
					WeakThis->CreatedPrimaryLayout != RequestedLayout.Get() || !LoadedWidgetClass ||
					LoadedWidgetClass->HasAnyClassFlags(CLASS_Abstract))
				{
					AsyncPushStateCallback(EAsyncPushWdgetState::Failed, nullptr);
					return;
				}

				UCommonActivatableWidgetContainerBase* FoundWidgetStack = RequestedLayout->FindWidgetStackByTag(InWidgetStackTag);
				if (!FoundWidgetStack)
				{
					AsyncPushStateCallback(EAsyncPushWdgetState::Failed, nullptr);
					return;
				}

				UWidgetActivatableBase* CreatedWidget = FoundWidgetStack->AddWidget<UWidgetActivatableBase>(
					LoadedWidgetClass,
					[AsyncPushStateCallback](UWidgetActivatableBase& CreatedWidgetInstance) {
						AsyncPushStateCallback(EAsyncPushWdgetState::OnCreatedBeforePush, &CreatedWidgetInstance);
					}
				);

				AsyncPushStateCallback(CreatedWidget ? EAsyncPushWdgetState::AfterPush : EAsyncPushWdgetState::Failed, CreatedWidget);
			}
		)
	);
}

void UFrontendSubsystem::PushConfirmScreenToModalStackAsync(EConfirmScreenType InScreenType, const FText& InScreenTitle, const FText& InScreenMsg, TFunction<void(EConfirmScreenButtonType)> ButtonClickedCallback)
{
	UConfirmScreenInfoObject* CreatedInfoObject = nullptr;

	switch (InScreenType)
	{
	case EConfirmScreenType::Ok:
		CreatedInfoObject = UConfirmScreenInfoObject::CreateOkScreen(InScreenTitle, InScreenMsg);
		break;

	case EConfirmScreenType::YesNo:
		CreatedInfoObject = UConfirmScreenInfoObject::CreateYesNoScreen(InScreenTitle, InScreenMsg);
		break;

	case EConfirmScreenType::OkCancel:
		CreatedInfoObject = UConfirmScreenInfoObject::CreateOkCancelScreen(InScreenTitle, InScreenMsg);
		break;

	case EConfirmScreenType::Unknown:
		break;

	default:
		break;
	}

	if (!CreatedInfoObject)
	{
		ButtonClickedCallback(EConfirmScreenButtonType::Canceled);
		return;
	}
	TStrongObjectPtr<UConfirmScreenInfoObject> Info(CreatedInfoObject);

	PushSoftWidgetToStackAsync(
		FrontendGameplayTags::Frontend_WidgetStack_GameMenu,
		UFrontendFunctionLibrary::GetFrontendSoftWidgetClassByTag(FrontendGameplayTags::Frontend_Widget_ConfirmScreen),
		[Info, ButtonClickedCallback](EAsyncPushWdgetState InPushState, UWidgetActivatableBase* PushedWidget)
		{
			if (InPushState == EAsyncPushWdgetState::Failed)
			{
				ButtonClickedCallback(EConfirmScreenButtonType::Canceled);
			}
			else if (InPushState == EAsyncPushWdgetState::OnCreatedBeforePush)
			{
				if (UWidgetConfirmScreen* CreatedConfirmScreen = Cast<UWidgetConfirmScreen>(PushedWidget))
				{
					CreatedConfirmScreen->InitConfirmScreen(Info.Get(), ButtonClickedCallback);
				}
				else { ButtonClickedCallback(EConfirmScreenButtonType::Canceled); }
			}
		}
	);
}
