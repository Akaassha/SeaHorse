#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Engine/World.h"
#include "Gameplay/Core/SHGameState.h"
#include "Gameplay/Core/SHPlayerState.h"
#include "Gameplay/Core/SHPlayerController.h"
#include "Gameplay/SHHand.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSHLocalMatchUIReadinessTest, "SeaHorse.Gameplay.UI.DelayedPlayerState",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSHLocalMatchUIReadinessTest::RunTest(const FString& Parameters)
{
    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
    ASHPlayerController* PC = World->SpawnActor<ASHPlayerController>();
    PC->SetAsLocalPlayerController();
    TestTrue(TEXT("Test controller is local"), PC->IsLocalController());
    PC->TrySetupTableView();
    TestFalse(TEXT("UI waits for GameState"), PC->bLocalMatchUIInitialized);

    ASHGameState* State = World->SpawnActor<ASHGameState>();
    World->SetGameState(State);
    TArray<ASHHand*> Hands;
    TArray<ASHPlayerState*> Players;
    for (int32 Seat = 0; Seat < 4; ++Seat)
    {
        ASHHand* Hand = World->SpawnActor<ASHHand>();
        Hand->SetLayoutSeatIndex(Seat);
        ASHPlayerState* Player = World->SpawnActor<ASHPlayerState>();
        Player->SetSeatIndex(Seat);
        Player->SetHand(Hand);
        State->AddPlayerState(Player);
        Hands.Add(Hand);
        Players.Add(Player);
    }
    State->SetParticipantHands(Hands);
    State->SetMatchReady(true);
    PC->TrySetupTableView();
    TestFalse(TEXT("Ready match still waits for controller PlayerState"), PC->bLocalMatchUIInitialized);

    // Model the controller reference arriving after PlayerState BeginPlay.
    PC->PlayerState = Players[0];
    Players[3]->SetHand(nullptr);
    PC->TrySetupTableView();
    TestFalse(TEXT("UI waits for the remaining replicated hand assignment"), PC->bLocalMatchUIInitialized);
    Players[3]->SetHand(Hands[3]);
    PC->TrySetupTableView();
    TestTrue(TEXT("A later retry initializes UI after references arrive"), PC->bLocalMatchUIInitialized);
    TestTrue(TEXT("Table is initialized before UI"), PC->bTableViewInitialized);

    // A sentinel proves subsequent retries do not re-enter the UI initialization block.
    PC->bLocalMatchUIInitialized = false;
    PC->TrySetupTableView();
    TestFalse(TEXT("Completed setup does not invoke UI initialization twice"), PC->bLocalMatchUIInitialized);
    World->DestroyWorld(false);
    return true;
}
#endif
