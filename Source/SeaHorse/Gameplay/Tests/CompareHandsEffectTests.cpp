#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Engine/Engine.h"
#include "Engine/DataTable.h"
#include "Engine/World.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/Pawn.h"
#include "UObject/UnrealType.h"
#include "Gameplay/Board/VictoryStack.h"
#include "Gameplay/Cards/CardDefinition.h"
#include "Gameplay/Cards/Fragments/CardEffectFragment.h"
#include "Gameplay/Cards/SHCard.h"
#include "Gameplay/Cards/Tasks/CompareHandsEffectTask.h"
#include "Gameplay/Components/TurnComponent.h"
#include "Gameplay/Components/DeckComponent.h"
#include "Gameplay/Core/SHGameMode.h"
#include "Gameplay/Core/SHGameState.h"
#include "Gameplay/Core/SHPlayerController.h"
#include "Gameplay/Core/SHPlayerState.h"
#include "Gameplay/Presentation/SHHandRevealPawn.h"
#include "Gameplay/SHHand.h"

namespace
{
struct FCompareHandsTestWorld
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	ASHGameMode* Mode = World->SpawnActor<ASHGameMode>();
	ASHGameState* State = World->SpawnActor<ASHGameState>();
	TArray<ASHPlayerController*> Controllers;
	TArray<ASHPlayerState*> Players;
	TArray<ASHHand*> Hands;

	FCompareHandsTestWorld()
	{
		GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
		World->SetGameState(State);
		Mode->GameState = State;
		FindFProperty<FObjectProperty>(UWorld::StaticClass(), TEXT("AuthorityGameMode"))
			->SetObjectPropertyValue_InContainer(World, Mode);
		UTurnComponent* Turns = NewObject<UTurnComponent>(Mode);
		FindFProperty<FObjectProperty>(ASHGameMode::StaticClass(), TEXT("TurnComponent"))
			->SetObjectPropertyValue_InContainer(Mode, Turns);
		for (int32 Seat = 0; Seat < 3; ++Seat)
		{
			ASHHand* Hand = World->SpawnActor<ASHHand>();
			Hand->SetLayoutSeatIndex(Seat);
			FindFProperty<FObjectProperty>(ASHHand::StaticClass(), TEXT("VictoryStack"))
				->SetObjectPropertyValue_InContainer(Hand, World->SpawnActor<AVictoryStack>());
			ASHPlayerController* PC = World->SpawnActor<ASHPlayerController>();
			ASHPlayerState* Player = World->SpawnActor<ASHPlayerState>();
			PC->PlayerState = Player;
			PC->SetAsLocalPlayerController();
			World->AddController(PC);
			Player->SetOwner(PC);
			Player->SetSeatIndex(Seat);
			Player->SetHand(Hand);
			Hand->SetOwner(PC);
			State->AddPlayerState(Player);
			Hands.Add(Hand);
			Controllers.Add(PC);
			Players.Add(Player);
		}
		State->SetParticipantHands(Hands);
		Turns->InitializeTurns(Players[0]);
	}

	~FCompareHandsTestWorld()
	{
		World->DestroyWorld(false);
		GEngine->DestroyWorldContext(World);
	}

	ASHCard* Card(ASHHand* Hand)
	{
		ASHCard* Result = World->SpawnActor<ASHCard>();
		Result->SetCardDefinition(UCardDefinition::StaticClass());
		Hand->AddCard(Result, Hand->GetCardCount());
		return Result;
	}

	FActivatedPair Pair()
	{
		ASHCard* A = World->SpawnActor<ASHCard>();
		ASHCard* B = World->SpawnActor<ASHCard>();
		A->SetCardDefinition(UCardDefinition::StaticClass());
		B->SetCardDefinition(UCardDefinition::StaticClass());
		A->SetOwner(Hands[0]);
		B->SetOwner(Hands[0]);
		Hands[0]->AddActivationPairToLogicalHand(A, B);
		Hands[0]->SetActivationPairState(A, B, EActivationPairState::Ready);
		return *Hands[0]->FindActivationPair(A);
	}

};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSHCompareHandsEffectTest,
	"SeaHorse.Gameplay.Effects.CompareHands",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSHCompareHandsEffectTest::RunTest(const FString& Parameters)
{
	auto Start = [](FCompareHandsTestWorld& T)
	{
		const FActivatedPair ActivePair = T.Pair();
		UCompareHandsEffectTask* Task = NewObject<UCompareHandsEffectTask>(T.Mode);
		Task->Initialize(T.Players[0], ActivePair.CardA, ActivePair.CardB, TEXT("CompareHands"));
		T.Mode->ActiveEffectTasks.Add(Task);
		Task->StartEffect();
		return Task;
	};
	UClass* Diego = LoadClass<UCardDefinition>(nullptr,
		TEXT("/Game/SeaHorse/Cards/Definitions/Card_Diego.Card_Diego_C"));
	if (TestNotNull(TEXT("Saved Diego - Brawler definition exists"), Diego))
	{
		const UCompareHandsEffectFragment* Fragment = Cast<UCompareHandsEffectFragment>(
			UCardDefinition::FindFragmentByClass(Diego, UCompareHandsEffectFragment::StaticClass()));
		if (TestNotNull(TEXT("Diego uses the two-hand comparison fragment"), Fragment))
		{
			TestEqual(TEXT("Diego uses the authoritative comparison task"),
				Fragment->EffectTaskClass.Get(), UCompareHandsEffectTask::StaticClass());
			TestTrue(TEXT("Diego reuses the hand-reveal presentation pawn"),
				Fragment->RevealPawnClass && Fragment->RevealPawnClass->IsChildOf(ASHHandRevealPawn::StaticClass()));
		}
		const UCardDefinition* CDO = Diego->GetDefaultObject<UCardDefinition>();
		TestEqual(TEXT("Diego has the requested Polish name"), CDO->CardName.ToString(), FString(TEXT("Diego – Zwadźca")));
		TestEqual(TEXT("Diego has the requested ability text"), CDO->SkillDesc.ToString(),
			FString(TEXT("Z wybranym graczem porównujecie liczbę kart w waszych taliach. Nadmiar zostaje dobrany przez gracza z ich mniejszą ilością.")));
		TestNotNull(TEXT("Diego has artwork"), CDO->CardTextrue.Get());
	}
	UDataTable* MainDeck = LoadObject<UDataTable>(nullptr, TEXT("/Game/SeaHorse/Deck/DT_Deck.DT_Deck"));
	if (TestNotNull(TEXT("Main deck data table loads"), MainDeck))
	{
		const FDeckEntry* DiegoEntry = MainDeck->FindRow<FDeckEntry>(TEXT("Diego"), TEXT("CompareHandsTest"));
		if (TestNotNull(TEXT("Main deck contains Diego"), DiegoEntry))
		{
			TestEqual(TEXT("Main deck contains a matching pair of Diego cards"), DiegoEntry->Count, 2);
			TestEqual(TEXT("Main deck row points to the Diego definition"), DiegoEntry->CardDefinition.Get(), Diego);
		}
	}

	{
		FCompareHandsTestWorld T;
		APawn* SelectedGameplayPawn = T.World->SpawnActor<APawn>();
		T.Controllers[1]->Possess(SelectedGameplayPawn);
		for (int32 Index = 0; Index < 4; ++Index) { T.Card(T.Hands[0]); }
		for (int32 Index = 0; Index < 2; ++Index) { T.Card(T.Hands[1]); }
		T.Card(T.Hands[2]);
		UCompareHandsEffectTask* Task = Start(T);
		const auto& Candidates = T.Mode->PendingParticipantSelections.FindChecked(T.Players[0]).Candidates;
		TestFalse(TEXT("Activator cannot compare with their own hand"), Candidates.Contains(T.Hands[0]));
		TestTrue(TEXT("Another connected player can be selected"), Candidates.Contains(T.Hands[1]));
		T.Mode->SubmitParticipantSelection(T.Players[0], T.Hands[1]);
		TestTrue(TEXT("Different hand sizes open a comparison session"), Task->GetSessionId().IsValid());
		TestEqual(TEXT("Larger hand is fixed at session start"), Task->LargerHand.Get(), T.Hands[0]);
		TestEqual(TEXT("Smaller hand receives cards"), Task->ReceivingHand.Get(), T.Hands[1]);
		TestEqual(TEXT("Size difference determines the exact draw count"), Task->RemainingTransfers, 2);
		TestEqual(TEXT("Both full private hands are snapshotted"),
			Task->LastLargerSnapshot.Num() + Task->LastReceivingSnapshot.Num(), 6);
		TestEqual(TEXT("Comparison does not replace the selected player's gameplay pawn"),
			T.Controllers[1]->GetPawn().Get(), SelectedGameplayPawn);
		TestNotEqual(TEXT("Selected player's presentation pawn stays unpossessed"),
			T.Controllers[1]->GetPawn().Get(), static_cast<APawn*>(Task->SelectedPawn.Get()));
		const FGuid Session = Task->GetSessionId();
		Task->AcknowledgePresentation(T.Players[0], Session);
		Task->AcknowledgePresentation(T.Players[1], Session);
		ASHCard* First = T.Hands[0]->GetCards()[1];
		const int32 LargerBefore = T.Hands[0]->GetCardCount();
		const int32 SmallerBefore = T.Hands[1]->GetCardCount();
		Task->TransferCard(T.Players[0], Session, First, 0);
		TestEqual(TEXT("Only the smaller-hand player may draw"), T.Hands[0]->GetCardCount(), LargerBefore);
		Task->TransferCard(T.Players[1], FGuid::NewGuid(), First, 0);
		TestEqual(TEXT("A stale session cannot move a card"), T.Hands[0]->GetCardCount(), LargerBefore);
		Task->TransferCard(T.Players[1], Session, First, 0);
		TestFalse(TEXT("Chosen card leaves the larger hand"), T.Hands[0]->ContainsCard(First));
		TestEqual(TEXT("Chosen card is inserted in the receiving hand"), T.Hands[1]->GetCards()[0], First);
		TestEqual(TEXT("One accepted drag reduces the remaining count"), Task->RemainingTransfers, 1);
		Task->TransferCard(T.Players[1], Session, First, 0);
		TestEqual(TEXT("A transferred card cannot be given back or transferred twice"),
			T.Hands[1]->GetCardCount(), SmallerBefore + 1);
		ASHCard* Second = T.Hands[0]->GetCards()[0];
		Task->TransferCard(T.Players[1], Session, Second, T.Hands[1]->GetCardCount());
		TestTrue(TEXT("The exact number of transfers completes the effect"), Task->IsFinished());
		TestEqual(TEXT("Final hand sizes reflect the one-way difference transfer"), T.Hands[0]->GetCardCount(), 2);
		TestEqual(TEXT("Receiving hand gains exactly the difference"), T.Hands[1]->GetCardCount(), 4);
		TestEqual(TEXT("Successful Diego ability consumes its pair"),
			T.Hands[0]->GetVictoryStack()->GetPairCount(), 1);
		TestEqual(TEXT("Completing comparison preserves the selected player's gameplay pawn"),
			T.Controllers[1]->GetPawn().Get(), SelectedGameplayPawn);
	}

	{
		FCompareHandsTestWorld T;
		T.Card(T.Hands[0]);
		T.Card(T.Hands[1]);
		UCompareHandsEffectTask* Task = Start(T);
		T.Mode->SubmitParticipantSelection(T.Players[0], T.Hands[1]);
		TestTrue(TEXT("Equal hand sizes resolve immediately"), Task->IsFinished());
		TestFalse(TEXT("Equal hand sizes do not create a presentation session"), Task->GetSessionId().IsValid());
		TestEqual(TEXT("A valid equal comparison still consumes the pair"),
			T.Hands[0]->GetVictoryStack()->GetPairCount(), 1);
	}

	{
		FCompareHandsTestWorld T;
		T.Card(T.Hands[0]);
		T.Card(T.Hands[1]);
		T.Players[1]->SetProtectedFromCardEffects(true);
		UCompareHandsEffectTask* Task = Start(T);
		const auto& Candidates = T.Mode->PendingParticipantSelections.FindChecked(T.Players[0]).Candidates;
		TestFalse(TEXT("Protected players cannot be selected"), Candidates.Contains(T.Hands[1]));
		TestTrue(TEXT("An eligible connected player remains selectable"), Candidates.Contains(T.Hands[2]));
		TestFalse(TEXT("Selection is not committed before a target is accepted"), Task->bConsumePair);
	}

	TestTrue(TEXT("Card transfer uses a reliable server RPC"),
		ASHPlayerController::StaticClass()->FindFunctionByName(TEXT("ServerTransferComparedHandCard"))
			->HasAllFunctionFlags(FUNC_Net | FUNC_NetServer | FUNC_NetReliable));
	return true;
}

#if WITH_EDITOR
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSHCompareHandsPrivacyTest,
	"SeaHorse.Gameplay.Effects.CompareHandsPrivacy",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSHCompareHandsPrivacyTest::RunTest(const FString& Parameters)
{
	TGuardValue<bool> ScriptGuard(GAllowActorScriptExecutionInEditor, true);
	UClass* CardClass = LoadClass<ASHCard>(nullptr, TEXT("/Game/SeaHorse/Cards/BP_Card.BP_Card_C"));
	UClass* Definition = LoadClass<UCardDefinition>(nullptr,
		TEXT("/Game/SeaHorse/Cards/Definitions/Card_BodgyVampireHunter.Card_BodgyVampireHunter_C"));
	if (!TestNotNull(TEXT("Actual card Blueprint"), CardClass) ||
		!TestNotNull(TEXT("Actual card definition"), Definition)) { return false; }

	// Exercise the real begin/update RPC implementations for both participant roles.
	for (int32 LargerPlayerIndex : {0, 1})
	{
		FCompareHandsTestWorld T;
		const int32 DrawingPlayerIndex = 1 - LargerPlayerIndex;
		for (int32 PlayerIndex = 0; PlayerIndex < 2; ++PlayerIndex)
		{
			ULocalPlayer* LocalPlayer = NewObject<ULocalPlayer>(GEngine);
			LocalPlayer->PlayerController = T.Controllers[PlayerIndex];
			T.Controllers[PlayerIndex]->Player = LocalPlayer;
			for (int32 Index = 0; Index < (PlayerIndex == LargerPlayerIndex ? 4 : 2); ++Index)
			{
				ASHCard* Card = T.World->SpawnActor<ASHCard>(CardClass);
				Card->SetCardDefinition(Definition);
				Card->Initialize();
				T.Hands[PlayerIndex]->AddCard(Card, Index);
			}
		}
		const FActivatedPair Pair = T.Pair();
		UCompareHandsEffectTask* Task = NewObject<UCompareHandsEffectTask>(T.Mode);
		Task->Initialize(T.Players[0], Pair.CardA, Pair.CardB, TEXT("CompareHands"));
		T.Mode->ActiveEffectTasks.Add(Task);
		Task->StartEffect();
		T.Mode->SubmitParticipantSelection(T.Players[0], T.Hands[1]);
		const FGuid Session = Task->GetSessionId();
		TestTrue(TEXT("Comparison is open"), Session.IsValid() && !Task->IsFinished());
		TestNull(TEXT("Unrelated player receives no comparison"), T.Controllers[2]->GetActiveHandRevealPawn());

		auto CheckParticipantViews = [&]()
		{
			for (int32 ViewerIndex = 0; ViewerIndex < 2; ++ViewerIndex)
			{
				ASHHandRevealPawn* Stage = T.Controllers[ViewerIndex]->GetActiveHandRevealPawn();
				if (!TestNotNull(TEXT("Participant receives their own presentation"), Stage)) { continue; }
				TArray<FSHRevealedHandCard> Entries = Stage->GetCards();
				Entries.Append(Stage->GetReceivingCards());
				const TArray<ASHCard*> Visuals = Stage->GetPresentationCards();
				TestEqual(TEXT("Hidden cards still have a selectable visual copy"), Visuals.Num(), Entries.Num());
				TestEqual(TEXT("Both hands remain complete"), Entries.Num(), 6);
				for (int32 Index = 0; Index < Entries.Num(); ++Index)
				{
					const FSHRevealedHandCard& Entry = Entries[Index];
					if (!TestNotNull(TEXT("Blind draw retains source identity"), Entry.SourceCard.Get())) { continue; }
					const bool bOwnCard = Entry.SourceCard->GetOwningHand() == T.Hands[ViewerIndex];
					UClass* ExpectedDefinition = bOwnCard ? Definition : nullptr;
					TestEqual(TEXT("RPC only delivers definitions of the viewer's own cards"),
						Entry.CardDefinition.Get(), ExpectedDefinition);
					if (Visuals.IsValidIndex(Index) && TestNotNull(TEXT("Visual card exists"), Visuals[Index]))
					{
						TestEqual(TEXT("Only the viewer's own cards show their fronts"), bool(Visuals[Index]->bFaceUp), bOwnCard);
						TestEqual(TEXT("Hidden copies cannot recover secrets from the listen host's source actor"),
							Visuals[Index]->GetKnownCardDefinition().Get(), ExpectedDefinition);
						TestFalse(TEXT("Private copies are never replicated"), Visuals[Index]->GetIsReplicated());
					}
				}
			}
		};
		CheckParticipantViews();
		ASHCard* First = T.Hands[LargerPlayerIndex]->GetCards()[0];
		Task->TransferCard(T.Players[DrawingPlayerIndex], Session, First, 0);
		TestEqual(TEXT("Drawing a face-down card still moves it to the receiving hand"),
			First->GetOwningHand(), T.Hands[DrawingPlayerIndex]);
		TestFalse(TEXT("One draw leaves the second transfer available"), Task->IsFinished());
		// The transferred card becomes visible to its new owner and hidden to its former owner.
		CheckParticipantViews();
		Task->TransferCard(T.Players[DrawingPlayerIndex], Session,
			T.Hands[LargerPlayerIndex]->GetCards()[0], 0);
		TestTrue(TEXT("Face-down presentation does not block completing the effect"), Task->IsFinished());
		for (int32 Index = 0; Index < 2; ++Index)
		{
			TestNull(TEXT("Finishing clears each participant's private view"), T.Controllers[Index]->GetActiveHandRevealPawn());
		}
	}
	return true;
}
#endif
#endif
