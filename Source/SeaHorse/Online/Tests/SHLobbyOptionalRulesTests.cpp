#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Engine/World.h"
#include "Frontend/Lobby/SHLobbyGameMode.h"
#include "Frontend/Lobby/SHLobbyGameState.h"
#include "Frontend/Lobby/SHLobbyPlayerController.h"
#include "Frontend/Lobby/SHLobbyPlayerState.h"
#include "Gameplay/Core/SHGameMode.h"
#include "Gameplay/Core/SHGameState.h"
#include "Gameplay/SHHand.h"
#include "Net/UnrealNetwork.h"
#include "UObject/UnrealType.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSHLobbyOptionalRulesTest, "SeaHorse.Multiplayer.LobbyOptionalRules",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSHLobbyOptionalRulesTest::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	ASHLobbyGameMode* Mode = World->SpawnActor<ASHLobbyGameMode>();
	ASHLobbyGameState* State = World->SpawnActor<ASHLobbyGameState>();
	World->SetGameState(State);
	Mode->GameState = State;
	auto* HostPC = World->SpawnActor<ASHLobbyPlayerController>();
	auto* GuestPC = World->SpawnActor<ASHLobbyPlayerController>();
	auto* Host = World->SpawnActor<ASHLobbyPlayerState>();
	auto* Guest = World->SpawnActor<ASHLobbyPlayerState>();
	HostPC->SetPlayerState(Host);
	GuestPC->SetPlayerState(Guest);
	HostPC->SetAsLocalPlayerController();
	GuestPC->SetAsLocalPlayerController();
	State->AddPlayerState(Host);
	State->AddPlayerState(Guest);
	FSHLobbyInfo Info;
	Info.Host = Host;
	State->SetLobbyInfo(Info);
	Host->SetReady(true);
	Guest->SetReady(true);
	TestFalse(TEXT("Orphan removal defaults to disabled"), Info.OptionalRules.bAllowOrphanedRatfolkRemoval);
	TestTrue(TEXT("Removing the other Paulus defaults to enabled"), Info.OptionalRules.bRemoveOtherPaulusAfterRatfolkPair);
	TestTrue(TEXT("Host can change rules"), HostPC->CanSetOptionalRules());
	TestFalse(TEXT("Guest cannot change rules"), GuestPC->CanSetOptionalRules());
	FSHOptionalRules Changed;
	Changed.bAllowOrphanedRatfolkRemoval = true;
	Changed.bRemoveOtherPaulusAfterRatfolkPair = false;
	Mode->RequestSetOptionalRules(GuestPC, Changed);
	TestTrue(TEXT("Forged guest request preserves defaults"), State->GetLobbyInfo().OptionalRules == FSHOptionalRules());
	TestTrue(TEXT("Rejected request preserves readiness"), Guest->IsReady());
	Mode->RequestSetOptionalRules(HostPC, Changed);
	TestTrue(TEXT("Host choice is stored in lobby state"), State->GetLobbyInfo().OptionalRules == Changed);
	TestTrue(TEXT("Host remains ready"), Host->IsReady());
	TestFalse(TEXT("Guests confirm the new rules before starting"), Guest->IsReady());
	Guest->SetReady(true);
	Mode->RequestSetOptionalRules(HostPC, Changed);
	TestTrue(TEXT("Reapplying the same rules does not reset readiness"), Guest->IsReady());
	for (bool bStarting : {false, true})
	{
		Info = State->GetLobbyInfo();
		Info.bStartingMatch = bStarting;
		Info.bChangingMap = !bStarting;
		State->SetLobbyInfo(Info);
		TestFalse(TEXT("Rules locked during map change and match start"), HostPC->CanSetOptionalRules());
		Mode->RequestSetOptionalRules(HostPC, FSHOptionalRules());
		TestTrue(TEXT("In-flight requests cannot replace the rules snapshot"), State->GetLobbyInfo().OptionalRules == Changed);
	}
	const FProperty* LobbyInfo = FindFProperty<FProperty>(ASHLobbyGameState::StaticClass(), TEXT("Info"));
	TestTrue(TEXT("Lobby rules travel inside replicated lobby info"), LobbyInfo && LobbyInfo->HasAnyPropertyFlags(CPF_Net));
	World->DestroyWorld(false);

	for (bool bRemoveOrphaned : {false, true})
	for (bool bRemoveOther : {false, true})
	{
		FSHOptionalRules Expected;
		Expected.bAllowOrphanedRatfolkRemoval = bRemoveOrphaned;
		Expected.bRemoveOtherPaulusAfterRatfolkPair = bRemoveOther;
		const FString Options = TEXT("?SHExpectedPlayers=2") + Expected.ToTravelOptions();
		World = UWorld::CreateWorld(EWorldType::Game, false);
		ASHGameMode* MatchMode = World->SpawnActor<ASHGameMode>();
		ASHGameState* MatchState = World->SpawnActor<ASHGameState>();
		World->SetGameState(MatchState);
		MatchMode->GameState = MatchState;
		for (int32 Seat = 0; Seat < 4; ++Seat) { World->SpawnActor<ASHHand>()->SetLayoutSeatIndex(Seat); }
		FString Error;
		MatchMode->InitGame(TEXT("OptionalRulesTestMap"), Options, Error);
		TestTrue(TEXT("Authoritative match accepts lobby travel options"), Error.IsEmpty());
		MatchMode->InitGameState();
		TestTrue(TEXT("Both rule toggles survive lobby-to-match initialization"), MatchState->GetOptionalRules() == Expected);
		const FProperty* RulesProperty = FindFProperty<FProperty>(ASHGameState::StaticClass(), TEXT("OptionalRules"));
		TestTrue(TEXT("Match rules are replicated to clients"), RulesProperty && RulesProperty->HasAnyPropertyFlags(CPF_Net));
		World->DestroyWorld(false);
	}
	TestTrue(TEXT("Direct PIE without lobby keeps compatible defaults"), FSHOptionalRules::FromTravelOptions(TEXT("")) == FSHOptionalRules());
	TestTrue(TEXT("Malformed rule options keep defaults"), FSHOptionalRules::FromTravelOptions(TEXT("?SHRemoveOrphanedRatfolk=yes?SHRemoveOtherPaulus=-1")) == FSHOptionalRules());
	return true;
}
#endif
