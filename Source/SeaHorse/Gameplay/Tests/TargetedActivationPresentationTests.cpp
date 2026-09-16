#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Engine/World.h"
#include "Gameplay/Core/SHGameMode.h"
#include "Gameplay/Core/SHGameState.h"
#include "Gameplay/Core/SHPlayerState.h"
#include "Gameplay/Core/SHPlayerController.h"
#include "Gameplay/Components/TurnComponent.h"
#include "Gameplay/SHHand.h"
#include "Gameplay/Cards/SHCard.h"
#include "Gameplay/Cards/CardDefinition.h"
#include "UObject/UnrealType.h"
#include "InputKeyEventArgs.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSHTargetedActivationPresentationTest,
    "SeaHorse.Gameplay.Effects.TargetedCircleWaitsForAllTargets",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSHTargetedActivationPresentationTest::RunTest(const FString& Parameters)
{
    // The lightweight test world does not initialize actors for play. Allow the
    // multicast's local ProcessEvent path, which initialized gameplay worlds run normally.
    TGuardValue<bool> ScriptExecutionGuard(GAllowActorScriptExecutionInEditor, true);
    UClass* Definition = LoadClass<UCardDefinition>(nullptr,
        TEXT("/Game/SeaHorse/Cards/Definitions/Card_PaulusSilent.Card_PaulusSilent_C"));
    if (!TestNotNull(TEXT("Two-stage target effect definition"), Definition)) { return false; }
    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
    ASHGameMode* Mode = World->SpawnActor<ASHGameMode>();
    ASHGameState* State = World->SpawnActor<ASHGameState>();
    World->SetGameState(State);
    Mode->GameState = State;
    // Exercise the controller's actual server RPC lookup, not only GameMode directly.
    const FObjectProperty* AuthorityMode = FindFProperty<FObjectProperty>(UWorld::StaticClass(), TEXT("AuthorityGameMode"));
    if (!TestNotNull(TEXT("World authority GameMode property"), AuthorityMode)) { World->DestroyWorld(false); return false; }
    AuthorityMode->SetObjectPropertyValue_InContainer(World, Mode);
    Mode->TurnComponent = NewObject<UTurnComponent>(Mode);
    ASHPlayerState* Player = World->SpawnActor<ASHPlayerState>();
    ASHPlayerState* Source = World->SpawnActor<ASHPlayerState>();
    State->AddPlayerState(Player);
    State->AddPlayerState(Source);
    ASHHand* Hand = World->SpawnActor<ASHHand>();
    Player->SetHand(Hand);
    Source->SetHand(World->SpawnActor<ASHHand>());
    ASHHand* NPC = World->SpawnActor<ASHHand>();
    NPC->SetIsNPC(true);
    NPC->AddCard(World->SpawnActor<ASHCard>(), 0);
    State->SetParticipantHands({Hand, Source->GetHand(), NPC});
    ASHPlayerController* PC = World->SpawnActor<ASHPlayerController>();
    PC->SetAsLocalPlayerController();
    PC->PlayerState = Player;
    Player->SetOwner(PC);
    ASHCard* A = World->SpawnActor<ASHCard>();
    ASHCard* B = World->SpawnActor<ASHCard>();
    A->CardDefinition = Definition;
    B->CardDefinition = Definition;
    Hand->AddActivationPairToLogicalHand(A, B);
    Hand->SetActivationPairState(A, B, EActivationPairState::AbilityEffect);
    Mode->CardActivateEffect(Player, A, B);
    TestFalse(TEXT("Another player cannot cancel this choice"), Mode->CancelEffectTargetSelection(Source, A, B));
    TestTrue(TEXT("Controller can request cancellation"), PC->CancelEffectTargeting());
    TestFalse(TEXT("Cancellation clears pending choices"), Mode->IsWaitingForPlayerSelection());
    TestFalse(TEXT("Cancellation removes the unfinished task"), Mode->HasActiveEffectTasks());
    TestEqual(TEXT("Cancelled pair is ready again"), Hand->FindActivationPair(A)->State, EActivationPairState::Ready);
    TestFalse(TEXT("Cancelled pair is not spent"), Hand->FindActivationPair(A)->bActivated);
    TestFalse(TEXT("Repeated cancellation is harmless"), Mode->CancelEffectTargetSelection(Player, A, B));
    Hand->SetActivationPairState(A, B, EActivationPairState::AbilityEffect);
    Mode->CardActivateEffect(Player, A, B);
    Mode->SubmitPlayerSelection(Player, Player);
    FInputKeyEventArgs Escape;
    Escape.Key = EKeys::Escape;
    Escape.Event = IE_Pressed;
    TestTrue(TEXT("ESC cancels second Paulus target selection through the controller"), PC->InputKey(Escape));
    TestFalse(TEXT("Second-stage choice is cleared"), Mode->IsWaitingForPlayerSelection());
    TestFalse(TEXT("Cancelled Paulus does not queue a draw restriction"), Mode->TurnComponent->ForcedDrawSources.Contains(Player));
    TestTrue(TEXT("Cancelled targeting never starts activation VFX"), Hand->PresentedActivationVFX.IsEmpty());
    Hand->SetActivationPairState(A, B, EActivationPairState::AbilityEffect);
    Mode->CardActivateEffect(Player, A, B);
    TestTrue(TEXT("Opening target selection does not play the circle"), Hand->PresentedActivationVFX.IsEmpty());
    Mode->SubmitPlayerSelection(Player, Player);
    TestTrue(TEXT("First accepted target does not play the circle"), Hand->PresentedActivationVFX.IsEmpty());
    TestTrue(TEXT("Second target selection remains pending"), Mode->PendingParticipantSelections.Contains(Player));
    TestTrue(TEXT("NPC stack is an authoritative source candidate"), Mode->PendingParticipantSelections.FindChecked(Player).Candidates.Contains(NPC));
    Mode->SubmitParticipantSelection(Player, Hand); // Invalid source: cannot draw from self.
    TestTrue(TEXT("Rejected target does not play the circle"), Hand->PresentedActivationVFX.IsEmpty());
    Mode->SubmitParticipantSelection(Player, NPC);
    TestEqual(TEXT("Final target starts exactly one circle"), Hand->PresentedActivationVFX.Num(), 1);
    TestTrue(TEXT("Circle holds pair movement"), Hand->IsPairMovementBlocked());
    TestFalse(TEXT("All target choices have completed"), Mode->PendingParticipantSelections.Contains(Player));
    TestEqual(TEXT("NPC hand is queued as the forced draw source"), Mode->TurnComponent->GetFirstForcedDrawSourceHand(Player), NPC);
    Mode->SubmitParticipantSelection(Player, NPC);
    TestEqual(TEXT("Duplicate final submission does not replay circle"), Hand->PresentedActivationVFX.Num(), 1);
    TestFalse(TEXT("Already resolved effects cannot be cancelled"), Mode->CancelEffectTargetSelection(Player, A, B));
    World->DestroyWorld(false);
    return true;
}
#endif
