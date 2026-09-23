#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "NiagaraSystem.h"
#include "Gameplay/SHHand.h"
#include "Gameplay/Cards/SHCard.h"
#include "Gameplay/Cards/CardDefinition.h"
#include "Gameplay/Cards/Fragments/CardEffectFragment.h"
#include "Gameplay/Cards/Tasks/CardEffectTask.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSHStandardEffectPresentationTest, "SeaHorse.Gameplay.Effects.StandardActivationPresentation",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSHStandardEffectPresentationTest::RunTest(const FString& Parameters)
{
    UNiagaraSystem* System = LoadObject<UNiagaraSystem>(nullptr, TEXT("/Game/SeaHorse/VFX/MagicCircle/NS_MagicCircle.NS_MagicCircle"));
    if (!TestNotNull(TEXT("Magic circle asset exists"), System)) { return false; }
    const TArray<FString> Standard = {TEXT("Card_Aramdila"), TEXT("Card_CrumoUrsula"), TEXT("Card_Fimarik"), TEXT("Card_Gnushor"), TEXT("Card_Otfried")};
    const TArray<FString> Targeted = {TEXT("Card_Gloria"), TEXT("Card_OlgaPriest"), TEXT("Card_PaulusSilent"), TEXT("Card_Wilhelm")};
    auto CheckCards = [this, System](const TArray<FString>& Names, bool bRequiresSelection)
    {
        for (const FString& Name : Names)
        {
            const FString Path = FString::Printf(TEXT("/Game/SeaHorse/Cards/Definitions/%s.%s_C"), *Name, *Name);
            UClass* CardClass = LoadClass<UCardDefinition>(nullptr, *Path);
            const UCardEffectFragment* Fragment = Cast<UCardEffectFragment>(UCardDefinition::FindFragmentByClass(CardClass, UCardEffectFragment::StaticClass()));
            if (TestNotNull(*Name, Fragment))
            {
                TestEqual(*FString::Printf(TEXT("%s has magic circle"), *Name), Fragment->ActivationVFX.Get(), System);
                TestEqual(TEXT("Presentation matches Niagara lifetime"), Fragment->ActivationVFXDuration, 1.5f);
                if (TestNotNull(TEXT("Effect task class"), Fragment->EffectTaskClass.Get()))
                {
                    TestEqual(TEXT("Effect uses the correct activation timing"), Fragment->EffectTaskClass->GetDefaultObject<UCardEffectTask>()->RequiresTargetSelection(), bRequiresSelection);
                }
            }
        }
    };
    CheckCards(Standard, false);
    CheckCards(Targeted, true);

    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
    ASHHand* Hand = World->SpawnActor<ASHHand>();
    ASHCard* A = World->SpawnActor<ASHCard>();
    ASHCard* B = World->SpawnActor<ASHCard>();
    Hand->MulticastPlayActivationVFX_Implementation(A, B, System, 1.5f);
    Hand->MulticastPlayActivationVFX_Implementation(A, B, System, 1.5f);
    TestEqual(TEXT("A repeated notification does not spawn another circle"), Hand->PresentedActivationVFX.Num(), 1);
    TestTrue(TEXT("Pair movement waits for the visual effect"), Hand->IsPairMovementBlocked());
    TestEqual(TEXT("A repeated notification does not duplicate the lock"), Hand->LocalPresentationBlocks.Num(), 1);
    ++GFrameCounter;
    World->GetTimerManager().Tick(0.01f); // Move newly scheduled timers into the active timer heap.
    ++GFrameCounter;
    World->GetTimerManager().Tick(1.6f);
    TestFalse(TEXT("The lock expires without a Niagara completion callback"), Hand->IsPairMovementBlocked());
    Hand->MulticastBeginPairEffectExecution_Implementation(A, B);
    Hand->MulticastPlayActivationVFX_Implementation(A, B, System, 1.5f);
    Hand->MulticastPlayActivationVFX_Implementation(A, B, System, 1.5f);
    TestTrue(TEXT("A new execution of the same pair replays its circle and movement lock"), Hand->IsPairMovementBlocked());
    TestEqual(TEXT("Duplicate notifications within the second execution remain suppressed"), Hand->LocalPresentationBlocks.Num(), 1);
    ++GFrameCounter; World->GetTimerManager().Tick(0.01f);
    ++GFrameCounter; World->GetTimerManager().Tick(1.6f);
    TestFalse(TEXT("Second execution releases its own lock"), Hand->IsPairMovementBlocked());
    World->DestroyWorld(false);
    return true;
}
#endif
