#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "Gameplay/Core/SHGameMode.h"
#include "Gameplay/Core/SHGameState.h"
#include "Gameplay/Core/SHPlayerState.h"
#include "Gameplay/Components/TurnComponent.h"
#include "Gameplay/Cards/Tasks/NewCardEffectTasks.h"
#include "Gameplay/Cards/SHCard.h"
#include "Gameplay/SHHand.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSHRotationPresentationTest,
    "SeaHorse.Gameplay.Effects.RotationWaitsForPresentation",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSHRotationPresentationTest::RunTest(const FString& Parameters)
{
    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
    ASHGameMode* Mode = World->SpawnActor<ASHGameMode>();
    ASHGameState* State = World->SpawnActor<ASHGameState>();
    World->SetGameState(State);
    Mode->GameState = State;
    Mode->TurnComponent = NewObject<UTurnComponent>(Mode);
    ASHPlayerState* Player = World->SpawnActor<ASHPlayerState>();
    ASHHand* First = World->SpawnActor<ASHHand>();
    ASHHand* Second = World->SpawnActor<ASHHand>();
    First->SetLayoutSeatIndex(0);
    Second->SetLayoutSeatIndex(1);
    State->SetParticipantHands({First, Second});
    Player->SetHand(First);
    ASHCard* Moving = World->SpawnActor<ASHCard>();
    First->AddCard(Moving, 0);
    URotateHandsLeftEffectTask* Task = NewObject<URotateHandsLeftEffectTask>(Mode);
    Task->Initialize(Player, World->SpawnActor<ASHCard>(), World->SpawnActor<ASHCard>(), NAME_None);
    Mode->ActiveEffectTasks.Add(Task);
    Mode->TurnComponent->BeginTurnTransitionBlock(TEXT("TestAnimation"));
    Task->StartEffect_Implementation();
    TestTrue(TEXT("Cards remain in place during animation"), First->GetCards().Contains(Moving));
    TestFalse(TEXT("Effect remains active"), Task->IsFinished());
    ++GFrameCounter; World->GetTimerManager().Tick(0.01f);
    ++GFrameCounter; World->GetTimerManager().Tick(0.1f);
    TestTrue(TEXT("Retry does not bypass presentation lock"), First->GetCards().Contains(Moving));
    Mode->TurnComponent->FinishTurnTransitionBlock(TEXT("TestAnimation"));
    ++GFrameCounter; World->GetTimerManager().Tick(0.1f);
    TestTrue(TEXT("Cards rotate after animation ends"), Second->GetCards().Contains(Moving));
    TestTrue(TEXT("Rotation completes the task"), Task->IsFinished());
    World->DestroyWorld(false);
    return true;
}
#endif
