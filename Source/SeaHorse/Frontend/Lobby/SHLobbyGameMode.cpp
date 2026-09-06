#include "Frontend/Lobby/SHLobbyGameMode.h"
#include "Frontend/Lobby/SHLobbyGameState.h"
#include "Frontend/Lobby/SHLobbyPlayerState.h"
#include "Frontend/Lobby/SHLobbyPlayerController.h"
#include "Online/SHSessionSubsystem.h"
#include "Engine/GameInstance.h"
#include "GameFramework/GameSession.h"

ASHLobbyGameMode::ASHLobbyGameMode()
{
	GameStateClass = ASHLobbyGameState::StaticClass();
	PlayerStateClass = ASHLobbyPlayerState::StaticClass();
	PlayerControllerClass = ASHLobbyPlayerController::StaticClass();
	DefaultPawnClass = nullptr;
	bStartPlayersAsSpectators = false;
	// Preserve the Steam connection while replacing lobby controllers/states on the match map.
	// AGameMode calls HandleStartingNewPlayer for migrated players as well as fresh logins.
	bUseSeamlessTravel = true;
}

ASHMainMenuGameMode::ASHMainMenuGameMode()
{
	PlayerControllerClass = ASHLobbyPlayerController::StaticClass();
	DefaultPawnClass = nullptr;
}

void ASHLobbyGameMode::InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage)
{
	Super::InitGame(MapName, Options, ErrorMessage);
	const auto* Sessions = GetGameInstance()->GetSubsystem<USHSessionSubsystem>();
	if (GameSession) { GameSession->MaxPlayers = Sessions ? Sessions->GetHostedMaxPlayers() : 4; }
}

void ASHLobbyGameMode::InitGameState()
{
	Super::InitGameState();
	FSHLobbyInfo Info;
	if (auto* Sessions = GetGameInstance()->GetSubsystem<USHSessionSubsystem>())
	{
		Info.ServerName = Sessions->GetHostedServerName();
		Info.MaxPlayers = Sessions->GetHostedMaxPlayers();
	}
	GetGameState<ASHLobbyGameState>()->SetLobbyInfo(Info);
}

void ASHLobbyGameMode::PreLogin(const FString& Options, const FString& Address, const FUniqueNetIdRepl& UniqueId, FString& ErrorMessage)
{
	Super::PreLogin(Options, Address, UniqueId, ErrorMessage);
	if (!ErrorMessage.IsEmpty()) { return; }
	const auto* State = GetGameState<ASHLobbyGameState>();
	if (!State || State->GetLobbyInfo().bStartingMatch) { ErrorMessage = TEXT("The match is already starting."); }
	else if (GetNumPlayers() >= State->GetLobbyInfo().MaxPlayers) { ErrorMessage = TEXT("The lobby is full."); }
}

void ASHLobbyGameMode::PostLogin(APlayerController* NewPlayer)
{
	Super::PostLogin(NewPlayer);
	auto* State = GetGameState<ASHLobbyGameState>();
	auto* Player = NewPlayer->GetPlayerState<ASHLobbyPlayerState>();
	if (!State || !Player) { return; }
	// Recheck after admission: multiple pending connections may have passed PreLogin together.
	if (!NewPlayer->IsLocalController() && (State->GetLobbyInfo().bStartingMatch ||
		State->GetLobbyPlayers().Num() > State->GetLobbyInfo().MaxPlayers))
	{
		GameSession->KickPlayer(NewPlayer, FText::FromString(TEXT("The lobby is full or starting.")));
		return;
	}
	// A remote client can never claim host privileges by logging in first.
	if (NewPlayer->IsLocalController())
	{
		FSHLobbyInfo Info = State->GetLobbyInfo();
		Info.Host = Player;
		State->SetLobbyInfo(Info);
		Player->SetReady(true);
	}
	State->NotifyLobbyChanged();
}

void ASHLobbyGameMode::Logout(AController* Exiting)
{
	Super::Logout(Exiting);
	if (auto* State = GetGameState<ASHLobbyGameState>()) { State->NotifyLobbyChanged(); }
}

void ASHLobbyGameMode::SetPlayerReady(ASHLobbyPlayerController* Player, bool bReady)
{
	auto* State = GetGameState<ASHLobbyGameState>();
	auto* PlayerState = IsValid(Player) ? Player->GetPlayerState<ASHLobbyPlayerState>() : nullptr;
	if (!State || !PlayerState || !State->PlayerArray.Contains(PlayerState)) { return; }
	if (State->GetLobbyInfo().bStartingMatch) { Player->ClientLobbyRequestRejected(TEXT("The match is already starting.")); return; }
	PlayerState->SetReady(bReady);
}

bool ASHLobbyGameMode::IsStartingRosterValid(int32 ExpectedPlayers) const
{
	const auto* State = GetGameState<ASHLobbyGameState>();
	return State && State->GetLobbyPlayers().Num() == ExpectedPlayers && State->AreAllPlayersReady();
}

void ASHLobbyGameMode::RequestStartMatch(ASHLobbyPlayerController* Player)
{
	auto* State = GetGameState<ASHLobbyGameState>();
	if (!IsValid(Player) || !State) { return; }
	if (!Player->IsLocalController() || !Player->IsLobbyHost())
	{
		Player->ClientLobbyRequestRejected(TEXT("Only the host can start the match.")); return;
	}
	if (!State->CanStartMatch())
	{
		Player->ClientLobbyRequestRejected(TEXT("At least two players must be present and everyone must be ready.")); return;
	}
	auto* Sessions = GetGameInstance()->GetSubsystem<USHSessionSubsystem>();
	if (!Sessions) { Player->ClientLobbyRequestRejected(TEXT("Session subsystem is unavailable.")); return; }
	FSHLobbyInfo Info = State->GetLobbyInfo();
	Info.bStartingMatch = true;
	State->SetLobbyInfo(Info);
	TWeakObjectPtr<ASHLobbyGameMode> WeakThis(this);
	TWeakObjectPtr<ASHLobbyPlayerController> WeakPlayer(Player);
	Sessions->StartLobbyMatch(State->GetLobbyPlayers().Num(), [WeakThis, WeakPlayer](bool bSuccess, const FString& Error)
	{
		if (!bSuccess && WeakThis.IsValid())
		{
			if (auto* CurrentState = WeakThis->GetGameState<ASHLobbyGameState>())
			{
				FSHLobbyInfo Current = CurrentState->GetLobbyInfo();
				Current.bStartingMatch = false;
				CurrentState->SetLobbyInfo(Current);
			}
			if (WeakPlayer.IsValid()) { WeakPlayer->ClientLobbyRequestRejected(Error); }
		}
	});
}
