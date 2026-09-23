#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "Online/SHSessionTypes.h"
#include "SHLobbyGameMode.generated.h"

class ASHLobbyPlayerController;

UCLASS()
class SEAHORSE_API ASHLobbyGameMode : public AGameModeBase
{
	GENERATED_BODY()
public:
	ASHLobbyGameMode();
	virtual void InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage) override;
	virtual void InitGameState() override;
	virtual void PreLogin(const FString& Options, const FString& Address, const FUniqueNetIdRepl& UniqueId, FString& ErrorMessage) override;
	virtual void PostLogin(APlayerController* NewPlayer) override;
	virtual void Logout(AController* Exiting) override;
	void SetPlayerReady(ASHLobbyPlayerController* Player, bool bReady);
	void RequestStartMatch(ASHLobbyPlayerController* Player);
	void RequestSelectMatchMap(ASHLobbyPlayerController* Player, ESHMatchMap Map);
	bool IsStartingRosterValid(int32 ExpectedPlayers) const;
private:
#if WITH_DEV_AUTOMATION_TESTS
	friend class FSHLobbyMapSelectionTest;
#endif
	void FinishMapSelection(ASHLobbyPlayerController* Player, ESHMatchMap Map, bool bSuccess, const FString& Error);
};

// Local menu must not run the card GameMode's hand/deck initialization.
UCLASS()
class SEAHORSE_API ASHMainMenuGameMode : public AGameModeBase
{
	GENERATED_BODY()
public:
	ASHMainMenuGameMode();
};
