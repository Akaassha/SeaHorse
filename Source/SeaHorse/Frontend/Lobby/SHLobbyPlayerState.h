#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerState.h"
#include "SHLobbyPlayerState.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FSHLobbyPlayerChanged);

UCLASS()
class SEAHORSE_API ASHLobbyPlayerState : public APlayerState
{
	GENERATED_BODY()
public:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void OnRep_PlayerName() override;
	UFUNCTION(BlueprintPure, Category="SeaHorse|Lobby") bool IsReady() const { return bReady; }
	UPROPERTY(BlueprintAssignable) FSHLobbyPlayerChanged OnLobbyPlayerChanged;
	void SetReady(bool bValue);
private:
	UPROPERTY(ReplicatedUsing=OnRep_Ready) bool bReady = false;
	UFUNCTION() void OnRep_Ready();
};
