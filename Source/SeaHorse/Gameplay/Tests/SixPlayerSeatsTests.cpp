#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Engine/World.h"
#include "Gameplay/Core/SHGameMode.h"
#include "Gameplay/Core/SHGameState.h"
#include "Gameplay/Core/SHPlayerState.h"
#include "Gameplay/Components/DeckComponent.h"
#include "Gameplay/Components/TurnComponent.h"
#include "Gameplay/Cards/SHCard.h"
#include "Gameplay/SHHand.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSHSixPlayerSeatsTest,
    "SeaHorse.Gameplay.Players.TwoToSix", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSHSixPlayerSeatsTest::RunTest(const FString& Parameters)
{
    TGuardValue<bool> ScriptExecution(GAllowActorScriptExecutionInEditor, true);
    for (const int32 SeatCount : {4, 6})
    for (int32 HumanCount = 2; HumanCount <= SeatCount; ++HumanCount)
    {
        UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
        ASHGameMode* Mode = World->SpawnActor<ASHGameMode>();
        ASHGameState* State = World->SpawnActor<ASHGameState>();
        World->SetGameState(State);
        Mode->GameState = State;
        TArray<ASHHand*> Hands;
        TArray<ASHPlayerState*> Players;
        for (int32 Seat = 0; Seat < SeatCount; ++Seat)
        {
            ASHHand* Hand = World->SpawnActor<ASHHand>();
            Hand->SetLayoutSeatIndex(Seat);
            Hands.Add(Hand);
            if (Seat < HumanCount)
            {
                ASHPlayerState* Player = World->SpawnActor<ASHPlayerState>();
                State->AddPlayerState(Player);
                Player->SetHand(Hand);
                Players.Add(Player);
            }
        }
        FString Error;
        TestTrue(TEXT("Discover valid map seats"), Mode->DiscoverTableSeats(Error));
        TestEqual(TEXT("Capacity comes from map"), Mode->TotalSeatCount, SeatCount);
        Mode->AssignSeats();
        Mode->InitializeParticipantHands();
        TestEqual(TEXT("All seats participate"), State->GetParticipantCount(), SeatCount);
        TestEqual(TEXT("Unoccupied seats are BN"), State->GetNPCHands().Num(), SeatCount - HumanCount);
        UDeckComponent* Deck = NewObject<UDeckComponent>(Mode);
        const int32 CardCount = SeatCount * 3 + 1;
        for (int32 Index = 0; Index < CardCount; ++Index)
            Deck->Deck.Add(World->SpawnActor<ASHCard>());
        TestTrue(TEXT("Deal selects a human to start"), Players.Contains(Deck->DealCards()));
        int32 DealtCount = 0;
        for (ASHHand* Hand : Hands)
        {
            DealtCount += Hand->GetCardCount();
            TestTrue(TEXT("Every seat receives its share"), Hand->GetCardCount() >= 3 && Hand->GetCardCount() <= 4);
        }
        TestEqual(TEXT("No cards lost while dealing"), DealtCount, CardCount);
        UTurnComponent* Turns = NewObject<UTurnComponent>(Mode);
        for (int32 Index = 0; Index < Players.Num(); ++Index)
            TestEqual(TEXT("Turn order wraps and skips BN"), Turns->ChooseNextPlayer_Implementation(Players[Index]), Players[(Index + 1) % HumanCount]);
        ASHCard* LastSeatCard = Hands.Last()->GetCards()[0];
        Mode->PassHandsToLeft();
        TestTrue(TEXT("Rotation wraps final seat to zero"), Hands[0]->GetCards().Contains(LastSeatCard));
        Hands.Last()->SetLayoutSeatIndex(0);
        TestFalse(TEXT("Duplicate seats rejected"), Mode->DiscoverTableSeats(Error));
        Hands.Last()->SetLayoutSeatIndex(SeatCount);
        TestFalse(TEXT("Missing or out of range seats rejected"), Mode->DiscoverTableSeats(Error));
        World->DestroyWorld(false);
    }
    return true;
}
#endif
