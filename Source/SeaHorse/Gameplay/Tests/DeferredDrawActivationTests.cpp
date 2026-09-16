#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Engine/World.h"
#include "Gameplay/Core/SHGameMode.h"
#include "Gameplay/Core/SHGameState.h"
#include "Gameplay/Core/SHPlayerState.h"
#include "Gameplay/Core/SHPlayerController.h"
#include "Gameplay/Components/TurnComponent.h"
#include "Gameplay/Cards/Tasks/AdditionalDrawEffectTask.h"
#include "Gameplay/Cards/SHCard.h"
#include "Gameplay/Cards/CardDefinition.h"
#include "Gameplay/SHHand.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSHDeferredDrawActivationTest,
    "SeaHorse.Gameplay.Effects.PaulusBeforeQueuedDraws",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSHDeferredDrawActivationTest::RunTest(const FString& Parameters)
{
    TGuardValue<bool> ScriptGuard(GAllowActorScriptExecutionInEditor, true);
    UClass* Paulus = LoadClass<UCardDefinition>(nullptr,
        TEXT("/Game/SeaHorse/Cards/Definitions/Card_PaulusSilent.Card_PaulusSilent_C"));
    if (!TestNotNull(TEXT("Paulus definition"), Paulus)) { return false; }
    for (bool bSameSource : {true, false})
    {
        UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
        ASHGameMode* Mode = World->SpawnActor<ASHGameMode>();
        ASHGameState* State = World->SpawnActor<ASHGameState>();
        World->SetGameState(State);
        Mode->GameState = State;
        Mode->TurnComponent = NewObject<UTurnComponent>(Mode);
        ASHPlayerState* Player = World->SpawnActor<ASHPlayerState>();
        State->AddPlayerState(Player);
        ASHHand* Hand = World->SpawnActor<ASHHand>();
        ASHHand* Source = World->SpawnActor<ASHHand>();
        ASHHand* Other = World->SpawnActor<ASHHand>();
        Source->SetIsNPC(true);
        Other->SetIsNPC(true);
        Player->SetHand(Hand);
        State->SetParticipantHands({Hand, Source, Other});
        ASHPlayerController* PC = World->SpawnActor<ASHPlayerController>();
        PC->PlayerState = Player;
        Player->SetOwner(PC);
        Mode->TurnComponent->InitializeTurns(Player);
        Source->AddCard(World->SpawnActor<ASHCard>(), 0);
        Source->AddCard(World->SpawnActor<ASHCard>(), 1);
        Other->AddCard(World->SpawnActor<ASHCard>(), 0);
        ASHCard* ExtraA = World->SpawnActor<ASHCard>();
        ASHCard* ExtraB = World->SpawnActor<ASHCard>();
        Hand->AddActivationPairToLogicalHand(ExtraA, ExtraB);
        Hand->SetActivationPairState(ExtraA, ExtraB, EActivationPairState::AbilityEffect);
        UAdditionalDrawEffectTask* Extra = bSameSource
            ? static_cast<UAdditionalDrawEffectTask*>(NewObject<UDrawAgainFromSamePlayerEffectTask>(Mode))
            : static_cast<UAdditionalDrawEffectTask*>(NewObject<UDrawAgainFromDifferentPlayerEffectTask>(Mode));
        Extra->Initialize(Player, ExtraA, ExtraB, NAME_None);
        Mode->ActiveEffectTasks.Add(Extra);
        auto& Active = Mode->PendingPairActivations.AddDefaulted_GetRef();
        Active.ActivatingPlayer = Player;
        Active.CardA = ExtraA;
        Active.CardB = ExtraB;
        Active.bClickPresentationStarted = true;
        Active.bAbilityStarted = true;
        Extra->StartEffect_Implementation();
        ASHCard* A = World->SpawnActor<ASHCard>();
        ASHCard* B = World->SpawnActor<ASHCard>();
        A->CardDefinition = Paulus;
        B->CardDefinition = Paulus;
        Hand->AddActivationPairToLogicalHand(A, B);
        Hand->SetActivationPairState(A, B, EActivationPairState::Ready);
        Mode->RequestStoredPairActivation(Player, A);
        TestTrue(TEXT("Paulus starts choosing targets before any draw"), Mode->PendingPlayerSelections.Contains(Player));
        TestFalse(TEXT("Additional draw remains pending"), Extra->IsFinished());
        TestTrue(TEXT("Paulus can be cancelled while an extra draw is queued"), Mode->CancelEffectTargetSelection(Player, A, B));
        TestFalse(TEXT("Cancelling Paulus preserves the earlier extra draw"), Extra->IsFinished());
        TestTrue(TEXT("Earlier draw task stays active"), Mode->ActiveEffectTasks.Contains(Extra));
        Mode->RequestStoredPairActivation(Player, A);
        TestTrue(TEXT("Cancelled pair can be activated again"), Mode->PendingPlayerSelections.Contains(Player));
        Mode->SubmitPlayerSelection(Player, Player);
        Mode->SubmitParticipantSelection(Player, Source);
        TestTrue(TEXT("First draw respects Paulus"), Mode->TurnComponent->CanDrawCardFromHand(Player, Source));
        TestFalse(TEXT("First draw rejects another source"), Mode->TurnComponent->CanDrawCardFromHand(Player, Other));
        ASHCard* Drawn = Source->GetTopCard();
        Source->RemoveCard(Drawn);
        Hand->AddCard(Drawn, 0);
        Mode->TurnComponent->HandleCardDrawnFromHand(Player, Source);
        TestFalse(TEXT("First draw does not complete the extra draw"), Extra->IsFinished());
        TestEqual(TEXT("Second draw applies the original same/different rule"), Mode->TurnComponent->CanDrawCardFromHand(Player, Source), bSameSource);
        TestEqual(TEXT("Alternative source follows extra-draw rule"), Mode->TurnComponent->CanDrawCardFromHand(Player, Other), !bSameSource);
        Mode->TurnComponent->HandleCardDrawnFromHand(Player, bSameSource ? Source : Other);
        TestTrue(TEXT("Second draw completes the additional effect"), Extra->IsFinished());
        TestEqual(TEXT("Turn proceeds after both draws"), State->GetTurnPhase(), ETurnPhase::SecondPairing);
        World->DestroyWorld(false);
    }
    return true;
}
#endif
