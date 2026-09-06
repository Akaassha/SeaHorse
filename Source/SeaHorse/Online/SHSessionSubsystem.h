#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Interfaces/OnlineSessionInterface.h"
#include "OnlineSessionSettings.h"
#include "Containers/Ticker.h"
#include "Online/SHSessionTypes.h"
#include "SHSessionSubsystem.generated.h"

// Local online-service coordination, persistent across maps. Replicated lobby data lives in GameState.
UCLASS()
class SEAHORSE_API USHSessionSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()
public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	UFUNCTION(BlueprintCallable, Category="SeaHorse|Sessions") void CreateMatch(const FString& ServerName, int32 MaxPlayers = 4, bool bLAN = false);
	UFUNCTION(BlueprintCallable, Category="SeaHorse|Sessions") void FindMatches(bool bLAN = false, int32 MaxResults = 50);
	// Per-request completion used by the async BP node; unrelated rejected requests cannot complete it.
	void FindMatchesWithCallback(bool bLAN, int32 MaxResults,
		TFunction<void(bool, const TArray<FSHSessionResult>&, const FString&)> Completion);
	UFUNCTION(BlueprintCallable, Category="SeaHorse|Sessions") void JoinMatch(const FSHSessionResult& Result);
	UFUNCTION(BlueprintCallable, Category="SeaHorse|Sessions") void LeaveMatch();
	UFUNCTION(BlueprintPure, Category="SeaHorse|Sessions") bool IsBusy() const { return Operation != ESHSessionOperation::None; }
	UFUNCTION(BlueprintPure, Category="SeaHorse|Sessions") ESHSessionOperation GetOperation() const { return Operation; }
	UFUNCTION(BlueprintPure, Category="SeaHorse|Sessions") const TArray<FSHSessionResult>& GetSearchResults() const { return SearchResults; }
	UFUNCTION(BlueprintPure, Category="SeaHorse|Sessions") FString GetLastError() const { return LastError; }
	UFUNCTION(BlueprintPure, Category="SeaHorse|Sessions") bool HasSession() const;
	UFUNCTION(BlueprintPure, Category="SeaHorse|Sessions") FName GetOnlineServiceName() const;
	UPROPERTY(BlueprintAssignable) FSHSessionOperationComplete OnOperationComplete;
	UPROPERTY(BlueprintAssignable) FSHSessionSearchComplete OnSearchComplete;
	UPROPERTY(BlueprintAssignable) FSHSessionStateChanged OnOperationChanged;
	UPROPERTY(BlueprintAssignable) FSHConnectionError OnConnectionError;

	// Server-only C++ entry point; the lobby GameMode validates host and readiness first.
	void StartLobbyMatch(int32 PlayerCount, TFunction<void(bool, const FString&)> Completion);
	// Called by the authoritative match GameMode once every participant has a hand and the match is ready.
	void NotifyMatchReady(UWorld* MatchWorld);
	int32 GetHostedMaxPlayers() const { return HostedMaxPlayers; }
	FString GetHostedServerName() const { return HostedServerName; }
private:
#if WITH_DEV_AUTOMATION_TESTS
	friend class FSHAsyncSearchCompletionTest;
	friend class FSHMatchTravelCompletionTest;
#endif
	bool Prepare(ESHSessionOperation Requested);
	void BeginOperation(ESHSessionOperation Requested);
	void Finish(bool bSuccess, const FString& Error = FString());
	void Reject(ESHSessionOperation Requested, const FString& Error);
	void ClearOnlineDelegates();
	void HandleCreate(FName Name, bool bSuccess);
	void HandleFind(bool bSuccess);
	void HandleJoin(FName Name, EOnJoinSessionCompleteResult::Type Result);
	void HandleDestroy(FName Name, bool bSuccess);
	void HandleStart(FName Name, bool bSuccess);
	void HandlePostLoad(UWorld* World);
	void HandleNetworkFailure(UWorld* World, UNetDriver* Driver, ENetworkFailure::Type Type, const FString& Error);
	void HandleTravelFailure(UWorld* World, ETravelFailure::Type Type, const FString& Error);
	void HandleConnectionFailure(const FString& Error);
	void ReturnToMenu();
	void TravelTo(const FString& URL, bool bServer);
	bool HandleTimeout(float DeltaTime);
	int32 GetLocalUserNumber() const;
	IOnlineSessionPtr Sessions;
	TSharedPtr<FOnlineSessionSearch> Search;
	TMap<FGuid, FOnlineSessionSearchResult> JoinableResults;
	UPROPERTY(Transient) TArray<FSHSessionResult> SearchResults;
	ESHSessionOperation Operation = ESHSessionOperation::None;
	ESHSessionOperation TravelOrigin = ESHSessionOperation::None;
	FString LastError;
	FString HostedServerName;
	FString PendingMatchURL;
	int32 HostedMaxPlayers = 4;
	int32 StartingPlayerCount = 0;
	bool bHandlingFailure = false;
	TFunction<void(bool, const FString&)> StartCompletion;
	TFunction<void(bool, const TArray<FSHSessionResult>&, const FString&)> SearchCompletion;
	FDelegateHandle CreateHandle, FindHandle, JoinHandle, DestroyHandle, StartHandle;
	FDelegateHandle NetworkFailureHandle, TravelFailureHandle, PostLoadHandle;
	FTSTicker::FDelegateHandle TimeoutHandle;
};
