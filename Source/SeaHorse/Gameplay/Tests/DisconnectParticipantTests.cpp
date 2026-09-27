#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Algo/RandomShuffle.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "UObject/UnrealType.h"
#include "TimerManager.h"
#include "Gameplay/Core/SHGameMode.h"
#include "Gameplay/Core/SHGameState.h"
#include "Gameplay/Core/SHPlayerController.h"
#include "Gameplay/Core/SHPlayerState.h"
#include "Gameplay/Components/TurnComponent.h"
#include "Gameplay/Cards/SHCard.h"
#include "Gameplay/Cards/CardDefinition.h"
#include "Gameplay/Cards/Tasks/ExtendedCardEffectTasks.h"
#include "Gameplay/Board/VictoryStack.h"
#include "Gameplay/SHHand.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSHDisconnectedParticipantTest,
	"SeaHorse.Gameplay.Players.DisconnectBecomesNPC", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSHDisconnectedParticipantTest::RunTest(const FString& Parameters)
{
	for (const int32 DepartingSeat : {0, 2})
	{
		UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
		GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
		ASHGameMode* Mode = World->SpawnActor<ASHGameMode>();
		ASHGameState* State = World->SpawnActor<ASHGameState>();
		World->SetGameState(State);
		Mode->GameState = State;
		FindFProperty<FObjectProperty>(UWorld::StaticClass(), TEXT("AuthorityGameMode"))->SetObjectPropertyValue_InContainer(World, Mode);
		Mode->TurnComponent = NewObject<UTurnComponent>(Mode);
		TArray<ASHHand*> Hands;
		TArray<ASHPlayerState*> Players;
		TArray<ASHPlayerController*> Controllers;
		for (int32 Seat = 0; Seat < 6; ++Seat)
		{
			ASHHand* Hand = World->SpawnActor<ASHHand>();
			Hand->SetLayoutSeatIndex(Seat);
			Hands.Add(Hand);
			if (Seat < 5)
			{
				ASHPlayerController* PC = World->SpawnActor<ASHPlayerController>();
				ASHPlayerState* Player = World->SpawnActor<ASHPlayerState>();
				PC->PlayerState = Player;
				Player->SetOwner(PC);
				Hand->SetOwner(PC);
				Player->SetHand(Hand);
				Player->SetSeatIndex(Seat);
				Players.Add(Player);
				Controllers.Add(PC);
				State->AddPlayerState(Player);
			}
			else { Hand->SetIsNPC(true); }
			for (int32 Index = 0; Index < 4; ++Index)
			{
				ASHCard* Card = World->SpawnActor<ASHCard>();
				Card->SetCardDefinition(UCardDefinition::StaticClass());
				Hand->AddCard(Card, Hand->GetCardCount());
			}
		}
		State->SetParticipantHands(Hands);
		Mode->TurnComponent->InitializeTurns(Players[0]);
		State->SetTurnPhase(ETurnPhase::DrawCard);
		ASHHand* DepartingHand = Hands[DepartingSeat];
		ASHPlayerState* DepartingPlayer = Players[DepartingSeat];
		auto AddPair = [&]()
		{
			ASHCard* A = World->SpawnActor<ASHCard>();
			ASHCard* B = World->SpawnActor<ASHCard>();
			A->SetCardDefinition(UCardDefinition::StaticClass()); B->SetCardDefinition(UCardDefinition::StaticClass());
			A->SetOwner(DepartingHand); B->SetOwner(DepartingHand);
			DepartingHand->AddActivationPairToLogicalHand(A, B);
			A->Reveal(); B->Reveal();
			DepartingHand->SetActivationPairState(A, B, EActivationPairState::Ready);
			return A;
		};
		ASHCard* PairA = AddPair();
		ASHCard* PairB = DepartingHand->FindActivationPair(PairA)->CardB;
		AddPair();
		AVictoryStack* Victory = World->SpawnActor<AVictoryStack>();
		FindFProperty<FObjectProperty>(ASHHand::StaticClass(), TEXT("VictoryStack"))->SetObjectPropertyValue_InContainer(DepartingHand, Victory);
		Victory->AddPair(World->SpawnActor<ASHCard>(), World->SpawnActor<ASHCard>());
		TArray<ASHCard*> Expected = DepartingHand->GetCards();
		for (const FActivatedPair& Pair : DepartingHand->GetLogicalActivationPairs()) { Expected.Add(Pair.CardA); Expected.Add(Pair.CardB); }

		UDrawTwoReturnOneEffectTask* DrawTask = NewObject<UDrawTwoReturnOneEffectTask>(Mode);
		DrawTask->Initialize(DepartingPlayer, PairA, PairB, NAME_None);
		Mode->ActiveEffectTasks.Add(DrawTask);
		if (DepartingSeat == 0)
		{
			Mode->TurnComponent->ScheduleAdditionalDraw(DrawTask, DepartingPlayer, EAdditionalDrawSourceRule::SamePlayer);
		}
		Mode->TurnComponent->RegisterPendingPairSettlement(PairA, PairB);
		Mode->PendingPairActivations.Add({DepartingPlayer, PairA, PairB});
		Mode->CompletedEffectPairsWaitingForPresentation.Add({DepartingPlayer, PairA, PairB});
		FTimerHandle DelayedFinish;
		World->GetTimerManager().SetTimer(DelayedFinish, FTimerDelegate::CreateWeakLambda(DrawTask, [DrawTask]() { DrawTask->FinishEffect(); }), 0.1f, false);
		FMath::RandInit(77133);
		Algo::RandomShuffle(Expected);
		FMath::RandInit(77133);
		Mode->Logout(Controllers[DepartingSeat]);
		for (int32 Step = 0; Step < 4; ++Step) { ++GFrameCounter; World->GetTimerManager().Tick(0.1f); }

		TestTrue(TEXT("The vacated logical hand becomes a BN"), DepartingHand->IsLogicalNPC());
		TestEqual(TEXT("The participant seat stays in place"), State->FindParticipantHandBySeat(DepartingSeat), DepartingHand);
		TestEqual(TEXT("Seat count stays fixed"), State->GetParticipantCount(), 6);
		TestEqual(TEXT("Exactly one extra BN appears"), State->GetNPCHands().Num(), 2);
		TestTrue(TEXT("Hand and activation cards are shuffled together"), DepartingHand->GetCards() == Expected);
		TestTrue(TEXT("No activation pairs remain on the vacated seat"), DepartingHand->GetLogicalActivationPairs().IsEmpty());
		TestNull(TEXT("Disconnected player's hand reference is released"), DepartingPlayer->GetHand());
		TestNull(TEXT("The stack has no disconnected network owner"), DepartingHand->GetOwner());
		TestFalse(TEXT("The departing player leaves the human turn roster"), State->PlayerArray.Contains(DepartingPlayer));
		TestEqual(TEXT("Scored pairs stay in the victory stack"), Victory->GetPairCount(), 1);
		for (ASHCard* Card : Expected)
		{
			TestEqual(TEXT("Converted card belongs to the same logical container"), Card->GetOwningHand(), DepartingHand);
			TestEqual(TEXT("Converted activation cards enter the hand zone"), Card->GetCardZone(), ECardZone::Hand);
			TestNull(TEXT("Replacement BN does not retain a public reveal"),
				FindFProperty<FClassProperty>(ASHCard::StaticClass(), TEXT("RevealedCardDefinition"))->GetObjectPropertyValue_InContainer(Card));
			Card->OnRep_RevealedCardDefinition();
			TestFalse(TEXT("Late reveal replication keeps replacement BN face down"), Card->bFaceUp);
		}
		TestTrue(TEXT("Detached task cannot finish or resume later"), DrawTask->IsFinished());
		TestFalse(TEXT("No active effect remains owned by the departing player"), Mode->HasActiveEffectTasks());
		TestTrue(TEXT("Pending pair work is discarded"), Mode->PendingPairActivations.IsEmpty() && Mode->CompletedEffectPairsWaitingForPresentation.IsEmpty());
		TestFalse(TEXT("No orphaned pair settlement blocks turns"), Mode->TurnComponent->HasUnsettledPairs());
		TestEqual(TEXT("Current departure advances to the next human; other departure preserves turn"), State->GetCurrentPlayer(), Players[DepartingSeat == 0 ? 1 : 0]);
		TestEqual(TEXT("Current departure begins a fresh turn; another departure preserves phase"), State->GetTurnPhase(), DepartingSeat == 0 ? ETurnPhase::FirstPairing : ETurnPhase::DrawCard);
		World->DestroyWorld(false);
		GEngine->DestroyWorldContext(World);
	}
	{
		// Exercise the replicated mode-change presentation with a rotated local
		// table and the departed PlayerState's hand already cleared.
		UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
		GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
		ASHGameState* State = World->SpawnActor<ASHGameState>();
		World->SetGameState(State);
		ASHPlayerController* PC = World->SpawnActor<ASHPlayerController>();
		PC->SetAsLocalPlayerController();
		World->AddController(PC);
		ASHPlayerState* LocalPlayer = World->SpawnActor<ASHPlayerState>();
		ASHPlayerState* DepartingPlayer = World->SpawnActor<ASHPlayerState>();
		PC->PlayerState = LocalPlayer;
		LocalPlayer->SetOwner(PC);
		LocalPlayer->SetSeatIndex(2);
		DepartingPlayer->SetSeatIndex(3);
		TArray<ASHHand*> Hands;
		for (int32 Seat = 0; Seat < 4; ++Seat)
		{
			ASHHand* Hand = World->SpawnActor<ASHHand>();
			Hand->SetLayoutSeatIndex(Seat);
			Hands.Add(Hand);
		}
		LocalPlayer->SetHand(Hands[2]);
		DepartingPlayer->SetHand(Hands[3]);
		State->AddPlayerState(LocalPlayer);
		State->AddPlayerState(DepartingPlayer);
		State->SetParticipantHands(Hands);
		for (int32 VisualSeat = 0; VisualSeat < 4; ++VisualSeat) { Hands[VisualSeat]->SetRepresentedHand(Hands[(VisualSeat + 2) % 4]); }
		ASHHand* DepartingVisualSlot = Hands[1];
		DepartingVisualSlot->SetRepresentedPlayerState(DepartingPlayer);
		ASHCard* Card = World->SpawnActor<ASHCard>();
		Card->SetCardDefinition(UCardDefinition::StaticClass());
		Hands[3]->AddCard(Card, 0);
		Card->SetFaceUp(true);
		DepartingPlayer->SetHand(nullptr);
		Hands[3]->SetIsNPC(true); // Runs the same OnRep_IsNPC hook as a client.
		TestEqual(TEXT("Conversion rebinds the correct rotated visual slot"), PC->FindVisualHandForLogicalHand(Hands[3]), DepartingVisualSlot);
		TestNull(TEXT("Rotated slot no longer points at the departing PlayerState"), DepartingVisualSlot->GetRepresentedPlayerState());
		TestEqual(TEXT("Rotated slot retains the authoritative logical BN hand"), DepartingVisualSlot->GetRepresentedHand(), Hands[3]);
		TestTrue(TEXT("Blueprint queries on the rotated visual slot see BN mode"), DepartingVisualSlot->IsNPC());
		TestFalse(TEXT("Converted stack presentation hides formerly visible hand cards"), Card->bFaceUp);
		TestEqual(TEXT("Local player's visual seat remains unchanged"), Hands[0]->GetRepresentedHand(), Hands[2]);
		TestEqual(TEXT("Other visual seats keep their mapping"), Hands[2]->GetRepresentedHand(), Hands[0]);
		World->DestroyWorld(false);
		GEngine->DestroyWorldContext(World);
	}
	return true;
}
#endif
