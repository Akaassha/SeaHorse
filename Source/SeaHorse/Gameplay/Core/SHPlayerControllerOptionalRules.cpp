#include "Gameplay/Core/SHPlayerController.h"
#include "Gameplay/Core/SHGameMode.h"
#include "Gameplay/Core/SHPlayerState.h"

void ASHPlayerController::ServerRemoveOrphanedRatfolk_Implementation(ASHCard* Card)
{
	ASHGameMode* Mode = GetWorld() ? GetWorld()->GetAuthGameMode<ASHGameMode>() : nullptr;
	const bool bRemoved = IsValid(Mode) && Mode->RequestRemoveOrphanedRatfolk(GetPlayerState<ASHPlayerState>(), Card);
	ClientOrphanedRatfolkRemovalResult(bRemoved);
}

void ASHPlayerController::ClientOrphanedRatfolkRemovalResult_Implementation(bool bRemoved)
{
	OnOrphanedRatfolkRemovalResult.Broadcast(bRemoved);
}
