#include "Online/SHSessionSubsystem.h"
#include "Online/SHOnlineSettings.h"
#include "Frontend/Lobby/SHLobbyGameMode.h"
#include "OnlineSubsystem.h"
#include "OnlineSubsystemUtils.h"
#include "Online/OnlineSessionNames.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/PackageName.h"

namespace
{
	const FName GameKey(TEXT("SH_GAME"));
	const FName ServerKey(TEXT("SH_SERVER"));
	const FString GameVersion(TEXT("SeaHorse_1"));
	FString MainMenuOptions()
	{
		const auto* Settings = GetDefault<USHOnlineSettings>();
		UClass* Mode = Settings->MainMenuGameMode.LoadSynchronous();
		if (!Mode || !Mode->IsChildOf(ASHMainMenuGameMode::StaticClass())) { Mode = ASHMainMenuGameMode::StaticClass(); }
		return TEXT("game=") + Mode->GetPathName();
	}
	bool IsAvailableMap(const TSoftObjectPtr<UWorld>& Map)
	{
		return !Map.IsNull() && FPackageName::DoesPackageExist(Map.GetLongPackageName());
	}
}

void USHSessionSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	if (GEngine)
	{
		NetworkFailureHandle = GEngine->OnNetworkFailure().AddUObject(this, &ThisClass::HandleNetworkFailure);
		TravelFailureHandle = GEngine->OnTravelFailure().AddUObject(this, &ThisClass::HandleTravelFailure);
	}
	PostLoadHandle = FCoreUObjectDelegates::PostLoadMapWithWorld.AddUObject(this, &ThisClass::HandlePostLoad);
}

void USHSessionSubsystem::Deinitialize()
{
	ClearOnlineDelegates();
	FTSTicker::GetCoreTicker().RemoveTicker(TimeoutHandle);
	FCoreUObjectDelegates::PostLoadMapWithWorld.Remove(PostLoadHandle);
	if (GEngine)
	{
		GEngine->OnNetworkFailure().Remove(NetworkFailureHandle);
		GEngine->OnTravelFailure().Remove(TravelFailureHandle);
	}
	StartCompletion = nullptr;
	auto PendingSearchCompletion = MoveTemp(SearchCompletion);
	SearchCompletion = nullptr;
	if (PendingSearchCompletion) { PendingSearchCompletion(false, {}, TEXT("The game instance is shutting down.")); }
	Search.Reset();
	Sessions.Reset();
	Super::Deinitialize();
}

int32 USHSessionSubsystem::GetLocalUserNumber() const
{
	const ULocalPlayer* Player = GetGameInstance()->GetFirstGamePlayer();
	return Player ? Player->GetControllerId() : 0;
}

FName USHSessionSubsystem::GetOnlineServiceName() const
{
	const IOnlineSubsystem* OnlineService = Online::GetSubsystem(GetWorld());
	return OnlineService ? OnlineService->GetSubsystemName() : NAME_None;
}

bool USHSessionSubsystem::HasSession() const
{
	IOnlineSessionPtr Interface = Sessions.IsValid() ? Sessions : Online::GetSessionInterface(GetWorld());
	return Interface.IsValid() && Interface->GetNamedSession(NAME_GameSession) != nullptr;
}

void USHSessionSubsystem::Reject(ESHSessionOperation Requested, const FString& Error)
{
	LastError = Error;
	OnOperationComplete.Broadcast(Requested, false, Error);
}

bool USHSessionSubsystem::Prepare(ESHSessionOperation Requested)
{
	if (IsBusy()) { Reject(Requested, TEXT("Another session operation is in progress.")); return false; }
	Sessions = Online::GetSessionInterface(GetWorld());
	if (!Sessions.IsValid()) { Reject(Requested, TEXT("Online service is unavailable.")); return false; }
	if (!GetGameInstance()->GetFirstLocalPlayerController())
	{
		Reject(Requested, TEXT("A local player controller is required."));
		return false;
	}
	return true;
}

void USHSessionSubsystem::BeginOperation(ESHSessionOperation Requested)
{
	Operation = Requested;
	LastError.Empty();
	FTSTicker::GetCoreTicker().RemoveTicker(TimeoutHandle);
	TimeoutHandle = FTSTicker::GetCoreTicker().AddTicker(
		FTickerDelegate::CreateUObject(this, &ThisClass::HandleTimeout),
		FMath::Clamp(GetDefault<USHOnlineSettings>()->OperationTimeoutSeconds, 5.f, 120.f));
	OnOperationChanged.Broadcast(Operation);
}

void USHSessionSubsystem::ClearOnlineDelegates()
{
	if (!Sessions.IsValid()) { return; }
	Sessions->ClearOnCreateSessionCompleteDelegate_Handle(CreateHandle);
	Sessions->ClearOnFindSessionsCompleteDelegate_Handle(FindHandle);
	Sessions->ClearOnJoinSessionCompleteDelegate_Handle(JoinHandle);
	Sessions->ClearOnDestroySessionCompleteDelegate_Handle(DestroyHandle);
	Sessions->ClearOnStartSessionCompleteDelegate_Handle(StartHandle);
	CreateHandle.Reset(); FindHandle.Reset(); JoinHandle.Reset(); DestroyHandle.Reset(); StartHandle.Reset();
}

void USHSessionSubsystem::Finish(bool bSuccess, const FString& Error)
{
	const ESHSessionOperation Completed = Operation == ESHSessionOperation::Travel ? TravelOrigin : Operation;
	ClearOnlineDelegates();
	FTSTicker::GetCoreTicker().RemoveTicker(TimeoutHandle);
	TimeoutHandle.Reset();
	Operation = ESHSessionOperation::None;
	TravelOrigin = ESHSessionOperation::None;
	LastError = Error;
	auto Completion = MoveTemp(StartCompletion);
	StartCompletion = nullptr;
	auto CompletedSearch = Completed == ESHSessionOperation::Find ? MoveTemp(SearchCompletion) : nullptr;
	if (Completed == ESHSessionOperation::Find) { SearchCompletion = nullptr; }
	const TArray<FSHSessionResult> CompletedResults = bSuccess && Completed == ESHSessionOperation::Find ? SearchResults : TArray<FSHSessionResult>();
	OnOperationChanged.Broadcast(Operation);
	if (Completion) { Completion(bSuccess, Error); }
	if (CompletedSearch) { CompletedSearch(bSuccess, CompletedResults, Error); }
	if (Completed != ESHSessionOperation::None) { OnOperationComplete.Broadcast(Completed, bSuccess, Error); }
}

void USHSessionSubsystem::CreateMatch(const FString& ServerName, int32 MaxPlayers, bool bLAN)
{
	if (!Prepare(ESHSessionOperation::Create)) { return; }
	const FString Name = ServerName.TrimStartAndEnd();
	if (Name.IsEmpty() || Name.Len() > 64 || MaxPlayers < 2 || MaxPlayers > 4)
	{
		Reject(ESHSessionOperation::Create, TEXT("Use a server name of 1-64 characters and 2-4 player slots.")); return;
	}
	if (HasSession() || GetWorld()->GetNetMode() != NM_Standalone)
	{
		Reject(ESHSessionOperation::Create, TEXT("Leave the current match before hosting another.")); return;
	}
	const auto* Settings = GetDefault<USHOnlineSettings>();
	if (!IsAvailableMap(Settings->LobbyMap)) { Reject(ESHSessionOperation::Create, TEXT("Lobby map is missing.")); return; }
	if (!Settings->LobbyGameMode.IsNull())
	{
		UClass* Mode = Settings->LobbyGameMode.LoadSynchronous();
		if (!Mode || !Mode->IsChildOf(ASHLobbyGameMode::StaticClass()))
		{
			Reject(ESHSessionOperation::Create, TEXT("Lobby GameMode must inherit from SHLobbyGameMode.")); return;
		}
	}
	HostedServerName = Name;
	HostedMaxPlayers = MaxPlayers;
	FOnlineSessionSettings SessionSettings;
	SessionSettings.NumPublicConnections = MaxPlayers;
	SessionSettings.bIsLANMatch = bLAN;
	SessionSettings.bShouldAdvertise = true;
	SessionSettings.bAllowJoinInProgress = false;
	SessionSettings.bAllowInvites = false; // Invite acceptance is not part of this API.
	SessionSettings.bUsesPresence = !bLAN;
	SessionSettings.bAllowJoinViaPresence = !bLAN;
	SessionSettings.bUseLobbiesIfAvailable = !bLAN;
	SessionSettings.Set(GameKey, GameVersion, EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);
	SessionSettings.Set(ServerKey, Name, EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);
	SessionSettings.Set(SETTING_MAPNAME, Settings->LobbyMap.GetLongPackageName(), EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);
	BeginOperation(ESHSessionOperation::Create);
	CreateHandle = Sessions->AddOnCreateSessionCompleteDelegate_Handle(FOnCreateSessionCompleteDelegate::CreateUObject(this, &ThisClass::HandleCreate));
	if (!Sessions->CreateSession(GetLocalUserNumber(), NAME_GameSession, SessionSettings) && Operation == ESHSessionOperation::Create)
	{
		Finish(false, TEXT("Online service refused to create the session."));
	}
}

void USHSessionSubsystem::HandleCreate(FName Name, bool bSuccess)
{
	if (Name != NAME_GameSession || Operation != ESHSessionOperation::Create) { return; }
	if (!bSuccess) { Finish(false, TEXT("Session creation failed. Check the online service login.")); return; }
	const auto* Settings = GetDefault<USHOnlineSettings>();
	const FString Mode = Settings->LobbyGameMode.IsNull() ? ASHLobbyGameMode::StaticClass()->GetPathName() : Settings->LobbyGameMode.ToSoftObjectPath().ToString();
	const FString URL = Settings->LobbyMap.GetLongPackageName() + TEXT("?listen?game=") + Mode;
	TravelTo(URL, false);
}

void USHSessionSubsystem::FindMatches(bool bLAN, int32 MaxResults)
{
	FindMatchesWithCallback(bLAN, MaxResults, nullptr);
}

void USHSessionSubsystem::FindMatchesWithCallback(bool bLAN, int32 MaxResults,
	TFunction<void(bool, const TArray<FSHSessionResult>&, const FString&)> Completion)
{
	if (!Prepare(ESHSessionOperation::Find))
	{
		if (Completion) { Completion(false, {}, LastError); }
		return;
	}
	SearchCompletion = MoveTemp(Completion);
	JoinableResults.Reset(); SearchResults.Reset();
	Search = MakeShared<FOnlineSessionSearch>();
	Search->bIsLanQuery = bLAN;
	Search->MaxSearchResults = FMath::Clamp(MaxResults, 1, 200);
	Search->QuerySettings.Set(GameKey, GameVersion, EOnlineComparisonOp::Equals);
	if (!bLAN) { Search->QuerySettings.Set(SEARCH_LOBBIES, true, EOnlineComparisonOp::Equals); }
	BeginOperation(ESHSessionOperation::Find);
	FindHandle = Sessions->AddOnFindSessionsCompleteDelegate_Handle(FOnFindSessionsCompleteDelegate::CreateUObject(this, &ThisClass::HandleFind));
	if (!Sessions->FindSessions(GetLocalUserNumber(), Search.ToSharedRef()) && Operation == ESHSessionOperation::Find) { HandleFind(false); }
}

void USHSessionSubsystem::HandleFind(bool bSuccess)
{
	if (Operation != ESHSessionOperation::Find) { return; }
	if (bSuccess && Search)
	{
		for (const auto& Found : Search->SearchResults)
		{
			FString Game;
			Found.Session.SessionSettings.Get(GameKey, Game);
			if (!Found.IsValid() || Game != GameVersion || Found.Session.NumOpenPublicConnections <= 0) { continue; }
			FSHSessionResult Result;
			Result.ResultId = FGuid::NewGuid();
			Found.Session.SessionSettings.Get(ServerKey, Result.ServerName);
			Result.HostName = Found.Session.OwningUserName;
			Result.MaxPlayers = Found.Session.SessionSettings.NumPublicConnections;
			Result.CurrentPlayers = FMath::Clamp(Result.MaxPlayers - Found.Session.NumOpenPublicConnections, 0, Result.MaxPlayers);
			Result.PingMilliseconds = Found.PingInMs;
			Result.bIsLAN = Found.Session.SessionSettings.bIsLANMatch;
			SearchResults.Add(Result);
			JoinableResults.Add(Result.ResultId, Found);
		}
	}
	const TArray<FSHSessionResult> CompletedResults = SearchResults;
	Finish(bSuccess, bSuccess ? FString() : TEXT("Session search failed."));
	OnSearchComplete.Broadcast(bSuccess, CompletedResults);
}

void USHSessionSubsystem::JoinMatch(const FSHSessionResult& Result)
{
	if (!Prepare(ESHSessionOperation::Join)) { return; }
	if (HasSession() || GetWorld()->GetNetMode() != NM_Standalone)
	{
		Reject(ESHSessionOperation::Join, TEXT("Leave the current match before joining another.")); return;
	}
	const FOnlineSessionSearchResult* Found = JoinableResults.Find(Result.ResultId);
	if (!Found || !Found->IsValid()) { Reject(ESHSessionOperation::Join, TEXT("This search result expired. Refresh the list.")); return; }
	BeginOperation(ESHSessionOperation::Join);
	JoinHandle = Sessions->AddOnJoinSessionCompleteDelegate_Handle(FOnJoinSessionCompleteDelegate::CreateUObject(this, &ThisClass::HandleJoin));
	if (!Sessions->JoinSession(GetLocalUserNumber(), NAME_GameSession, *Found) && Operation == ESHSessionOperation::Join)
	{
		Finish(false, TEXT("Online service refused to join the session."));
	}
}

void USHSessionSubsystem::HandleJoin(FName Name, EOnJoinSessionCompleteResult::Type Result)
{
	if (Name != NAME_GameSession || Operation != ESHSessionOperation::Join) { return; }
	if (Result != EOnJoinSessionCompleteResult::Success)
	{
		const FString Error = Result == EOnJoinSessionCompleteResult::SessionIsFull ? TEXT("The lobby is full.") :
			Result == EOnJoinSessionCompleteResult::SessionDoesNotExist ? TEXT("The lobby no longer exists.") : TEXT("Unable to join the lobby.");
		Finish(false, Error); return;
	}
	FString Address;
	if (!Sessions->GetResolvedConnectString(NAME_GameSession, Address) || Address.IsEmpty())
	{
		HandleConnectionFailure(TEXT("The session has no valid connection address.")); return;
	}
	TravelTo(Address, false);
}

void USHSessionSubsystem::TravelTo(const FString& URL, bool bServer)
{
	TravelOrigin = Operation;
	ClearOnlineDelegates();
	BeginOperation(ESHSessionOperation::Travel);
	if (bServer)
	{
		if (!GetWorld()->ServerTravel(URL, true)) { HandleConnectionFailure(TEXT("Server travel was rejected.")); }
	}
	else
	{
		GetGameInstance()->GetFirstLocalPlayerController()->ClientTravel(URL, TRAVEL_Absolute);
	}
}

void USHSessionSubsystem::HandlePostLoad(UWorld* World)
{
	if (World && World->GetGameInstance() == GetGameInstance() && Operation == ESHSessionOperation::Travel &&
		TravelOrigin != ESHSessionOperation::Start)
	{
		Finish(true);
	}
}

void USHSessionSubsystem::NotifyMatchReady(UWorld* MatchWorld)
{
	// Seamless travel does not emit PostLoadMapWithWorld. Do not leave the start timeout armed,
	// and do not report success merely because the host loaded before the other participants.
	if (MatchWorld && MatchWorld->GetGameInstance() == GetGameInstance() &&
		MatchWorld->GetNetMode() != NM_Client && Operation == ESHSessionOperation::Travel &&
		TravelOrigin == ESHSessionOperation::Start)
	{
		UE_LOG(LogTemp, Log, TEXT("[SH_SESSION] Match ready after travel: %s"), *MatchWorld->GetMapName());
		Finish(true);
	}
}

void USHSessionSubsystem::LeaveMatch()
{
	if (IsBusy()) { Reject(ESHSessionOperation::Leave, TEXT("Wait for the current session operation to finish.")); return; }
	Sessions = Online::GetSessionInterface(GetWorld());
	BeginOperation(ESHSessionOperation::Leave);
	if (!HasSession()) { ReturnToMenu(); return; }
	DestroyHandle = Sessions->AddOnDestroySessionCompleteDelegate_Handle(FOnDestroySessionCompleteDelegate::CreateUObject(this, &ThisClass::HandleDestroy));
	if (!Sessions->DestroySession(NAME_GameSession) && Operation == ESHSessionOperation::Leave)
	{
		HandleDestroy(NAME_GameSession, false);
	}
}

void USHSessionSubsystem::HandleDestroy(FName Name, bool bSuccess)
{
	if (Name != NAME_GameSession || Operation != ESHSessionOperation::Leave) { return; }
	if (!bSuccess)
	{
		// Still disconnect locally; retain the error so callers can retry service cleanup.
		Finish(false, TEXT("Online session cleanup failed. Retry LeaveMatch before hosting again."));
		const auto* Settings = GetDefault<USHOnlineSettings>();
		UGameplayStatics::OpenLevel(GetGameInstance(), FName(*Settings->MainMenuMap.GetLongPackageName()), true, MainMenuOptions());
		return;
	}
	ReturnToMenu();
}

void USHSessionSubsystem::ReturnToMenu()
{
	JoinableResults.Reset(); SearchResults.Reset(); Search.Reset();
	const auto* Settings = GetDefault<USHOnlineSettings>();
	if (!IsAvailableMap(Settings->MainMenuMap)) { Finish(false, TEXT("Main menu map is missing.")); return; }
	TravelOrigin = ESHSessionOperation::Leave;
	ClearOnlineDelegates();
	BeginOperation(ESHSessionOperation::Travel);
	UGameplayStatics::OpenLevel(GetGameInstance(), FName(*Settings->MainMenuMap.GetLongPackageName()), true, MainMenuOptions());
}

void USHSessionSubsystem::StartLobbyMatch(int32 PlayerCount, TFunction<void(bool, const FString&)> Completion)
{
	if (IsBusy() || !Sessions.IsValid() || !HasSession() || GetWorld()->GetNetMode() != NM_ListenServer || PlayerCount < 2 || PlayerCount > 4)
	{
		Completion(false, TEXT("A hosted session with 2-4 players is required.")); return;
	}
	const auto* Settings = GetDefault<USHOnlineSettings>();
	if (!IsAvailableMap(Settings->MatchMap)) { Completion(false, TEXT("Match map is missing.")); return; }
	StartingPlayerCount = PlayerCount;
	PendingMatchURL = Settings->MatchMap.GetLongPackageName() + FString::Printf(TEXT("?SHExpectedPlayers=%d"), PlayerCount);
	StartCompletion = MoveTemp(Completion);
	BeginOperation(ESHSessionOperation::Start);
	StartHandle = Sessions->AddOnStartSessionCompleteDelegate_Handle(FOnStartSessionCompleteDelegate::CreateUObject(this, &ThisClass::HandleStart));
	if (!Sessions->StartSession(NAME_GameSession) && Operation == ESHSessionOperation::Start) { HandleStart(NAME_GameSession, false); }
}

void USHSessionSubsystem::HandleStart(FName Name, bool bSuccess)
{
	if (Name != NAME_GameSession || Operation != ESHSessionOperation::Start) { return; }
	if (!bSuccess) { Finish(false, TEXT("Online service could not start the match.")); return; }
	const auto* Lobby = GetWorld()->GetAuthGameMode<ASHLobbyGameMode>();
	if (!Lobby || !Lobby->IsStartingRosterValid(StartingPlayerCount))
	{
		// The backend has already started; disconnect rather than strand a partial roster in a match.
		HandleConnectionFailure(TEXT("The lobby roster changed while the match was starting. Please create a new lobby."));
		return;
	}
	// Close advertising as well as rejecting late arrivals in the authoritative GameMode.
	if (FOnlineSessionSettings* Settings = Sessions->GetSessionSettings(NAME_GameSession))
	{
		Settings->bShouldAdvertise = false;
		Settings->bAllowJoinViaPresence = false;
		Sessions->UpdateSession(NAME_GameSession, *Settings, true);
	}
	TravelTo(PendingMatchURL, true);
}

bool USHSessionSubsystem::HandleTimeout(float DeltaTime)
{
	if (Operation == ESHSessionOperation::Find)
	{
		Sessions->CancelFindSessions();
		HandleFind(false);
	}
	else if (Operation == ESHSessionOperation::Leave || (Operation == ESHSessionOperation::Travel && TravelOrigin == ESHSessionOperation::Leave))
	{
		Finish(false, TEXT("Leaving the session timed out."));
		UGameplayStatics::OpenLevel(GetGameInstance(), FName(*GetDefault<USHOnlineSettings>()->MainMenuMap.GetLongPackageName()), true, MainMenuOptions());
	}
	else { HandleConnectionFailure(TEXT("The session operation timed out.")); }
	return false;
}

void USHSessionSubsystem::HandleNetworkFailure(UWorld* World, UNetDriver* Driver, ENetworkFailure::Type Type, const FString& Error)
{
	UE_LOG(LogTemp, Warning, TEXT("[SH_SESSION] Network failure type=%d world=%s: %s"),
		static_cast<int32>(Type), *GetNameSafe(World), *Error);
	if (World && World->GetGameInstance() == GetGameInstance()) { HandleConnectionFailure(Error); }
}

void USHSessionSubsystem::HandleTravelFailure(UWorld* World, ETravelFailure::Type Type, const FString& Error)
{
	UE_LOG(LogTemp, Warning, TEXT("[SH_SESSION] Travel failure type=%d world=%s: %s"),
		static_cast<int32>(Type), *GetNameSafe(World), *Error);
	if (World && World->GetGameInstance() == GetGameInstance()) { HandleConnectionFailure(Error); }
}

void USHSessionSubsystem::HandleConnectionFailure(const FString& Error)
{
	if (bHandlingFailure) { return; }
	TGuardValue<bool> Guard(bHandlingFailure, true);
	const bool bWasLeaving = Operation == ESHSessionOperation::Leave || TravelOrigin == ESHSessionOperation::Leave;
	Finish(false, Error);
	OnConnectionError.Broadcast(Error);
	if (!bWasLeaving && !IsBusy()) { LeaveMatch(); }
}
