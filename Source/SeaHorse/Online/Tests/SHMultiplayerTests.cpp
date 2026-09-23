#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "Frontend/Lobby/SHLobbyGameState.h"
#include "Frontend/Lobby/SHLobbyPlayerState.h"
#include "Frontend/Lobby/SHLobbyPlayerController.h"
#include "Frontend/Lobby/SHLobbyGameMode.h"
#include "Online/SHOnlineSettings.h"
#include "Online/SHSessionSubsystem.h"
#include "Misc/PackageName.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSHLobbyReadinessTest, "SeaHorse.Multiplayer.LobbyReadiness",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSHLobbyReadinessTest::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	if (!TestNotNull(TEXT("Test world"), World)) { return false; }
	ASHLobbyGameState* State = World->SpawnActor<ASHLobbyGameState>();
	World->SetGameState(State);
	ASHLobbyPlayerState* Host = World->SpawnActor<ASHLobbyPlayerState>();
	ASHLobbyPlayerState* Guest = World->SpawnActor<ASHLobbyPlayerState>();
	FSHLobbyInfo Info;
	Info.Host = Host;
	Info.MaxPlayers = 2;
	State->SetLobbyInfo(Info);
	State->AddPlayerState(Host);
	State->AddPlayerState(Guest);
	Host->SetReady(true);
	TestFalse(TEXT("Unready guest prevents start"), State->CanStartMatch());
	Guest->SetReady(true);
	TestTrue(TEXT("Two ready players can start"), State->CanStartMatch());
	Info.bStartingMatch = true;
	State->SetLobbyInfo(Info);
	TestFalse(TEXT("Duplicate start is disabled"), State->CanStartMatch());
	Info.bStartingMatch = false;
	State->SetLobbyInfo(Info);
	State->RemovePlayerState(Guest);
	TestFalse(TEXT("Guest departure prevents one-player start"), State->CanStartMatch());
	State->AddPlayerState(Guest);
	State->RemovePlayerState(Host);
	ASHLobbyPlayerState* OtherGuest = World->SpawnActor<ASHLobbyPlayerState>();
	State->AddPlayerState(OtherGuest);
	OtherGuest->SetReady(true);
	TestFalse(TEXT("Remaining guests cannot start without the host"), State->CanStartMatch());
	State->AddPlayerState(Host);
	TestFalse(TEXT("Over-capacity roster cannot start"), State->CanStartMatch());
	World->DestroyWorld(false);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSHMultiplayerConfigurationTest, "SeaHorse.Multiplayer.ConfigurationAndRPCs",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSHMultiplayerConfigurationTest::RunTest(const FString& Parameters)
{
	const auto* Settings = GetDefault<USHOnlineSettings>();
	TestTrue(TEXT("Main menu exists"), FPackageName::DoesPackageExist(Settings->MainMenuMap.GetLongPackageName()));
	TestTrue(TEXT("Lobby exists"), FPackageName::DoesPackageExist(Settings->LobbyMap.GetLongPackageName()));
	TestTrue(TEXT("Match map exists"), FPackageName::DoesPackageExist(Settings->MatchMap.GetLongPackageName()));
	const auto* Lobby = GetDefault<ASHLobbyGameMode>();
	TestEqual(TEXT("Lobby uses its own replicated state"), Lobby->GameStateClass.Get(), ASHLobbyGameState::StaticClass());
	TestEqual(TEXT("Lobby uses player readiness state"), Lobby->PlayerStateClass.Get(), ASHLobbyPlayerState::StaticClass());
	TestNull(TEXT("Lobby does not spawn gameplay pawns"), Lobby->DefaultPawnClass.Get());
	TestTrue(TEXT("Lobby preserves connections during match travel"), Lobby->bUseSeamlessTravel);
	for (FName FunctionName : { FName(TEXT("ServerSetReady")), FName(TEXT("ServerStartMatch")), FName(TEXT("ServerSelectMatchMap")) })
	{
		const UFunction* Function = ASHLobbyPlayerController::StaticClass()->FindFunctionByName(FunctionName);
		if (TestNotNull(TEXT("Blueprint RPC exists"), Function))
		{
			TestTrue(TEXT("Lobby requests run on the server"), Function->HasAllFunctionFlags(FUNC_Net | FUNC_NetServer | FUNC_NetReliable | FUNC_BlueprintCallable));
		}
	}
	for (FName FunctionName : { FName(TEXT("CreateMatch")), FName(TEXT("FindMatches")), FName(TEXT("JoinMatch")), FName(TEXT("LeaveMatch")) })
	{
		const UFunction* Function = USHSessionSubsystem::StaticClass()->FindFunctionByName(FunctionName);
		TestTrue(TEXT("Session API is exposed to Blueprint"), Function && Function->HasAnyFunctionFlags(FUNC_BlueprintCallable));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSHAsyncSearchCompletionTest, "SeaHorse.Multiplayer.AsyncSearchCompletion",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSHAsyncSearchCompletionTest::RunTest(const FString& Parameters)
{
	UGameInstance* Instance = NewObject<UGameInstance>();
	USHSessionSubsystem* Sessions = NewObject<USHSessionSubsystem>(Instance);
	// Model an in-flight service request without advertising a real Steam lobby.
	Sessions->Operation = ESHSessionOperation::Find;
	int32 FirstCalls = 0;
	Sessions->SearchCompletion = [this, &FirstCalls](bool bSuccess, const TArray<FSHSessionResult>& Results, const FString& Error)
	{
		++FirstCalls;
		TestTrue(TEXT("Original search succeeds"), bSuccess);
		TestEqual(TEXT("Original request receives its room"), Results.Num(), 1);
		if (Results.Num() == 1) { TestEqual(TEXT("Room data is preserved"), Results[0].ServerName, FString(TEXT("Test lobby"))); }
		TestTrue(TEXT("Success has no error"), Error.IsEmpty());
	};
	int32 RejectedCalls = 0;
	Sessions->FindMatchesWithCallback(false, 50,
		[this, &RejectedCalls](bool bSuccess, const TArray<FSHSessionResult>& Results, const FString& Error)
		{
			++RejectedCalls;
			TestFalse(TEXT("Overlapping request fails independently"), bSuccess);
			TestTrue(TEXT("Rejected request has no rooms"), Results.IsEmpty());
			TestFalse(TEXT("Rejected request explains the failure"), Error.IsEmpty());
		});
	TestEqual(TEXT("Rejected callback completes once"), RejectedCalls, 1);
	TestEqual(TEXT("First request is still pending"), FirstCalls, 0);
	FSHSessionResult Room;
	Room.ServerName = TEXT("Test lobby");
	Sessions->SearchResults.Add(Room);
	Sessions->Finish(true);
	Sessions->Finish(true);
	TestEqual(TEXT("First callback completes exactly once"), FirstCalls, 1);

	Sessions->Operation = ESHSessionOperation::Find;
	int32 FailedCalls = 0;
	Sessions->SearchCompletion = [this, &FailedCalls](bool bSuccess, const TArray<FSHSessionResult>& Results, const FString& Error)
	{
		++FailedCalls;
		TestFalse(TEXT("Service failure reaches callback"), bSuccess);
		TestTrue(TEXT("Failure cannot return stale rooms"), Results.IsEmpty());
		TestEqual(TEXT("Error reaches callback"), Error, FString(TEXT("Service failure")));
	};
	Sessions->Finish(false, TEXT("Service failure"));
	TestEqual(TEXT("Failure completes once"), FailedCalls, 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSHMatchTravelCompletionTest, "SeaHorse.Multiplayer.MatchTravelCompletion",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSHMatchTravelCompletionTest::RunTest(const FString& Parameters)
{
	UGameInstance* Instance = NewObject<UGameInstance>();
	USHSessionSubsystem* Sessions = NewObject<USHSessionSubsystem>(Instance);
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	UWorld* OtherWorld = UWorld::CreateWorld(EWorldType::Game, false);
	if (!World || !OtherWorld)
	{
		if (World) { World->DestroyWorld(false); }
		if (OtherWorld) { OtherWorld->DestroyWorld(false); }
		AddError(TEXT("Could not create travel test worlds"));
		return false;
	}
	World->SetGameInstance(Instance);
	OtherWorld->SetGameInstance(NewObject<UGameInstance>());
	Sessions->Operation = ESHSessionOperation::Travel;
	Sessions->TravelOrigin = ESHSessionOperation::Start;
	int32 CompletionCount = 0;
	Sessions->StartCompletion = [this, &CompletionCount](bool bSuccess, const FString& Error)
	{
		++CompletionCount;
		TestTrue(TEXT("Ready match completes successfully"), bSuccess);
		TestTrue(TEXT("Ready match has no error"), Error.IsEmpty());
	};
	Sessions->HandlePostLoad(World);
	TestTrue(TEXT("Loading the host map alone does not complete match startup"), Sessions->IsBusy());
	Sessions->NotifyMatchReady(OtherWorld);
	TestEqual(TEXT("Another instance cannot finish our match startup"), CompletionCount, 0);
	Sessions->NotifyMatchReady(World);
	TestFalse(TEXT("Seamless match readiness releases the session operation"), Sessions->IsBusy());
	TestEqual(TEXT("Match start completes once"), CompletionCount, 1);
	Sessions->NotifyMatchReady(World);
	TestEqual(TEXT("Repeated readiness cannot complete twice"), CompletionCount, 1);
	Sessions->Operation = ESHSessionOperation::Travel;
	Sessions->TravelOrigin = ESHSessionOperation::Leave;
	Sessions->NotifyMatchReady(World);
	TestTrue(TEXT("Match readiness cannot complete a menu return"), Sessions->IsBusy());
	Sessions->HandlePostLoad(World);
	TestFalse(TEXT("Ordinary menu travel still completes on map load"), Sessions->IsBusy());
	OtherWorld->DestroyWorld(false);
	World->DestroyWorld(false);
	return true;
}

#endif
