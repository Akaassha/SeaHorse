#include "Frontend/Lobby/SHLobbyPlayerController.h"
#include "Frontend/Lobby/SHLobbyGameMode.h"
#include "Frontend/Lobby/SHLobbyGameState.h"
#include "Frontend/Lobby/SHLobbyPlayerState.h"
#include "Frontend/FrontendFunctionLibrary.h"
#include "Frontend/FrontendGameplayTags.h"
#include "Frontend/FrontendSubsystem.h"
#include "Frontend/Settings/FrontendDeveloperSettings.h"
#include "Frontend/Widgets/WidgetActivatableBase.h"
#include "Frontend/Widgets/WidgetPrimaryLayout.h"
#include "TimerManager.h"

namespace
{
	constexpr float LobbyFrontendInitialDelay = 0.25f;
	constexpr float LobbyFrontendRetryDelay = 0.1f;
	constexpr int32 MaxLobbyFrontendAttempts = 50;
}

ASHLobbyPlayerController::ASHLobbyPlayerController()
{
	bShowMouseCursor = true;
	PrimaryActorTick.bCanEverTick = false;
}

void ASHLobbyPlayerController::ReceivedPlayer()
{
	Super::ReceivedPlayer();
	if (IsLocalController() && GetNetMode() == NM_Client)
	{
		GetWorldTimerManager().SetTimer(LobbyFrontendTimer, this,
			&ThisClass::EnsureClientLobbyFrontend, LobbyFrontendInitialDelay, false);
	}
}

void ASHLobbyPlayerController::RetryClientLobbyFrontend()
{
	if (++LobbyFrontendAttempts >= MaxLobbyFrontendAttempts)
	{
		UE_LOG(LogTemp, Error, TEXT("[SH_LOBBY_UI] Client lobby frontend was not ready after %d attempts in world %s"),
			LobbyFrontendAttempts, *GetNameSafe(GetWorld()));
		return;
	}
	GetWorldTimerManager().SetTimer(LobbyFrontendTimer, this,
		&ThisClass::EnsureClientLobbyFrontend, LobbyFrontendRetryDelay, false);
}

void ASHLobbyPlayerController::EnsureClientLobbyFrontend()
{
	UWorld* World = GetWorld();
	if (!World || !IsLocalController() || GetNetMode() != NM_Client)
	{
		return;
	}
	if (!World->GetGameState<ASHLobbyGameState>())
	{
		RetryClientLobbyFrontend();
		return;
	}

	UFrontendSubsystem* Frontend = UFrontendSubsystem::Get(World);
	if (!Frontend)
	{
		RetryClientLobbyFrontend();
		return;
	}

	if (!Frontend->PrepareWidgetStackForPlayer(
		World, this, FrontendGameplayTags::Frontend_WidgetStack_GameMenu))
	{
		const TSoftClassPtr<UWidgetPrimaryLayout> LayoutClass =
			GetDefault<UFrontendDeveloperSettings>()->PrimaryLayoutWidgetClass;
		UClass* LoadedLayoutClass = LayoutClass.LoadSynchronous();
		if (!LoadedLayoutClass)
		{
			UE_LOG(LogTemp, Error, TEXT("[SH_LOBBY_UI] Primary layout class is not configured or could not be loaded"));
			return;
		}

		ClientPrimaryLayout = CreateWidget<UWidgetPrimaryLayout>(this, LoadedLayoutClass);
		if (!ClientPrimaryLayout || !ClientPrimaryLayout->AddToPlayerScreen())
		{
			ClientPrimaryLayout = nullptr;
			RetryClientLobbyFrontend();
			return;
		}
		Frontend->RegisterCreatedPrimaryLayoutWidget(ClientPrimaryLayout);
		UE_LOG(LogTemp, Log, TEXT("[SH_LOBBY_UI] Restored the primary layout for remote client in %s"),
			*GetNameSafe(World));
	}

	// Give the map's normal async push one more frame. If it did not run, the controller
	// supplies the lobby screen itself after the network player is fully received.
	GetWorldTimerManager().SetTimer(LobbyFrontendTimer, this,
		&ThisClass::EnsureClientLobbyScreen, LobbyFrontendRetryDelay, false);
}

void ASHLobbyPlayerController::EnsureClientLobbyScreen()
{
	UWorld* World = GetWorld();
	UFrontendSubsystem* Frontend = UFrontendSubsystem::Get(World);
	if (!World || !Frontend || !IsLocalController() || GetNetMode() != NM_Client ||
		!Frontend->PrepareWidgetStackForPlayer(
			World, this, FrontendGameplayTags::Frontend_WidgetStack_GameMenu))
	{
		RetryClientLobbyFrontend();
		return;
	}

	const TSoftClassPtr<UWidgetActivatableBase> LobbyScreen =
		UFrontendFunctionLibrary::GetFrontendSoftWidgetClassByTag(
			FrontendGameplayTags::Frontend_Widget_LobbyScreen);
	UClass* LoadedLobbyClass = LobbyScreen.LoadSynchronous();
	if (!LoadedLobbyClass)
	{
		UE_LOG(LogTemp, Error, TEXT("[SH_LOBBY_UI] Lobby screen class is not configured or could not be loaded"));
		return;
	}
	if (Frontend->DoesWidgetStackContainClass(
		World, FrontendGameplayTags::Frontend_WidgetStack_GameMenu, LoadedLobbyClass))
	{
		UE_LOG(LogTemp, Log, TEXT("[SH_LOBBY_UI] Lobby screen confirmed for remote client in %s"),
			*GetNameSafe(World));
		return;
	}

	TWeakObjectPtr<ThisClass> WeakThis(this);
	Frontend->PushSoftWidgetToStackAsync(
		FrontendGameplayTags::Frontend_WidgetStack_GameMenu, LobbyScreen,
		[WeakThis](EAsyncPushWdgetState State, UWidgetActivatableBase* Widget)
		{
			ThisClass* Controller = WeakThis.Get();
			if (!Controller) { return; }
			if (State == EAsyncPushWdgetState::OnCreatedBeforePush && Widget)
			{
				Widget->SetOwningPlayer(Controller);
			}
			else if (State == EAsyncPushWdgetState::AfterPush && Widget)
			{
				UE_LOG(LogTemp, Log, TEXT("[SH_LOBBY_UI] Restored the lobby screen for remote client"));
			}
			else if (State == EAsyncPushWdgetState::Failed)
			{
				Controller->RetryClientLobbyFrontend();
			}
		});
}

bool ASHLobbyPlayerController::CanSetOptionalRules() const
{
	const auto* State = GetWorld() ? GetWorld()->GetGameState<ASHLobbyGameState>() : nullptr;
	return IsLobbyHost() && State && !State->GetLobbyInfo().bStartingMatch && !State->GetLobbyInfo().bChangingMap;
}

void ASHLobbyPlayerController::ServerSetOptionalRules_Implementation(const FSHOptionalRules& Rules)
{
	if (auto* Mode = GetWorld()->GetAuthGameMode<ASHLobbyGameMode>()) { Mode->RequestSetOptionalRules(this, Rules); }
	else { ClientLobbyRequestRejected(TEXT("Optional rules can only be selected in the lobby.")); }
}

bool ASHLobbyPlayerController::IsLobbyHost() const
{
	const auto* State = GetWorld() ? GetWorld()->GetGameState<ASHLobbyGameState>() : nullptr;
	return State && PlayerState && State->GetLobbyInfo().Host == PlayerState;
}

void ASHLobbyPlayerController::ServerSetReady_Implementation(bool bReady)
{
	if (auto* Mode = GetWorld()->GetAuthGameMode<ASHLobbyGameMode>()) { Mode->SetPlayerReady(this, bReady); }
	else { ClientLobbyRequestRejected(TEXT("Readiness is only available in the lobby.")); }
}

void ASHLobbyPlayerController::ServerStartMatch_Implementation()
{
	if (auto* Mode = GetWorld()->GetAuthGameMode<ASHLobbyGameMode>()) { Mode->RequestStartMatch(this); }
	else { ClientLobbyRequestRejected(TEXT("The match can only be started from the lobby.")); }
}

void ASHLobbyPlayerController::ClientLobbyRequestRejected_Implementation(const FString& Reason)
{
	OnLobbyRequestRejected.Broadcast(Reason);
}

bool ASHLobbyPlayerController::CanSelectMatchMap(ESHMatchMap Map) const
{
	const auto* State = GetWorld() ? GetWorld()->GetGameState<ASHLobbyGameState>() : nullptr;
	return IsLobbyHost() && State && State->CanSelectMatchMap(Map);
}

void ASHLobbyPlayerController::ServerSelectMatchMap_Implementation(ESHMatchMap Map)
{
	if (auto* Mode = GetWorld()->GetAuthGameMode<ASHLobbyGameMode>()) { Mode->RequestSelectMatchMap(this, Map); }
	else { ClientLobbyRequestRejected(TEXT("Maps can only be selected in the lobby.")); }
}
