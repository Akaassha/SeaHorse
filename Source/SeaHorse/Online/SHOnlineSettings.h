#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "Online/SHSessionTypes.h"
#include "SHOnlineSettings.generated.h"

class AGameModeBase;

UCLASS(Config=Game, DefaultConfig, meta=(DisplayName="SeaHorse Multiplayer"))
class SEAHORSE_API USHOnlineSettings : public UDeveloperSettings
{
	GENERATED_BODY()
public:
	UPROPERTY(Config, EditAnywhere, Category="Maps")
	TSoftObjectPtr<UWorld> MainMenuMap = TSoftObjectPtr<UWorld>(FSoftObjectPath(TEXT("/Game/SeaHorse/Maps/L_MainMenu.L_MainMenu")));
	UPROPERTY(Config, EditAnywhere, Category="Maps")
	TSoftObjectPtr<UWorld> LobbyMap = TSoftObjectPtr<UWorld>(FSoftObjectPath(TEXT("/Game/SeaHorse/Maps/L_Lobby.L_Lobby")));
	// Retains the destination map's World Settings GameMode (including its BP deck configuration).
	UPROPERTY(Config, EditAnywhere, Category="Maps", meta=(DisplayName="Small Match Map (4 seats)"))
	TSoftObjectPtr<UWorld> MatchMap = TSoftObjectPtr<UWorld>(FSoftObjectPath(TEXT("/Game/SeaHorse/Maps/L_Test.L_Test")));
	UPROPERTY(Config, EditAnywhere, Category="Maps", meta=(DisplayName="Medium Match Map (6 seats)"))
	TSoftObjectPtr<UWorld> MediumMatchMap = TSoftObjectPtr<UWorld>(FSoftObjectPath(TEXT("/Game/SeaHorse/Maps/L_Game_SixPlayers.L_Game_SixPlayers")));
	UPROPERTY(Config, EditAnywhere, Category="Maps")
	ESHMatchMap DefaultMatchMap = ESHMatchMap::Small;

	TSoftObjectPtr<UWorld> GetMatchMap(ESHMatchMap Map) const;
	static int32 GetMapSeatCount(ESHMatchMap Map);
	bool BuildMatchURL(ESHMatchMap Map, int32 PlayerCount, FString& URL, FString& Error) const;
	// Optional Blueprint subclass of SHMainMenuGameMode.
	UPROPERTY(Config, EditAnywhere, Category="Menu", meta=(MetaClass="/Script/SeaHorse.SHMainMenuGameMode"))
	TSoftClassPtr<AGameModeBase> MainMenuGameMode;

	// Optional Blueprint subclass of SHLobbyGameMode.
	UPROPERTY(Config, EditAnywhere, Category="Lobby", meta=(MetaClass="/Script/SeaHorse.SHLobbyGameMode"))
	TSoftClassPtr<AGameModeBase> LobbyGameMode;
	UPROPERTY(Config, EditAnywhere, Category="Sessions", meta=(ClampMin="5.0", ClampMax="120.0"))
	float OperationTimeoutSeconds = 30.f;
};
