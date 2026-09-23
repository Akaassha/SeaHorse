#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "Frontend/Lobby/SHLobbyGameMode.h"
#include "Frontend/Lobby/SHLobbyGameState.h"
#include "Frontend/Lobby/SHLobbyPlayerState.h"
#include "Frontend/Lobby/SHLobbyPlayerController.h"
#include "Online/SHOnlineSettings.h"
#include "Online/SHSessionSubsystem.h"
#include "Online/OnlineSessionNames.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSHLobbyMapSelectionTest, "SeaHorse.Multiplayer.LobbyMapSelection",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSHLobbyMapSelectionTest::RunTest(const FString& Parameters)
{
	const auto* Settings = GetDefault<USHOnlineSettings>();
	FString URL, Error;
	TestTrue(TEXT("Small map accepts four humans"), Settings->BuildMatchURL(ESHMatchMap::Small, 4, URL, Error));
	TestEqual(TEXT("Small travel uses configured map and roster"), URL, Settings->MatchMap.GetLongPackageName() + TEXT("?SHExpectedPlayers=4"));
	TestFalse(TEXT("Small map refuses five humans"), Settings->BuildMatchURL(ESHMatchMap::Small, 5, URL, Error));
	TestTrue(TEXT("Rejected travel has no URL"), URL.IsEmpty());
	TestTrue(TEXT("Medium map accepts six humans"), Settings->BuildMatchURL(ESHMatchMap::Medium, 6, URL, Error));
	TestEqual(TEXT("Medium travel uses configured map and roster"), URL, Settings->MediumMatchMap.GetLongPackageName() + TEXT("?SHExpectedPlayers=6"));
	TestTrue(TEXT("Medium map also accepts two humans plus BN seats"), Settings->BuildMatchURL(ESHMatchMap::Medium, 2, URL, Error));
	TestFalse(TEXT("Unknown map ID is rejected"), Settings->BuildMatchURL(static_cast<ESHMatchMap>(255), 2, URL, Error));
	TestFalse(TEXT("One-player start rejected"), Settings->BuildMatchURL(ESHMatchMap::Small, 1, URL, Error));
	auto* BrokenSettings = NewObject<USHOnlineSettings>();
	BrokenSettings->MediumMatchMap = TSoftObjectPtr<UWorld>(FSoftObjectPath(TEXT("/Game/MissingLobbyTestMap.MissingLobbyTestMap")));
	TestFalse(TEXT("Missing configured map rejected before travel"), BrokenSettings->BuildMatchURL(ESHMatchMap::Medium, 2, URL, Error));

	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	ASHLobbyGameMode* Mode = World->SpawnActor<ASHLobbyGameMode>();
	ASHLobbyGameState* State = World->SpawnActor<ASHLobbyGameState>();
	World->SetGameState(State);
	Mode->GameState = State;
	ASHLobbyPlayerController* HostPC = World->SpawnActor<ASHLobbyPlayerController>();
	ASHLobbyPlayerController* GuestPC = World->SpawnActor<ASHLobbyPlayerController>();
	ASHLobbyPlayerState* Host = World->SpawnActor<ASHLobbyPlayerState>();
	ASHLobbyPlayerState* Guest = World->SpawnActor<ASHLobbyPlayerState>();
	HostPC->SetPlayerState(Host);
	GuestPC->SetPlayerState(Guest);
	HostPC->SetAsLocalPlayerController();
	GuestPC->SetAsLocalPlayerController(); // Even a local non-host must not be authorized.
	State->AddPlayerState(Host);
	State->AddPlayerState(Guest);
	FSHLobbyInfo Info;
	Info.Host = Host;
	State->SetLobbyInfo(Info);
	Host->SetReady(true);
	Guest->SetReady(true);
	TestTrue(TEXT("Host can select medium map"), HostPC->CanSelectMatchMap(ESHMatchMap::Medium));
	TestFalse(TEXT("Guest cannot select map"), GuestPC->CanSelectMatchMap(ESHMatchMap::Medium));
	Mode->RequestSelectMatchMap(GuestPC, ESHMatchMap::Medium);
	TestEqual(TEXT("Unauthorized request does not change map"), State->GetLobbyInfo().SelectedMap, ESHMatchMap::Small);
	TestTrue(TEXT("Unauthorized request does not reset ready"), Guest->IsReady());
	Mode->RequestSelectMatchMap(HostPC, ESHMatchMap::Small);
	TestTrue(TEXT("Re-selecting current map does not reset ready"), Guest->IsReady());
	Info.bChangingMap = true;
	State->SetLobbyInfo(Info);
	TestFalse(TEXT("Pending map change blocks start"), State->CanStartMatch());
	TestFalse(TEXT("Pending map change blocks another choice"), HostPC->CanSelectMatchMap(ESHMatchMap::Medium));
	Mode->SetPlayerReady(GuestPC, false);
	TestTrue(TEXT("Readiness cannot change during session update"), Guest->IsReady());
	Mode->FinishMapSelection(HostPC, ESHMatchMap::Medium, false, TEXT("Simulated service failure"));
	TestEqual(TEXT("Failure preserves previous map"), State->GetLobbyInfo().SelectedMap, ESHMatchMap::Small);
	TestEqual(TEXT("Failure preserves previous capacity"), State->GetLobbyInfo().MaxPlayers, 4);
	TestTrue(TEXT("Failure preserves readiness and unlocks start"), State->CanStartMatch());
	State->SetLobbyInfo(Info);
	Mode->FinishMapSelection(HostPC, ESHMatchMap::Medium, true, FString());
	TestEqual(TEXT("Confirmed choice stored in replicated lobby info"), State->GetLobbyInfo().SelectedMap, ESHMatchMap::Medium);
	TestEqual(TEXT("Confirmed medium map allows six humans"), State->GetLobbyInfo().MaxPlayers, 6);
	TestTrue(TEXT("Host remains ready"), Host->IsReady());
	TestFalse(TEXT("Guest must confirm readiness again"), Guest->IsReady());
	Guest->SetReady(true);
	TestTrue(TEXT("Ready guests can start selected map"), State->CanStartMatch());
	TArray<ASHLobbyPlayerState*> ExtraGuests;
	for (int32 Index = 0; Index < 4; ++Index)
	{
		auto* Additional = World->SpawnActor<ASHLobbyPlayerState>();
		State->AddPlayerState(Additional);
		ExtraGuests.Add(Additional);
		Additional->SetReady(true);
	}
	TestTrue(TEXT("Six ready humans can start medium map"), State->CanStartMatch());
	TestTrue(TEXT("Six humans may choose small map"), HostPC->CanSelectMatchMap(ESHMatchMap::Small));
	Info = State->GetLobbyInfo();
	Info.bChangingMap = true;
	State->SetLobbyInfo(Info);
	Mode->FinishMapSelection(HostPC, ESHMatchMap::Small, true, FString());
	TestEqual(TEXT("Small map is selected even with six humans"), State->GetLobbyInfo().SelectedMap, ESHMatchMap::Small);
	TestEqual(TEXT("Existing lobby capacity is preserved"), State->GetLobbyInfo().MaxPlayers, 6);
	for (ASHLobbyPlayerState* Player : State->GetLobbyPlayers()) { Player->SetReady(true); }
	TestFalse(TEXT("Small map blocks start with six ready humans"), State->CanStartMatch());
	State->RemovePlayerState(ExtraGuests[0]);
	State->RemovePlayerState(ExtraGuests[1]);
	TestTrue(TEXT("Small map can start after roster drops to four"), State->CanStartMatch());
	Info = State->GetLobbyInfo();
	Info.bStartingMatch = true;
	State->SetLobbyInfo(Info);
	TestFalse(TEXT("Starting match freezes map choice"), HostPC->CanSelectMatchMap(ESHMatchMap::Medium));
	World->DestroyWorld(false);

	// Exercise asynchronous service completion, including failures and late callbacks.
	UGameInstance* Instance = NewObject<UGameInstance>();
	USHSessionSubsystem* Sessions = NewObject<USHSessionSubsystem>(Instance);
	int32 Completions = 0;
	Sessions->Operation = ESHSessionOperation::UpdateLobbyMap;
	Sessions->PendingLobbyMap = ESHMatchMap::Medium;
	Sessions->MapChangeCompletion = [this, &Completions](bool bSuccess, const FString& Failure)
	{
		++Completions;
		TestTrue(TEXT("Successful service callback"), bSuccess);
	};
	Sessions->HandleUpdateLobbyMap(NAME_GameSession, true);
	TestEqual(TEXT("Session now has six public slots"), Sessions->GetHostedMaxPlayers(), 6);
	TestEqual(TEXT("Session remembers confirmed map for travel"), Sessions->GetHostedMatchMap(), ESHMatchMap::Medium);
	TestFalse(TEXT("Successful update releases busy state"), Sessions->IsBusy());
	Sessions->HandleUpdateLobbyMap(NAME_GameSession, true);
	TestEqual(TEXT("Late callback ignored after completion"), Completions, 1);
	Sessions->Operation = ESHSessionOperation::UpdateLobbyMap;
	Sessions->PendingLobbyMap = ESHMatchMap::Small;
	Sessions->MapChangeCompletion = [this, &Completions](bool bSuccess, const FString& Failure)
	{
		++Completions;
		TestFalse(TEXT("Service failure propagated"), bSuccess);
	};
	Sessions->HandleUpdateLobbyMap(NAME_GameSession, false);
	TestEqual(TEXT("Failed update preserves confirmed session limit"), Sessions->GetHostedMaxPlayers(), 6);
	TestEqual(TEXT("Failed update preserves confirmed session map"), Sessions->GetHostedMatchMap(), ESHMatchMap::Medium);
	TestEqual(TEXT("Each request completes exactly once"), Completions, 2);
	Sessions->Operation = ESHSessionOperation::UpdateLobbyMap;
	Sessions->MapChangeCompletion = [this](bool bSuccess, const FString& Failure)
	{
		TestFalse(TEXT("Timeout propagates failure"), bSuccess);
	};
	Sessions->HandleTimeout(0.f);
	TestFalse(TEXT("Timeout releases map-selection operation without leaving lobby"), Sessions->IsBusy());
	return true;
}
#endif
