#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Gameplay/Cards/CardDefinition.h"
#include "Gameplay/Cards/SHCard.h"
#include "Gameplay/Core/SHPlayerController.h"
#include "Gameplay/Presentation/SHHandRevealPawn.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSHHandComparisonPresentationTest,
	"SeaHorse.Gameplay.UI.HandComparisonPresentation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSHHandComparisonPresentationTest::RunTest(const FString& Parameters)
{
	UClass* CardClass = LoadClass<ASHCard>(nullptr, TEXT("/Game/SeaHorse/Cards/BP_Card.BP_Card_C"));
	UClass* Definition = LoadClass<UCardDefinition>(nullptr,
		TEXT("/Game/SeaHorse/Cards/Definitions/Card_BodgyVampireHunter.Card_BodgyVampireHunter_C"));
	if (!TestNotNull(TEXT("Card presentation Blueprint loads"), CardClass) ||
		!TestNotNull(TEXT("A real card definition loads"), Definition))
	{
		return false;
	}

	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
	ASHPlayerController* PC = World->SpawnActor<ASHPlayerController>();
	PC->SetAsLocalPlayerController();
	World->AddController(PC);
	ASHHandRevealPawn* Stage = World->SpawnActor<ASHHandRevealPawn>();
	Stage->SetOwner(PC);

	auto Entry = [World, CardClass, Definition]()
	{
		FSHRevealedHandCard Result;
		Result.SourceCard = World->SpawnActor<ASHCard>();
		Result.SourceCard->SetCardDefinition(Definition);
		Result.CardDefinition = Definition;
		Result.CardActorClass = CardClass;
		return Result;
	};
	TArray<FSHRevealedHandCard> Larger = {Entry(), Entry(), Entry()};
	TArray<FSHRevealedHandCard> Receiving = {Entry()};
	Stage->InitializeComparisonPresentation(PC, FGuid::NewGuid(), Larger, Receiving, 2, true);

	TestTrue(TEXT("Comparison mode is active"), Stage->IsComparingHands());
	TestTrue(TEXT("Drawing client can drag from the larger hand"), Stage->CanTransferComparedCards());
	TestEqual(TEXT("Presentation exposes the remaining difference"), Stage->GetRemainingTransfers(), 2);
	TestEqual(TEXT("Both hands receive local visual copies"), Stage->GetPresentationCards().Num(), 4);
	TestEqual(TEXT("Receiving row is represented separately"), Stage->GetReceivingPresentationCards().Num(), 1);

	const TArray<ASHCard*> AllVisuals = Stage->GetPresentationCards();
	const TArray<ASHCard*> ReceivingVisuals = Stage->GetReceivingPresentationCards();
	if (AllVisuals.Num() == 4 && ReceivingVisuals.Num() == 1)
	{
		const float TopAverageY = (AllVisuals[0]->GetRootComponent()->GetRelativeLocation().Y +
			AllVisuals[1]->GetRootComponent()->GetRelativeLocation().Y +
			AllVisuals[2]->GetRootComponent()->GetRelativeLocation().Y) / 3.f;
		const float BottomY = ReceivingVisuals[0]->GetRootComponent()->GetRelativeLocation().Y;
		TestTrue(TEXT("Larger hand is laid out above the receiving hand"), TopAverageY < BottomY);
	}

	FSHRevealedHandCard Moved = Larger[0];
	Larger.RemoveAt(0);
	Receiving.Add(Moved);
	Stage->ApplyComparisonSnapshot(Larger, Receiving, 1, true);
	TestEqual(TEXT("Snapshot update moves one visual into the receiving row"),
		Stage->GetReceivingPresentationCards().Num(), 2);
	TestEqual(TEXT("Updated difference remains visible"), Stage->GetRemainingTransfers(), 1);
	Stage->ApplyComparisonSnapshot(Larger, Receiving, 1, false);
	TestFalse(TEXT("Observing client cannot drag cards"), Stage->CanTransferComparedCards());
	Stage->ClearPresentation();
	TestEqual(TEXT("Closing comparison destroys every private visual"), Stage->GetPresentationCards().Num(), 0);

	World->DestroyWorld(false);
	GEngine->DestroyWorldContext(World);
	return true;
}
#endif
