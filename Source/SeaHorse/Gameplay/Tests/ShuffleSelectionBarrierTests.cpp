#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Components/SplineComponent.h"
#include "Engine/World.h"
#include "Gameplay/Cards/SHCard.h"
#include "Gameplay/Components/CardsLayoutComponent.h"
#include "Gameplay/Core/SHGameState.h"
#include "Gameplay/Core/SHPlayerController.h"
#include "Gameplay/SHHand.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSHShuffleSelectionBarrierTest, "SeaHorse.Gameplay.Input.ShuffleSelectionBarrier",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSHShuffleSelectionBarrierTest::RunTest(const FString& Parameters)
{
	for (bool bNPC : {false, true})
	{
		UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
		World->SetGameState(World->SpawnActor<ASHGameState>());
		auto* PC = World->SpawnActor<ASHPlayerController>(); PC->SetAsLocalPlayerController(); World->AddController(PC);
		auto* Source = World->SpawnActor<ASHHand>(); Source->SetIsNPC(bNPC);
		auto* Other = World->SpawnActor<ASHHand>();
		TArray<ASHCard*> Original;
		for (int32 Index = 0; Index < 3; ++Index)
		{
			auto* Card = World->SpawnActor<ASHCard>();
			auto* Root = NewObject<USceneComponent>(Card); Card->AddInstanceComponent(Root); Card->SetRootComponent(Root); Root->RegisterComponent();
			Source->AddCard(Card, Index); Original.Add(Card);
		}
		const TArray<ASHCard*> Shuffled = {Original[1], Original[2], Original[0]};
		const TArray<ASHCard*> Candidates = bNPC ? TArray<ASHCard*>{Shuffled.Last()} : Shuffled;
		PC->ClientRequestHandCardsSelectionAfterShuffle_Implementation(Source, Shuffled, Candidates, 1, 1, nullptr);
		TestTrue(TEXT("An offer arriving before the shuffled order blocks world input"), PC->bAwaitingPlayerSelectionResponse);
		TestTrue(TEXT("Old visible order exposes no clickable draw candidates"), PC->LocalHandCardSelectionCandidates.IsEmpty());
		TestTrue(TEXT("Waiting absorbs clicks instead of passing them to ordinary gameplay"), PC->TryHandleEffectSelectionClick(Original[0]));
		Source->RemoveCard(Original[0]); Source->AddCard(Original[0], Source->GetCardCount());
		Original[0]->SetOwner(Other); // Ownership may arrive after the hand array.
		PC->PollShuffledHandSelection();
		TestTrue(TEXT("Matching order still waits for per-card ownership replication"), PC->LocalHandCardSelectionCandidates.IsEmpty());
		Original[0]->SetOwner(Source);
		PC->PollShuffledHandSelection();
		TestFalse(TEXT("Complete replication releases the wait"), PC->bAwaitingPlayerSelectionResponse);
		TestEqual(TEXT("Only the server's post-shuffle choices become selectable"), PC->LocalHandCardSelectionCandidates.Num(), Candidates.Num());
		if (bNPC) { TestEqual(TEXT("BN exposes the new top card"), PC->LocalHandCardSelectionCandidates[0].Get(), Shuffled.Last()); }

		PC->ClientRequestHandCardsSelectionAfterShuffle_Implementation(Source, Original, Original, 1, 1, nullptr);
		PC->ClearLocalEffectSelectionState();
		PC->ClientRequestHandCardsSelection_Implementation({Shuffled[0]}, 1, 1, nullptr);
		PC->PollShuffledHandSelection();
		TestEqual(TEXT("An abandoned shuffle cannot overwrite a newer selection"), PC->LocalHandCardSelectionCandidates.Num(), 1);
		TestEqual(TEXT("Newer selection survives cancellation of the earlier wait"), PC->LocalHandCardSelectionCandidates[0].Get(), Shuffled[0]);

		auto* Layout = NewObject<USHHandCardsLayoutComponent>(Source);
		Source->AddInstanceComponent(Layout); Layout->RegisterComponent(); Layout->RegisterAllComponentTickFunctions(true); Layout->BeginPlay();
		auto* Spline = NewObject<USplineComponent>(Source);
		Source->AddInstanceComponent(Spline); Spline->RegisterComponent();
		Spline->SetSplinePoints({FVector(-30, 0, 0), FVector(30, 0, 0)}, ESplineCoordinateSpace::World);
		Layout->Initialize(Shuffled, Spline, nullptr);
		if (bNPC) { Layout->UpdateNPCCardsPositions(Shuffled); }
		Shuffled[0]->SetActorLocation(FVector(0, 100, 100));
		PC->ClientRequestHandCardsSelectionAfterShuffle_Implementation(Source, Shuffled, Candidates, 1, 1, nullptr);
		TestTrue(TEXT("Draw remains unavailable while cards visibly move to their shuffled positions"), PC->LocalHandCardSelectionCandidates.IsEmpty());
		Layout->MoveCardsToDesiredPositions(1.0f);
		PC->PollShuffledHandSelection();
		TestEqual(TEXT("Settled hand unlocks the post-shuffle selection"), PC->LocalHandCardSelectionCandidates.Num(), Candidates.Num());
		PC->ClearLocalEffectSelectionState();
		World->DestroyWorld(false);
	}
	return true;
}
#endif
