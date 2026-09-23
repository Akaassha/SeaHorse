#include "Frontend/Lobby/SHLobbyGameState.h"
#include "Frontend/Lobby/SHLobbyPlayerState.h"
#include "Net/UnrealNetwork.h"
#include "Online/SHOnlineSettings.h"

void ASHLobbyGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ASHLobbyGameState, Info);
}

TArray<ASHLobbyPlayerState*> ASHLobbyGameState::GetLobbyPlayers() const
{
	TArray<ASHLobbyPlayerState*> Players;
	for (APlayerState* Player : PlayerArray)
	{
		if (auto* LobbyPlayer = Cast<ASHLobbyPlayerState>(Player); IsValid(LobbyPlayer) && !LobbyPlayer->IsInactive()) { Players.Add(LobbyPlayer); }
	}
	Players.Sort([](const ASHLobbyPlayerState& A, const ASHLobbyPlayerState& B) { return A.GetPlayerId() < B.GetPlayerId(); });
	return Players;
}

bool ASHLobbyGameState::AreAllPlayersReady() const
{
	const auto Players = GetLobbyPlayers();
	if (Players.Num() < 2 || Players.Num() > Info.MaxPlayers ||
		Players.Num() > USHOnlineSettings::GetMapSeatCount(Info.SelectedMap) ||
		!IsValid(Info.Host) || !Players.Contains(Info.Host)) { return false; }
	for (const auto* Player : Players) { if (!Player->IsReady()) { return false; } }
	return true;
}

bool ASHLobbyGameState::CanStartMatch() const { return !Info.bStartingMatch && !Info.bChangingMap && AreAllPlayersReady(); }

bool ASHLobbyGameState::CanSelectMatchMap(ESHMatchMap Map) const
{
	const int32 Capacity = USHOnlineSettings::GetMapSeatCount(Map);
	return !Info.bStartingMatch && !Info.bChangingMap && Capacity > 0;
}

void ASHLobbyGameState::SetLobbyInfo(const FSHLobbyInfo& Value)
{
	if (!HasAuthority()) { return; }
	Info = Value;
	OnRep_Info();
	ForceNetUpdate();
}

void ASHLobbyGameState::OnRep_Info() { NotifyLobbyChanged(); }
void ASHLobbyGameState::NotifyLobbyChanged() { OnLobbyChanged.Broadcast(); }
void ASHLobbyGameState::AddPlayerState(APlayerState* PlayerState) { Super::AddPlayerState(PlayerState); NotifyLobbyChanged(); }
void ASHLobbyGameState::RemovePlayerState(APlayerState* PlayerState) { Super::RemovePlayerState(PlayerState); NotifyLobbyChanged(); }
