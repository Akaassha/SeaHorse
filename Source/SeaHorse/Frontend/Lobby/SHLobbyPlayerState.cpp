#include "Frontend/Lobby/SHLobbyPlayerState.h"
#include "Frontend/Lobby/SHLobbyGameState.h"
#include "Net/UnrealNetwork.h"

void ASHLobbyPlayerState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ASHLobbyPlayerState, bReady);
}

void ASHLobbyPlayerState::SetReady(bool bValue)
{
	if (!HasAuthority() || bReady == bValue) { return; }
	bReady = bValue;
	OnRep_Ready();
	ForceNetUpdate();
}

void ASHLobbyPlayerState::OnRep_Ready()
{
	OnLobbyPlayerChanged.Broadcast();
	if (GetWorld())
	{
		if (auto* State = GetWorld()->GetGameState<ASHLobbyGameState>()) { State->NotifyLobbyChanged(); }
	}
}

void ASHLobbyPlayerState::OnRep_PlayerName()
{
	Super::OnRep_PlayerName();
	OnRep_Ready();
}
