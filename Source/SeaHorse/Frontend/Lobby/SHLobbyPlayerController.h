#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "SHLobbyPlayerController.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FSHLobbyRequestRejected, const FString&, Reason);

UCLASS()
class SEAHORSE_API ASHLobbyPlayerController : public APlayerController
{
	GENERATED_BODY()
public:
	ASHLobbyPlayerController();
	UFUNCTION(BlueprintCallable, Server, Reliable, Category="SeaHorse|Lobby") void ServerSetReady(bool bReady);
	UFUNCTION(BlueprintCallable, Server, Reliable, Category="SeaHorse|Lobby") void ServerStartMatch();
	UFUNCTION(BlueprintPure, Category="SeaHorse|Lobby") bool IsLobbyHost() const;
	UPROPERTY(BlueprintAssignable) FSHLobbyRequestRejected OnLobbyRequestRejected;
	UFUNCTION(Client, Reliable) void ClientLobbyRequestRejected(const FString& Reason);
};
