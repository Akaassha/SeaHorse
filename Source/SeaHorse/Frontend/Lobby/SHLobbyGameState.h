#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameStateBase.h"
#include "Online/SHSessionTypes.h"
#include "SHLobbyGameState.generated.h"

class ASHLobbyPlayerState;
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FSHLobbyChanged);

USTRUCT(BlueprintType)
struct FSHLobbyInfo
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly) FString ServerName;
	UPROPERTY(BlueprintReadOnly) int32 MaxPlayers = 4;
	UPROPERTY(BlueprintReadOnly) ESHMatchMap SelectedMap = ESHMatchMap::Small;
	UPROPERTY(BlueprintReadOnly) bool bChangingMap = false;
	UPROPERTY(BlueprintReadOnly) bool bStartingMatch = false;
	UPROPERTY(BlueprintReadOnly) TObjectPtr<ASHLobbyPlayerState> Host;
};

UCLASS()
class SEAHORSE_API ASHLobbyGameState : public AGameStateBase
{
	GENERATED_BODY()
public:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void AddPlayerState(APlayerState* PlayerState) override;
	virtual void RemovePlayerState(APlayerState* PlayerState) override;
	UFUNCTION(BlueprintPure, Category="SeaHorse|Lobby") FSHLobbyInfo GetLobbyInfo() const { return Info; }
	UFUNCTION(BlueprintPure, Category="SeaHorse|Lobby") TArray<ASHLobbyPlayerState*> GetLobbyPlayers() const;
	UFUNCTION(BlueprintPure, Category="SeaHorse|Lobby") bool AreAllPlayersReady() const;
	UFUNCTION(BlueprintPure, Category="SeaHorse|Lobby") bool CanStartMatch() const;
	UFUNCTION(BlueprintPure, Category="SeaHorse|Lobby") bool CanSelectMatchMap(ESHMatchMap Map) const;
	UPROPERTY(BlueprintAssignable) FSHLobbyChanged OnLobbyChanged;
	void SetLobbyInfo(const FSHLobbyInfo& Value);
	void NotifyLobbyChanged();
private:
	UPROPERTY(ReplicatedUsing=OnRep_Info) FSHLobbyInfo Info;
	UFUNCTION() void OnRep_Info();
};
