#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Engine/World.h"
#include "Gameplay/Core/SHGameMode.h"
#include "Gameplay/Core/SHGameState.h"
#include "Gameplay/Core/SHPlayerState.h"
#include "Gameplay/Components/TurnComponent.h"
#include "Gameplay/SHHand.h"
#include "Gameplay/Board/VictoryStack.h"
#include "Gameplay/Cards/SHCard.h"
#include "Gameplay/Cards/CardDefinition.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSHBulkVictoryPresentationTest,
    "SeaHorse.Gameplay.Effects.BulkVictoryWaitsForPresentation",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSHBulkVictoryPresentationTest::RunTest(const FString& Parameters)
{
    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
    ASHGameMode* Mode = World->SpawnActor<ASHGameMode>();
    ASHGameState* State = World->SpawnActor<ASHGameState>();
    World->SetGameState(State);
    Mode->GameState = State;
    Mode->TurnComponent = NewObject<UTurnComponent>(Mode);
    TArray<ASHHand*> Hands;
    TArray<ASHCard*> FirstCards;
    for (int32 Index = 0; Index < 2; ++Index)
    {
        ASHPlayerState* Player = World->SpawnActor<ASHPlayerState>();
        State->AddPlayerState(Player);
        ASHHand* Hand = World->SpawnActor<ASHHand>();
        Player->SetHand(Hand);
        Hand->VictoryStack = World->SpawnActor<AVictoryStack>();
        ASHCard* A = World->SpawnActor<ASHCard>();
        ASHCard* B = World->SpawnActor<ASHCard>();
        A->CardDefinition = UCardDefinition::StaticClass();
        B->CardDefinition = A->CardDefinition;
        Hand->AddActivationPairToLogicalHand(A, B);
        Hands.Add(Hand);
        FirstCards.Add(A);
    }
    const FName CircleBlock(TEXT("GnushorCircle"));
    Mode->TurnComponent->BeginTurnTransitionBlock(CircleBlock);
    Mode->MoveAllActivationPairsToVictoryStacks();
    // Bulk collection and task completion can request the same pair again.
    Mode->MoveAllActivationPairsToVictoryStacks();
    Mode->FlushCompletedEffectPairs();
    TestEqual(TEXT("Each collected pair is queued only once"), Mode->CompletedEffectPairsWaitingForPresentation.Num(), 2);
    for (int32 Index = 0; Index < Hands.Num(); ++Index)
    {
        TestNotNull(TEXT("Pair stays on table during circle"), Hands[Index]->FindActivationPair(FirstCards[Index]));
        TestEqual(TEXT("Victory stack stays empty during circle"), Hands[Index]->GetVictoryStack()->GetPairCount(), 0);
    }
    Mode->TurnComponent->FinishTurnTransitionBlock(CircleBlock);
    // This synthetic world has no registered authoritative GameMode for the normal callback.
    Mode->FlushCompletedEffectPairs();
    Mode->FlushCompletedEffectPairs();
    for (int32 Index = 0; Index < Hands.Num(); ++Index)
    {
        TestNull(TEXT("Pair leaves table after circle"), Hands[Index]->FindActivationPair(FirstCards[Index]));
        TestEqual(TEXT("Each victory stack receives exactly one pair"), Hands[Index]->GetVictoryStack()->GetPairCount(), 1);
    }
    TestTrue(TEXT("Deferred moves are drained"), Mode->CompletedEffectPairsWaitingForPresentation.IsEmpty());
    World->DestroyWorld(false);
    return true;
}
#endif
