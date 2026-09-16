#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Engine/World.h"
#include "Components/StaticMeshComponent.h"
#include "Gameplay/Core/SHPlayerController.h"
#include "Gameplay/Cards/SHCard.h"
#include "Gameplay/SHHand.h"
#include "Gameplay/Player/SHPlayerRepresentation.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSHEffectTargetOutlineTest,
    "SeaHorse.Gameplay.Effects.TargetOutlineTransitions",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSHEffectTargetOutlineTest::RunTest(const FString& Parameters)
{
    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
    ASHPlayerController* PC = World->SpawnActor<ASHPlayerController>();
    ASHCard* Valid = World->SpawnActor<ASHCard>();
    ASHCard* Invalid = World->SpawnActor<ASHCard>();
    auto AddMesh = [](AActor* Card)
    {
        UStaticMeshComponent* Mesh = NewObject<UStaticMeshComponent>(Card);
        Card->AddInstanceComponent(Mesh);
        Mesh->RegisterComponent();
        return Mesh;
    };
    UStaticMeshComponent* ValidMesh = AddMesh(Valid);
    UStaticMeshComponent* InvalidMesh = AddMesh(Invalid);
    ValidMesh->SetCustomDepthStencilValue(3);
    ValidMesh->SetCustomDepthStencilWriteMask(ERendererStencilMask::ERSM_255);
    ValidMesh->SetRenderCustomDepth(true);
    ValidMesh->SetCustomPrimitiveDataFloat(20, 0.25f);
    PC->LocalActivationPairSelectionCandidates.Add(Valid);
    PC->UpdateEffectTargetOutlines(nullptr);
    TestTrue(TEXT("Valid target is outlined"), ValidMesh->bRenderCustomDepth != 0);
    TestEqual(TEXT("Valid target is white"), ValidMesh->CustomDepthStencilValue, 250);
    TestEqual(TEXT("Valid target enables white material reflections"), ValidMesh->GetCustomPrimitiveData().Data[20], 2.0f);
    TestEqual(TEXT("Invalid target suppresses ordinary material reflections"), InvalidMesh->GetCustomPrimitiveData().Data[20], 1.0f);
    TestFalse(TEXT("Invalid target without hover has no outline"), InvalidMesh->bRenderCustomDepth != 0);
    PC->UpdateEffectTargetOutlines(Valid);
    TestEqual(TEXT("Valid hover is green"), ValidMesh->CustomDepthStencilValue, 251);
    TestEqual(TEXT("Valid hover enables stronger green reflections"), ValidMesh->GetCustomPrimitiveData().Data[20], 3.0f);
    PC->UpdateEffectTargetOutlines(Invalid);
    TestEqual(TEXT("Previous valid hover returns to white"), ValidMesh->CustomDepthStencilValue, 250);
    TestTrue(TEXT("Invalid hover is visible"), InvalidMesh->bRenderCustomDepth != 0);
    TestEqual(TEXT("Invalid hover is red"), InvalidMesh->CustomDepthStencilValue, 252);
    TestEqual(TEXT("Invalid hover enables stronger red reflections"), InvalidMesh->GetCustomPrimitiveData().Data[20], 4.0f);
    TestEqual(TEXT("Previous hover returns to normal reflection intensity"), ValidMesh->GetCustomPrimitiveData().Data[20], 2.0f);
    PC->LocalActivationPairSelectionCandidates.Reset();
    PC->UpdateEffectTargetOutlines(Valid);
    TestEqual(TEXT("Changed candidate list updates hover validity"), ValidMesh->CustomDepthStencilValue, 252);
    TestFalse(TEXT("Previous invalid hover is cleared"), InvalidMesh->bRenderCustomDepth != 0);
    PC->RestoreEffectTargetOutlines();
    TestTrue(TEXT("Original interaction outline restored"), ValidMesh->bRenderCustomDepth != 0);
    TestEqual(TEXT("Original stencil restored"), ValidMesh->CustomDepthStencilValue, 3);
    TestTrue(TEXT("Original write mask restored"), ValidMesh->CustomDepthStencilWriteMask == ERendererStencilMask::ERSM_255);
    TestFalse(TEXT("Initially disabled mesh restored"), InvalidMesh->bRenderCustomDepth != 0);
    TestTrue(TEXT("Snapshots released"), PC->EffectOutlineMeshes.IsEmpty());
    TestEqual(TEXT("Previous material highlight state restored"), ValidMesh->GetCustomPrimitiveData().Data[20], 0.25f);
    TestEqual(TEXT("Ordinary Blueprint material behavior restored"), InvalidMesh->GetCustomPrimitiveData().Data[20], 0.0f);
    ASHHand* NPC = World->SpawnActor<ASHHand>();
    NPC->SetIsNPC(true);
    ASHHand* VisualSeat = World->SpawnActor<ASHHand>();
    VisualSeat->SetRepresentedHand(NPC);
    ASHPlayerRepresentation* Picker = World->SpawnActor<ASHPlayerRepresentation>();
    Picker->BindToHand(VisualSeat);
    UStaticMeshComponent* PickerMesh = AddMesh(Picker);
    PC->LocalParticipantSelectionCandidates.Add(NPC);
    TestTrue(TEXT("NPC picker uses the eligible logical hand"), PC->IsValidEffectTarget(Picker));
    PC->UpdateEffectTargetOutlines(nullptr);
    TestEqual(TEXT("NPC representation is white"), PickerMesh->CustomDepthStencilValue, 250);
    PC->UpdateEffectTargetOutlines(Picker);
    TestEqual(TEXT("NPC representation hover is green"), PickerMesh->CustomDepthStencilValue, 251);
    PC->RestoreEffectTargetOutlines();
    ValidMesh->SetRenderCustomDepth(true);
    PC->TargetingSourceCardA = Valid;
    PC->TargetingSourceCardB = Invalid;
    PC->UpdateEffectTargetOutlines(nullptr);
    PC->StopPairTargetingIndicator();
    TestFalse(TEXT("Ending selection does not restore spent source outline"), ValidMesh->bRenderCustomDepth != 0);
    ValidMesh->SetRenderCustomDepth(true);
    ValidMesh->SetCustomPrimitiveDataFloat(20, 3.0f);
    Valid->SetCardZone(ECardZone::Victory);
    TestFalse(TEXT("Victory movement clears outline on the card"), ValidMesh->bRenderCustomDepth != 0);
    TestEqual(TEXT("Victory movement clears target reflections"), ValidMesh->GetCustomPrimitiveData().Data[20], 0.0f);
    World->DestroyWorld(false);
    return true;
}
#endif
