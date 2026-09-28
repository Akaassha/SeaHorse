#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "TimerManager.h"
#include "UObject/UnrealType.h"
#include "Gameplay/Board/VictoryStack.h"
#include "Gameplay/Cards/CardDefinition.h"
#include "Gameplay/Cards/Fragments/CardEffectFragment.h"
#include "Gameplay/Cards/SHCard.h"
#include "Gameplay/Cards/Tasks/RevealHandEffectTask.h"
#include "Gameplay/Components/TurnComponent.h"
#include "Gameplay/Core/SHGameMode.h"
#include "Gameplay/Core/SHGameState.h"
#include "Gameplay/Core/SHPlayerController.h"
#include "Gameplay/Core/SHPlayerState.h"
#include "Gameplay/Presentation/SHHandRevealPawn.h"
#include "Gameplay/Presentation/HandRevealWidget.h"
#include "Gameplay/SHHand.h"

namespace
{
struct FHandRevealTestWorld
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	ASHGameMode* Mode = World->SpawnActor<ASHGameMode>();
	ASHGameState* State = World->SpawnActor<ASHGameState>();
	TArray<ASHPlayerController*> Controllers;
	TArray<ASHPlayerState*> Players;
	TArray<ASHHand*> Hands;

	FHandRevealTestWorld()
	{
		GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
		World->SetGameState(State);
		Mode->GameState = State;
		FindFProperty<FObjectProperty>(UWorld::StaticClass(), TEXT("AuthorityGameMode"))
			->SetObjectPropertyValue_InContainer(World, Mode);
		UTurnComponent* Turns = NewObject<UTurnComponent>(Mode);
		FindFProperty<FObjectProperty>(ASHGameMode::StaticClass(), TEXT("TurnComponent"))
			->SetObjectPropertyValue_InContainer(Mode, Turns);
		for (int32 Seat = 0; Seat < 4; ++Seat)
		{
			ASHHand* Hand = World->SpawnActor<ASHHand>();
			Hand->SetLayoutSeatIndex(Seat);
			FindFProperty<FObjectProperty>(ASHHand::StaticClass(), TEXT("VictoryStack"))
				->SetObjectPropertyValue_InContainer(Hand, World->SpawnActor<AVictoryStack>());
			Hands.Add(Hand);
			if (Seat < 3)
			{
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
				Controllers.Add(PC);
				Players.Add(Player);
			}
			else { Hand->SetIsNPC(true); }
			Card(Hand);
		}
		State->SetParticipantHands(Hands);
		Turns->InitializeTurns(Players[0]);
	}
	~FHandRevealTestWorld()
	{
		World->DestroyWorld(false);
		GEngine->DestroyWorldContext(World);
	}
	ASHCard* Card(ASHHand* Hand, UClass* Definition = UCardDefinition::StaticClass())
	{
		ASHCard* Result = World->SpawnActor<ASHCard>();
		Result->SetCardDefinition(Definition);
		Hand->AddCard(Result, Hand->GetCardCount());
		return Result;
	}
	FActivatedPair Pair()
	{
		ASHHand* Hand = Hands[0];
		ASHCard* A = World->SpawnActor<ASHCard>();
		ASHCard* B = World->SpawnActor<ASHCard>();
		A->SetCardDefinition(UCardDefinition::StaticClass());
		B->SetCardDefinition(UCardDefinition::StaticClass());
		A->SetOwner(Hand);
		B->SetOwner(Hand);
		Hand->AddActivationPairToLogicalHand(A, B);
		Hand->SetActivationPairState(A, B, EActivationPairState::Ready);
		return *Hand->FindActivationPair(A);
	}
	void Advance(float Seconds = 0.5f)
	{
		for (int32 Step = 0; Step < FMath::CeilToInt(Seconds / 0.1f); ++Step)
		{
			++GFrameCounter;
			World->GetTimerManager().Tick(0.1f);
		}
	}
	int32 RevealPawnCount() const
	{
		int32 Count = 0;
		for (TActorIterator<ASHHandRevealPawn> It(World); It; ++It)
		{
			if (IsValid(*It) && !It->IsActorBeingDestroyed()) { ++Count; }
		}
		return Count;
	}
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSHHandRevealTest, "SeaHorse.Gameplay.Effects.PrivateHandReveal",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSHHandRevealTest::RunTest(const FString& Parameters)
{
	UClass* Bodgy = LoadClass<UCardDefinition>(nullptr,
		TEXT("/Game/SeaHorse/Cards/Definitions/Card_BodgyShaoLiMaster.Card_BodgyShaoLiMaster_C"));
	if (!TestNotNull(TEXT("Saved Bodgy Shao Li Master definition exists"), Bodgy)) { return false; }
	const URevealHandEffectFragment* RevealFragment = Cast<URevealHandEffectFragment>(
		UCardDefinition::FindFragmentByClass(Bodgy, URevealHandEffectFragment::StaticClass()));
	if (!TestNotNull(TEXT("Saved definition contains the private hand-reveal fragment"), RevealFragment)) { return false; }
	TestEqual(TEXT("Saved Bodgy definition uses the authoritative reveal task"),
		RevealFragment->EffectTaskClass.Get(), URevealHandEffectTask::StaticClass());
	TestTrue(TEXT("Saved Bodgy definition configures a reveal-pawn class"),
		RevealFragment->RevealPawnClass && RevealFragment->RevealPawnClass->IsChildOf(ASHHandRevealPawn::StaticClass()));
	TestTrue(TEXT("Saved Bodgy definition configures a reveal-widget class"),
		RevealFragment->RevealWidgetClass && RevealFragment->RevealWidgetClass->IsChildOf(UHandRevealWidget::StaticClass()));
	TestNotNull(TEXT("Saved Bodgy definition includes its card artwork"), Bodgy->GetDefaultObject<UCardDefinition>()->CardTextrue.Get());
	UClass* SeaHorse = LoadClass<UCardDefinition>(nullptr,
		TEXT("/Game/SeaHorse/Cards/Definitions/Card_SeaHorse.Card_SeaHorse_C"));
	if (!TestNotNull(TEXT("Sea Horse definition exists"), SeaHorse)) { return false; }
	auto Start = [](FHandRevealTestWorld& T, bool bRepeat = false)
	{
		const FActivatedPair Pair = T.Pair();
		URevealHandEffectTask* Task = NewObject<URevealHandEffectTask>(T.Mode);
		Task->Initialize(T.Players[0], Pair.CardA, Pair.CardB, TEXT("RevealHand"));
		if (bRepeat) { Task->SetRepeatedExecution(); }
		T.Mode->ActiveEffectTasks.Add(Task);
		Task->StartEffect();
		return Task;
	};
	auto PublicDefinition = [](ASHCard* Card)
	{
		return FindFProperty<FClassProperty>(ASHCard::StaticClass(), TEXT("RevealedCardDefinition"))
			->GetObjectPropertyValue_InContainer(Card);
	};
	auto AcknowledgeBoth = [](FHandRevealTestWorld& T, URevealHandEffectTask* Task)
	{
		T.Mode->AcknowledgeHandReveal(T.Players[0], Task->GetSessionId());
		if (IsValid(Task->TargetPlayer)) { T.Mode->AcknowledgeHandReveal(Task->TargetPlayer, Task->GetSessionId()); }
	};
	{
		FHandRevealTestWorld T;
		ASHHand* Source = T.Hands[1];
		T.Card(Source, SeaHorse);
		ASHCard* MovedCard = T.Card(Source);
		const TArray<ASHCard*> Before = Source->GetCards();
		APawn* OriginalViewerPawn = T.World->SpawnActor<APawn>();
		APawn* OriginalTargetPawn = T.World->SpawnActor<APawn>();
		T.Controllers[0]->Possess(OriginalViewerPawn);
		T.Controllers[1]->Possess(OriginalTargetPawn);
		AActor* OriginalTargetView = T.Controllers[1]->GetViewTarget();
		URevealHandEffectTask* Task = Start(T);
		TestFalse(TEXT("The activator cannot target their own hand"),
			T.Mode->PendingParticipantSelections.FindChecked(T.Players[0]).Candidates.Contains(T.Hands[0]));
		TestTrue(TEXT("A nonempty BN can be selected"),
			T.Mode->PendingParticipantSelections.FindChecked(T.Players[0]).Candidates.Contains(T.Hands[3]));
		T.Mode->SubmitParticipantSelection(T.Players[0], Source);
		T.Advance();
		const FGuid Session = Task->GetSessionId();
		TestTrue(TEXT("An accepted target starts a unique private session"), Session.IsValid());
		TestEqual(TEXT("The source is the selected logical hand"), Task->GetRevealedHand(), Source);
		TestTrue(TEXT("Activator belongs to the session"), Task->IsSessionFor(T.Players[0], Session));
		TestTrue(TEXT("Showing player belongs to the session"), Task->IsSessionFor(T.Players[1], Session));
		TestFalse(TEXT("Unrelated player cannot use the session"), Task->IsSessionFor(T.Players[2], Session));
		TestFalse(TEXT("A stale session id never grants access"), Task->IsSessionFor(T.Players[0], FGuid::NewGuid()));
		TestTrue(TEXT("The viewer is blocked from ordinary gameplay while viewing"), T.Mode->IsPlayerInHandReveal(T.Players[0]));
		TestTrue(TEXT("The showing player is blocked from ordinary gameplay while viewing"), T.Mode->IsPlayerInHandReveal(T.Players[1]));
		TestFalse(TEXT("An unrelated player has no private session"), T.Mode->IsPlayerInHandReveal(T.Players[2]));
		TestTrue(TEXT("Showing cards does not move them between hands"), Source->GetCards() == Before);
		TestEqual(TEXT("The private snapshot contains the complete selected hand"), Task->LastSnapshot.Num(), Before.Num());
		for (int32 Index = 0; Index < FMath::Min(Task->LastSnapshot.Num(), Before.Num()); ++Index)
		{
			TestEqual(TEXT("Snapshot preserves authoritative card identity"), Task->LastSnapshot[Index].SourceCard.Get(), Before[Index]);
			TestEqual(TEXT("Snapshot contains the full definition, including text"), Task->LastSnapshot[Index].CardDefinition.Get(), Before[Index]->CardDefinition.Get());
		}
		for (ASHCard* Card : Before)
		{
			TestNull(TEXT("Private showing does not set public disclosure"), PublicDefinition(Card));
			TestEqual(TEXT("Cards keep their owner"), Card->GetOwningHand(), Source);
			TestEqual(TEXT("Cards remain in the hand zone"), Card->GetCardZone(), ECardZone::Hand);
			TestNotNull(TEXT("The original owner still has inspection permission"), Card->GetInspectableDefinition(T.Controllers[1]).Get());
			TestNull(TEXT("Third player cannot inspect originals even on the listen server"), Card->GetInspectableDefinition(T.Controllers[2]).Get());
		}
		ASHHandRevealPawn* ViewerPawn = Task->ViewerPawn;
		ASHHandRevealPawn* TargetPawn = Task->TargetPawn;
		if (TestNotNull(TEXT("Showing player's temporary pawn exists"), TargetPawn))
		{
			TestEqual(TEXT("Showing player possesses the temporary pawn"), T.Controllers[1]->GetPawn().Get(), static_cast<APawn*>(TargetPawn));
			TestEqual(TEXT("Showing pawn belongs to its controller"), TargetPawn->GetOwner(), static_cast<AActor*>(T.Controllers[1]));
			TestTrue(TEXT("Showing pawn is relevant only to its owner"), TargetPawn->bOnlyRelevantToOwner);
		}
		if (TestNotNull(TEXT("Activator's private presentation pawn exists"), ViewerPawn))
		{
			TestEqual(TEXT("Viewer pawn belongs to the activator"), ViewerPawn->GetOwner(), static_cast<AActor*>(T.Controllers[0]));
			TestTrue(TEXT("Viewer pawn is relevant only to its owner"), ViewerPawn->bOnlyRelevantToOwner);
			TestTrue(TEXT("Unpossessed viewer pawn opens its private network channel through its controller"),
				ViewerPawn->GetNetOwner() == T.Controllers[0]);
		}
		TestFalse(TEXT("Private pawn snapshots are never replicated as shared properties"),
			FindFProperty<FArrayProperty>(ASHHandRevealPawn::StaticClass(), TEXT("RevealedCards"))->HasAnyPropertyFlags(CPF_Net));
		TestTrue(TEXT("Definitions are delivered through reliable owner-client RPC"),
			ASHPlayerController::StaticClass()->FindFunctionByName(TEXT("ClientBeginHandReveal"))
				->HasAllFunctionFlags(FUNC_Net | FUNC_NetClient | FUNC_NetReliable));
		T.Mode->ReorderRevealedHand(T.Players[1], Session, MovedCard, 0);
		TestTrue(TEXT("A showing player cannot reorder before their presentation is ready"), Source->GetCards() == Before);
		AcknowledgeBoth(T, Task);
		for (ASHPlayerState* Unauthorized : {T.Players[0], T.Players[2]})
		{
			T.Mode->ReorderRevealedHand(Unauthorized, Session, MovedCard, 0);
			TestTrue(TEXT("Only the showing player may change the order"), Source->GetCards() == Before);
		}
		T.Mode->ReorderRevealedHand(T.Players[1], FGuid::NewGuid(), MovedCard, 0);
		T.Mode->ReorderRevealedHand(T.Players[1], Session, T.Hands[0]->GetCards()[0], 0);
		T.Mode->ReorderRevealedHand(T.Players[1], Session, MovedCard, -1);
		T.Mode->ReorderRevealedHand(T.Players[1], Session, MovedCard, Source->GetCardCount() + 1);
		TestTrue(TEXT("Stale sessions, foreign cards and invalid indices cannot reorder"), Source->GetCards() == Before);
		T.Mode->ReorderRevealedHand(T.Players[1], Session, MovedCard, 0);
		TestEqual(TEXT("Owner with Sea Horse can reorder the real hand"), Source->GetCards()[0], MovedCard);
		TestEqual(TEXT("Reordering preserves the number of cards"), Source->GetCardCount(), Before.Num());
		if (TestFalse(TEXT("Reordered snapshot is not empty"), Task->LastSnapshot.IsEmpty()))
		{
			TestEqual(TEXT("A reorder refreshes the shared private snapshot immediately"), Task->LastSnapshot[0].SourceCard.Get(), MovedCard);
		}
		T.Mode->FinishHandReveal(T.Players[1], Session);
		T.Mode->FinishHandReveal(T.Players[2], Session);
		T.Mode->FinishHandReveal(T.Players[0], FGuid::NewGuid());
		TestFalse(TEXT("Only the viewer with the current id can finish showing"), Task->IsFinished());
		T.Mode->FinishHandReveal(T.Players[0], Session);
		T.Advance();
		TestTrue(TEXT("Viewer completes the effect"), Task->IsFinished());
		TestFalse(TEXT("Completed session no longer authorizes either participant"), Task->IsSessionFor(T.Players[0], Session));
		TestFalse(TEXT("Completion releases the activation queue"), T.Mode->HasActiveEffectTasks());
		TestEqual(TEXT("Showing player's previous pawn is restored"), T.Controllers[1]->GetPawn().Get(), OriginalTargetPawn);
		TestEqual(TEXT("Showing player's previous view target is restored"), T.Controllers[1]->GetViewTarget(), OriginalTargetView);
		TestEqual(TEXT("Viewer's previous pawn is preserved or restored"), T.Controllers[0]->GetPawn().Get(), OriginalViewerPawn);
		TestEqual(TEXT("Completion leaves no temporary presentation pawns"), T.RevealPawnCount(), 0);
		TestTrue(TEXT("Completion clears private snapshot references"), Task->LastSnapshot.IsEmpty());
		TestEqual(TEXT("An acknowledged reveal consumes the pair once"), T.Hands[0]->GetVictoryStack()->GetPairCount(), 1);
		for (ASHCard* Card : Before) { TestNull(TEXT("Completion does not leave public disclosure behind"), PublicDefinition(Card)); }
		URevealHandEffectTask* Repeat = Start(T, true);
		T.Mode->SubmitParticipantSelection(T.Players[0], Source);
		TestTrue(TEXT("A repeated effect obtains a fresh session id"), Repeat->GetSessionId().IsValid() && Repeat->GetSessionId() != Session);
		AcknowledgeBoth(T, Repeat);
		Repeat->FinishViewing(T.Players[0], Session);
		TestFalse(TEXT("An old close request cannot finish Pancho's repeat"), Repeat->IsFinished());
		Repeat->FinishViewing(T.Players[0], Repeat->GetSessionId());
		TestEqual(TEXT("The repeated effect also cleans up its pawns"), T.RevealPawnCount(), 0);
	}
	for (const int32 TargetSeat : {1, 3})
	{
		FHandRevealTestWorld T;
		ASHHand* Source = T.Hands[TargetSeat];
		ASHCard* Card = T.Card(Source);
		const TArray<ASHCard*> Before = Source->GetCards();
		URevealHandEffectTask* Task = Start(T);
		T.Mode->SubmitParticipantSelection(T.Players[0], Source);
		const FGuid Session = Task->GetSessionId();
		TestTrue(TEXT("A human without Sea Horse and a BN can still be shown"), Session.IsValid());
		AcknowledgeBoth(T, Task);
		for (ASHPlayerState* Player : T.Players) { Task->Reorder(Player, Session, Card, 0); }
		TestTrue(TEXT("Human without Sea Horse and BN remain read only"), Source->GetCards() == Before);
		if (TargetSeat == 3)
		{
			TestNull(TEXT("BN has no showing-player pawn"), Task->TargetPawn.Get());
			TestFalse(TEXT("A human cannot impersonate a BN's owner"), Task->IsSessionFor(T.Players[1], Session));
		}
		Task->FinishViewing(T.Players[0], Session);
		TestEqual(TEXT("Read-only showing cleans up its pawn"), T.RevealPawnCount(), 0);
	}
	{
		FHandRevealTestWorld T;
		T.Players[1]->SetProtectedFromCardEffects(true);
		for (ASHCard* Card : T.Hands[2]->GetCards()) { T.Hands[2]->RemoveCard(Card); }
		URevealHandEffectTask* Task = Start(T);
		const auto& Candidates = T.Mode->PendingParticipantSelections.FindChecked(T.Players[0]).Candidates;
		TestFalse(TEXT("Protected hand cannot be targeted"), Candidates.Contains(T.Hands[1]));
		TestFalse(TEXT("Empty hand cannot be targeted"), Candidates.Contains(T.Hands[2]));
		TestTrue(TEXT("An eligible BN is still available"), Candidates.Contains(T.Hands[3]));
		TestTrue(TEXT("Uncommitted target selection may be cancelled"),
			T.Mode->CancelEffectTargetSelection(T.Players[0], Task->GetCardA(), Task->GetCardB()));
		TestTrue(TEXT("Cancellation closes the effect"), Task->IsFinished());
		TestFalse(TEXT("Cancelled selection does not start a session"), Task->GetSessionId().IsValid());
		TestEqual(TEXT("Cancellation creates no temporary pawns"), T.RevealPawnCount(), 0);
	}
	{
		FHandRevealTestWorld T;
		for (int32 Seat = 1; Seat < T.Hands.Num(); ++Seat)
		{
			for (ASHCard* Card : T.Hands[Seat]->GetCards()) { T.Hands[Seat]->RemoveCard(Card); }
		}
		URevealHandEffectTask* Task = Start(T);
		TestTrue(TEXT("No eligible target completes without waiting forever"), Task->IsFinished());
		TestFalse(TEXT("No eligible target leaves no pending selection"), T.Mode->IsWaitingForPlayerSelection());
		TestEqual(TEXT("An unsuccessful reveal retains its pair"),
			Task->GetPairDisposition(), ECardEffectPairDisposition::KeepOnTable);
		TestEqual(TEXT("An unsuccessful reveal spawns no presentation pawns"), T.RevealPawnCount(), 0);
	}
	{
		FHandRevealTestWorld T;
		ASHHand* Source = T.Hands[1];
		ASHCard* SeaHorseCard = T.Card(Source, SeaHorse);
		ASHCard* Movable = T.Card(Source);
		URevealHandEffectTask* Task = Start(T);
		T.Mode->SubmitParticipantSelection(T.Players[0], Source);
		TestTrue(TEXT("A Sea Horse initially grants rearrangement permission"), Task->bLastCanReorder);
		AcknowledgeBoth(T, Task);
		Source->RemoveCard(SeaHorseCard);
		T.Hands[2]->AddCard(SeaHorseCard, T.Hands[2]->GetCardCount());
		const TArray<ASHCard*> Before = Source->GetCards();
		Task->Reorder(T.Players[1], Task->GetSessionId(), Movable, 0);
		TestTrue(TEXT("Reordering permission is checked against the current hand"), Source->GetCards() == Before);
		T.Advance();
		TestFalse(TEXT("Loss of Sea Horse refreshes private presentation permissions"), Task->bLastCanReorder);
		TestEqual(TEXT("Snapshots stop exposing a card that left the shown hand"), Task->LastSnapshot.Num(), Before.Num());
		Task->FinishViewing(T.Players[0], Task->GetSessionId());
	}
	for (const int32 DepartingSeat : {0, 1})
	{
		FHandRevealTestWorld T;
		APawn* Original = T.World->SpawnActor<APawn>();
		T.Controllers[1]->Possess(Original);
		URevealHandEffectTask* Task = Start(T);
		T.Mode->SubmitParticipantSelection(T.Players[0], T.Hands[1]);
		Task->HandleParticipantDisconnected(T.Players[2]);
		TestFalse(TEXT("An unrelated disconnect does not dismiss private showing"), Task->IsFinished());
		Task->HandleParticipantDisconnected(T.Players[DepartingSeat]);
		T.Advance();
		TestTrue(TEXT("Either participant disconnecting closes the session"), Task->IsFinished());
		TestEqual(TEXT("Disconnect cleanup leaves no presentation pawns"), T.RevealPawnCount(), 0);
		if (DepartingSeat == 0)
		{
			TestEqual(TEXT("Surviving showing player regains the original pawn"), T.Controllers[1]->GetPawn().Get(), Original);
		}
	}
	{
		FHandRevealTestWorld T;
		APawn* Original = T.World->SpawnActor<APawn>();
		T.Controllers[1]->Possess(Original);
		URevealHandEffectTask* Task = Start(T);
		T.Mode->SubmitParticipantSelection(T.Players[0], T.Hands[1]);
		Task->AbandonEffect();
		TestTrue(TEXT("Server teardown invalidates the task"), Task->IsFinished());
		TestEqual(TEXT("Abandonment cleans up every presentation pawn"), T.RevealPawnCount(), 0);
		TestEqual(TEXT("Abandonment restores previous possession"), T.Controllers[1]->GetPawn().Get(), Original);
		T.Mode->ActiveEffectTasks.Remove(Task);
	}
	{
		FHandRevealTestWorld T;
		URevealHandEffectTask* Task = Start(T);
		T.Mode->SubmitParticipantSelection(T.Players[0], T.Hands[1]);
		if (TestNotNull(TEXT("A session pawn exists before destruction"), Task->ViewerPawn.Get()))
		{
			Task->ViewerPawn->Destroy();
			T.Advance();
			TestTrue(TEXT("Losing a presentation pawn cannot leave the turn blocked"), Task->IsFinished());
			TestEqual(TEXT("Losing one session pawn also removes the other"), T.RevealPawnCount(), 0);
		}
	}
	{
		// Simulate a first RPC arriving before its pawn channel: neither client
		// acknowledges until after the server has retried the initial snapshot.
		FHandRevealTestWorld T;
		URevealHandEffectTask* Task = Start(T);
		T.Mode->SubmitParticipantSelection(T.Players[0], T.Hands[1]);
		const FGuid Session = Task->GetSessionId();
		ASHHandRevealPawn* ViewerPawn = Task->ViewerPawn;
		ASHHandRevealPawn* TargetPawn = Task->TargetPawn;
		T.Advance(2.f);
		TestFalse(TEXT("An unmapped initial pawn does not abandon the session"), Task->IsFinished());
		TestTrue(TEXT("Awaiting presentation still holds the activation queue"), T.Mode->HasActiveEffectTasks());
		TestEqual(TEXT("Presentation retry preserves the session identity"), Task->GetSessionId(), Session);
		TestEqual(TEXT("Presentation retry reuses the viewer pawn"), Task->ViewerPawn.Get(), ViewerPawn);
		TestEqual(TEXT("Presentation retry reuses the target pawn"), Task->TargetPawn.Get(), TargetPawn);
		TestEqual(TEXT("Retries do not accumulate new private pawns"), T.RevealPawnCount(), 2);
		TestEqual(TEXT("An unacknowledged snapshot retains the authoritative hand"),
			Task->LastSnapshot.Num(), T.Hands[1]->GetCardCount());
		TestEqual(TEXT("An unopened view cannot consume the ability"),
			Task->GetPairDisposition(), ECardEffectPairDisposition::KeepOnTable);
		T.Mode->AcknowledgeHandReveal(T.Players[2], Session);
		T.Mode->AcknowledgeHandReveal(T.Players[0], FGuid::NewGuid());
		T.Mode->AcknowledgeHandReveal(T.Players[1], FGuid::NewGuid());
		TestFalse(TEXT("An unrelated player and stale messages cannot acknowledge the viewer"), Task->bViewerReady);
		TestFalse(TEXT("An unrelated player and stale messages cannot acknowledge the target"), Task->bTargetReady);
		T.Mode->AcknowledgeHandReveal(T.Players[1], Session);
		TestTrue(TEXT("Showing player can acknowledge only their own presentation"), Task->bTargetReady);
		TestFalse(TEXT("Target acknowledgement does not fabricate a successful reveal"), Task->bViewerReady);
		T.Mode->AcknowledgeHandReveal(T.Players[0], Session);
		T.Mode->AcknowledgeHandReveal(T.Players[0], Session);
		TestTrue(TEXT("Viewer acknowledgement is idempotent"), Task->bViewerReady && Task->bShownToViewer);
		T.Advance(16.f);
		TestFalse(TEXT("A fully ready presentation does not expire at the startup deadline"), Task->IsFinished());
		TestEqual(TEXT("Acknowledgement does not respawn the presentation"), Task->ViewerPawn.Get(), ViewerPawn);
		T.Mode->FinishHandReveal(T.Players[0], Session);
		TestEqual(TEXT("The recovered session consumes its pair exactly once"), T.Hands[0]->GetVictoryStack()->GetPairCount(), 1);
		T.Mode->AcknowledgeHandReveal(T.Players[0], Session);
		TestEqual(TEXT("A late acknowledgement cannot resurrect a closed presentation"), T.RevealPawnCount(), 0);
	}
	for (const bool bViewerWasReady : {false, true})
	{
		FHandRevealTestWorld T;
		APawn* Original = T.World->SpawnActor<APawn>();
		T.Controllers[1]->Possess(Original);
		URevealHandEffectTask* Task = Start(T);
		T.Mode->SubmitParticipantSelection(T.Players[0], T.Hands[1]);
		const FGuid Session = Task->GetSessionId();
		// Cover both missing viewer and missing showing-player acknowledgement.
		T.Mode->AcknowledgeHandReveal(T.Players[bViewerWasReady ? 0 : 1], Session);
		T.Advance(16.f);
		TestTrue(TEXT("A client that never becomes ready cannot block the match forever"), Task->IsFinished());
		TestFalse(TEXT("The startup deadline releases the effect queue"), T.Mode->HasActiveEffectTasks());
		TestEqual(TEXT("The startup deadline clears all private pawns"), T.RevealPawnCount(), 0);
		TestEqual(TEXT("The startup deadline restores the showing player's pawn"), T.Controllers[1]->GetPawn().Get(), Original);
		TestEqual(TEXT("Only an ability actually shown to the viewer is consumed on timeout"),
			Task->GetPairDisposition(), bViewerWasReady ? ECardEffectPairDisposition::MoveToVictoryStack : ECardEffectPairDisposition::KeepOnTable);
		TestEqual(TEXT("An unseen ability never scores a victory pair"),
			T.Hands[0]->GetVictoryStack()->GetPairCount(), bViewerWasReady ? 1 : 0);
		if (!bViewerWasReady)
		{
			TestNotNull(TEXT("A failed startup retains the pair for another attempt"), T.Hands[0]->FindActivationPair(Task->GetCardA()));
		}
		T.Mode->AcknowledgeHandReveal(T.Players[0], Session);
		TestEqual(TEXT("A timed-out session cannot be revived by a late acknowledgement"), T.RevealPawnCount(), 0);
	}
	return true;
}
#endif
