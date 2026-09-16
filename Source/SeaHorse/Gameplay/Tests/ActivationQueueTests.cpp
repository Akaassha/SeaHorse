#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Engine/World.h"
#include "Gameplay/Core/SHGameMode.h"
#include "Gameplay/Core/SHPlayerState.h"
#include "Gameplay/Core/SHGameState.h"
#include "Gameplay/Core/SHPlayerController.h"
#include "Gameplay/Components/TurnComponent.h"
#include "Gameplay/SHHand.h"
#include "Gameplay/Cards/SHCard.h"
#include "Gameplay/Cards/CardDefinition.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSHActivationQueueReadinessTest, "SeaHorse.Gameplay.Effects.ActivationQueueReadiness",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSHActivationQueueReadinessTest::RunTest(const FString& Parameters)
{
    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
    ASHGameMode* Mode = World->SpawnActor<ASHGameMode>();
    Mode->TurnComponent = NewObject<UTurnComponent>(Mode);
    ASHPlayerState* Player = World->SpawnActor<ASHPlayerState>();
    ASHHand* Hand = World->SpawnActor<ASHHand>();
    Player->SetHand(Hand);
    ASHCard* CardA = World->SpawnActor<ASHCard>();
    ASHCard* CardB = World->SpawnActor<ASHCard>();
    Hand->AddActivationPairToLogicalHand(CardA, CardB);
    Hand->SetActivationPairState(CardA, CardB, EActivationPairState::Creating);

    auto& Pending = Mode->PendingPairActivations.AddDefaulted_GetRef();
    Pending.ActivatingPlayer = Player;
    Pending.CardA = CardA;
    Pending.CardB = CardB;

    // An animation has not settled yet: the server must yield instead of spinning on this pair.
    Mode->TryProcessQueuedPairActivations();
    TestEqual(TEXT("Unsettled pair remains queued"), Mode->PendingPairActivations.Num(), 1);
    TestFalse(TEXT("Click presentation waits for settlement"), Mode->PendingPairActivations[0].bClickPresentationStarted);
    TestFalse(TEXT("Processing guard is released while waiting"), Mode->bProcessingPairActivations);

    // A presentation callback may request another pass while the outer pass owns the queue.
    Hand->RemoveActivationPair(CardA, CardB);
    Mode->bProcessingPairActivations = true;
    Mode->TryProcessQueuedPairActivations();
    TestEqual(TEXT("Nested pass leaves the outer queue untouched"), Mode->PendingPairActivations.Num(), 1);
    Mode->bProcessingPairActivations = false;
    Mode->TryProcessQueuedPairActivations();
    TestTrue(TEXT("A later pass removes a pair that no longer exists"), Mode->PendingPairActivations.IsEmpty());
    TestFalse(TEXT("Processing guard is released after draining"), Mode->bProcessingPairActivations);

    ASHGameState* State = World->SpawnActor<ASHGameState>();
    World->SetGameState(State);
    State->AddPlayerState(Player);
    State->SetParticipantHands({Hand});
    Player->SetSeatIndex(0);
    Hand->SetLayoutSeatIndex(0);
    Hand->SetRepresentedPlayerState(Player);
    ASHPlayerController* PC = World->SpawnActor<ASHPlayerController>();
    // This synthetic world does not run actor initialization, which normally registers controllers.
    World->AddController(PC);
    PC->SetAsLocalPlayerController();
    PC->PlayerState = Player;
    TestEqual(TEXT("Presentation can resolve the local controller"), World->GetFirstPlayerController(), static_cast<APlayerController*>(PC));
    Mode->TurnComponent->InitializeTurns(Player);
    CardA->CardDefinition = UCardDefinition::StaticClass();
    CardB->CardDefinition = CardA->CardDefinition;
    Hand->AddActivationPairToLogicalHand(CardA, CardB);
    Hand->SetActivationPairState(CardA, CardB, EActivationPairState::Ready);
    Hand->RefreshPairActivationAvailability();
    Hand->SetLocalActivatableCardHovered(CardA, true);
    TestEqual(TEXT("Ready pair has its activation indicator"), Hand->LocallyActivatablePairs.Num(), 1);
    TestTrue(TEXT("Ready pair is hovered"), Hand->bHasLocallyHoveredActivatablePair);

    // The first additional-draw task owns the queue until its draw is resolved.
    ASHCard* FirstA = World->SpawnActor<ASHCard>();
    ASHCard* FirstB = World->SpawnActor<ASHCard>();
    Hand->AddActivationPairToLogicalHand(FirstA, FirstB);
    Hand->SetActivationPairState(FirstA, FirstB, EActivationPairState::AbilityEffect);
    auto& Active = Mode->PendingPairActivations.AddDefaulted_GetRef();
    Active.ActivatingPlayer = Player;
    Active.CardA = FirstA;
    Active.CardB = FirstB;
    Active.bClickPresentationStarted = true;
    Active.bAbilityStarted = true;
    Mode->RequestStoredPairActivation(Player, CardA);
    TestEqual(TEXT("Second pair waits behind the active effect"), Mode->PendingPairActivations.Num(), 2);
    TestTrue(TEXT("Acceptance is stored in the replicated pair"), Hand->FindActivationPair(CardA)->bActivationQueued);
    TestTrue(TEXT("Listen host clears the second indicator immediately"), Hand->LocallyActivatablePairs.IsEmpty());
    TestFalse(TEXT("Listen host clears hover immediately"), Hand->bHasLocallyHoveredActivatablePair);
    TestFalse(TEXT("Queued pair cannot be activated twice"), Hand->CanLocalPlayerActivatePair(CardA));
    Mode->RequestStoredPairActivation(Player, CardB);
    TestEqual(TEXT("Repeated click cannot duplicate the queued pair"), Mode->PendingPairActivations.Num(), 2);

    // Replication or an unrelated UI refresh must not restore the queued indicator.
    Hand->RefreshActivationPairsPresentation();
    TestTrue(TEXT("Queued indicator stays off after presentation refresh"), Hand->LocallyActivatablePairs.IsEmpty());
    Mode->CompleteQueuedPairActivation(CardA, CardB);
    TestFalse(TEXT("A retained pair releases its queue flag"), Hand->FindActivationPair(CardA)->bActivationQueued);
    TestTrue(TEXT("Retained ready pair becomes available again"), Hand->CanLocalPlayerActivatePair(CardA));
    World->DestroyWorld(false);
    return true;
}

#endif
