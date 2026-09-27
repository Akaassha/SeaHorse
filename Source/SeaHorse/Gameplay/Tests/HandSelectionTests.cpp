#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Engine/World.h"
#include "Gameplay/Cards/SHCard.h"
#include "Gameplay/Components/CardsLayoutComponent.h"
#include "Gameplay/Core/SHGameState.h"
#include "Gameplay/Core/SHPlayerController.h"
#include "Gameplay/Core/SHPlayerState.h"
#include "Gameplay/SHHand.h"
#include "UObject/StructOnScope.h"
#include "UObject/UnrealType.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSHHandSelectionTest, "SeaHorse.Gameplay.Input.HandSelection",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSHHandSelectionTest::RunTest(const FString& Parameters)
{
	TGuardValue<bool> ScriptGuard(GAllowActorScriptExecutionInEditor, true);
	UClass* HandClass = LoadClass<ASHHand>(nullptr, TEXT("/Game/SeaHorse/Cards/BP_Hand.BP_Hand_C"));
	UClass* CardClass = LoadClass<ASHCard>(nullptr, TEXT("/Game/SeaHorse/Cards/BP_Card.BP_Card_C"));
	UClass* ControllerClass = LoadClass<ASHPlayerController>(nullptr, TEXT("/Game/SeaHorse/Core/BP_SHPlayerController.BP_SHPlayerController_C"));
	UClass* GameStateClass = LoadClass<ASHGameState>(nullptr, TEXT("/Game/SeaHorse/Core/BP_SHGameState.BP_SHGameState_C"));
	if (!TestNotNull(TEXT("Saved hand Blueprint"), HandClass) || !TestNotNull(TEXT("Saved card Blueprint"), CardClass) ||
		!TestNotNull(TEXT("Saved controller Blueprint"), ControllerClass) || !TestNotNull(TEXT("Saved GameState Blueprint"), GameStateClass)) { return false; }
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	auto* GS = World->SpawnActor<ASHGameState>(GameStateClass); World->SetGameState(GS);
	auto* PC = World->SpawnActor<ASHPlayerController>(ControllerClass);
	PC->SetAsLocalPlayerController(); World->AddController(PC);
	auto* LocalPS = World->SpawnActor<ASHPlayerState>();
	auto* OtherPS = World->SpawnActor<ASHPlayerState>();
	PC->PlayerState = LocalPS;
	auto* LogicalHand = World->SpawnActor<ASHHand>();
	auto* OtherHand = World->SpawnActor<ASHHand>();
	LocalPS->SetHand(LogicalHand); OtherPS->SetHand(OtherHand);
	auto* VisualHand = World->SpawnActor<ASHHand>(HandClass);
	// Human mapping uses PlayerState; SetRepresentedHand is the BN path and
	// deliberately clears PlayerState (which the Blueprint ownership gate needs).
	VisualHand->SetRepresentedPlayerState(LocalPS);
	TestEqual(TEXT("Visual seat represents the local player"), VisualHand->GetRepresentedPlayerState(), LocalPS);
	TestEqual(TEXT("Client visual seat is distinct from its logical hand"), VisualHand->GetRepresentedHand(), LogicalHand);
	auto* Layout = VisualHand->FindComponentByClass<USHHandCardsLayoutComponent>();
	UFunction* ClickEvent = VisualHand->FindFunction(TEXT("Clicked"));
	if (!TestNotNull(TEXT("Production hand layout"), Layout) || !TestNotNull(TEXT("Production click event"), ClickEvent))
	{
		World->DestroyWorld(false); return false;
	}
	Layout->RegisterAllComponentTickFunctions(true); Layout->BeginPlay();
	VisualHand->Initialize();
	auto MakeCard = [&](ASHHand* Owner, int32 Index)
	{
		auto* Card = World->SpawnActor<ASHCard>(CardClass); Owner->AddCard(Card, Index); return Card;
	};
	ASHCard* First = MakeCard(LogicalHand, 0);
	ASHCard* Second = MakeCard(LogicalHand, 1);
	ASHCard* Foreign = MakeCard(OtherHand, 0);
	auto Click = [&](ASHCard* Card)
	{
		PC->PointerPressedCard = Card;
		FStructOnScope Params(ClickEvent);
		FindFProperty<FObjectPropertyBase>(ClickEvent, TEXT("Card"))->SetObjectPropertyValue_InContainer(Params.GetStructMemory(), Card);
		VisualHand->ProcessEvent(ClickEvent, Params.GetStructMemory());
	};
	GS->SetCurrentPlayer(LocalPS); GS->SetTurnPhase(ETurnPhase::FirstPairing);
	Click(First);
	TestEqual(TEXT("Real Blueprint click selects card on the client's visual seat"), Layout->GetSelectedCard(), First);
	GS->SetCurrentPlayer(OtherPS);
	Click(First);
	TestNull(TEXT("Same-card click deselects immediately during another player's turn"), Layout->GetSelectedCard());
	TestEqual(TEXT("Off-turn deselection also clears the Blueprint selection query"), VisualHand->GetSelectedHandCardIndex(), INDEX_NONE);
	Click(Second);
	TestNull(TEXT("Off-turn clicks cannot select a new card"), Layout->GetSelectedCard());
	TestEqual(TEXT("Off-turn clicks leave hand contents unchanged"), LogicalHand->GetCardCount(), 2);
	TestTrue(TEXT("Off-turn clicks never create an activation pair"), LogicalHand->GetLogicalActivationPairs().IsEmpty());

	GS->SetCurrentPlayer(LocalPS);
	Click(Second);
	TestEqual(TEXT("Selection still works when the player's turn resumes"), Layout->GetSelectedCard(), Second);
	ASHCard* Inserted = MakeCard(LogicalHand, 0);
	TestEqual(TEXT("Replication-like insertion follows selected card identity instead of stale index"), VisualHand->GetSelectedHandCardIndex(), 2);
	Click(Second);
	TestNull(TEXT("Own-turn same-card click toggles selection off as well"), Layout->GetSelectedCard());

	Click(First);
	GS->SetCurrentPlayer(OtherPS);
	Click(Foreign);
	TestEqual(TEXT("Another player's card cannot deselect or replace local selection"), Layout->GetSelectedCard(), First);
	LogicalHand->RemoveCard(First); OtherHand->AddCard(First, OtherHand->GetCardCount());
	VisualHand->UpdateCardPositions();
	TestNull(TEXT("A transferred card loses its old local selection"), Layout->GetSelectedCard());
	TestEqual(TEXT("Transfer cannot select the replacement at the old array position"), VisualHand->GetSelectedHandCardIndex(), INDEX_NONE);

	GS->SetCurrentPlayer(LocalPS); Click(Second);
	Layout->SetDraggedCard(Second);
	TestNull(TEXT("Dragging clears the correct visual seat's selection"), Layout->GetSelectedCard());
	Layout->SetDraggedCard(nullptr);
	Layout->SetSelectedCardIndex(LogicalHand->GetCards().IndexOfByKey(Inserted));
	Inserted->SetCardZone(ECardZone::Activation);
	TestNull(TEXT("A card leaving the hand cannot remain selected"), Layout->GetSelectedCard());
	Inserted->SetCardZone(ECardZone::Hand);
	Layout->SetSelectedCardIndex(LogicalHand->GetCards().IndexOfByKey(Inserted));
	VisualHand->SetRepresentedHand(OtherHand);
	TestNull(TEXT("Remapping a visual seat does not transfer its selection to a different player"), Layout->GetSelectedCard());
	World->DestroyWorld(false);
	return true;
}
#endif
