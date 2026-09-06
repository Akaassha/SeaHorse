#include "Frontend/Lobby/SHLobbyPlayerController.h"
#include "Frontend/Lobby/SHLobbyGameMode.h"
#include "Frontend/Lobby/SHLobbyGameState.h"
#include "Frontend/Lobby/SHLobbyPlayerState.h"

ASHLobbyPlayerController::ASHLobbyPlayerController()
{
	bShowMouseCursor = true;
	PrimaryActorTick.bCanEverTick = false;
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
