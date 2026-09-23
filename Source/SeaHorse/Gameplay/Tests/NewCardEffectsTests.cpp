#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Algo/RandomShuffle.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/LocalPlayer.h"
#include "Slate/SceneViewport.h"
#include "Widgets/SViewport.h"
#include "TimerManager.h"
#include "UObject/UnrealType.h"
#include "Gameplay/Core/SHGameMode.h"
#include "Gameplay/Core/SHGameState.h"
#include "Gameplay/Core/SHPlayerState.h"
#include "Gameplay/Core/SHPlayerController.h"
#include "Gameplay/Components/TurnComponent.h"
#include "Gameplay/Board/VictoryStack.h"
#include "Gameplay/Cards/SHCard.h"
#include "Gameplay/Cards/CardDefinition.h"
#include "Gameplay/Cards/Tasks/ExtendedCardEffectTasks.h"
#include "Gameplay/Cards/Fragments/CardReactionFragment.h"
#include "Gameplay/Presentation/CardReactionPrompt.h"
#include "Gameplay/Presentation/CardSelectionPrompt.h"
#include "Gameplay/SHHand.h"
#if WITH_EDITOR
#include "WidgetBlueprint.h"
#include "Blueprint/WidgetBlueprintGeneratedClass.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Widgets/SOverlay.h"
#endif

struct FSHNewEffectsWorld
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	ASHGameMode* Mode = World->SpawnActor<ASHGameMode>();
	ASHGameState* State = World->SpawnActor<ASHGameState>();
	TArray<ASHPlayerState*> Players;
	TArray<ASHHand*> Hands;
	FSHNewEffectsWorld(int32 Humans = 3, int32 Seats = 4)
	{
		GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
		World->SetGameState(State);
		Mode->GameState = State;
		FindFProperty<FObjectProperty>(UWorld::StaticClass(), TEXT("AuthorityGameMode"))->SetObjectPropertyValue_InContainer(World, Mode);
		Mode->TurnComponent = NewObject<UTurnComponent>(Mode);
		for (int32 Seat = 0; Seat < Seats; ++Seat)
		{
			ASHHand* Hand = World->SpawnActor<ASHHand>();
			Hand->SetLayoutSeatIndex(Seat);
			Hand->VictoryStack = World->SpawnActor<AVictoryStack>();
			Hands.Add(Hand);
			// Place a BN between humans to exercise skipping non-human seats.
			if ((Seat != 1 && Players.Num() < Humans) || Humans == Seats)
			{
				ASHPlayerState* Player = World->SpawnActor<ASHPlayerState>();
				Player->SetSeatIndex(Seat);
				Player->SetHand(Hand);
				ASHPlayerController* PC = World->SpawnActor<ASHPlayerController>();
				PC->PlayerState = Player;
				PC->SetAsLocalPlayerController();
				Player->SetOwner(PC);
				State->AddPlayerState(Player);
				Players.Add(Player);
			}
			else { Hand->SetIsNPC(true); }
		}
		State->SetParticipantHands(Hands);
		Mode->TurnComponent->InitializeTurns(Players[0]);
	}
	~FSHNewEffectsWorld()
	{
		World->DestroyWorld(false);
		GEngine->DestroyWorldContext(World);
	}
	ASHCard* Card(ASHHand* Hand, UClass* Definition = UCardDefinition::StaticClass())
	{
		ASHCard* Card = World->SpawnActor<ASHCard>();
		Card->CardDefinition = Definition;
		Hand->AddCard(Card, Hand->GetCardCount());
		return Card;
	}
	ASHCard* Pair(ASHHand* Hand, UClass* Definition = UCardDefinition::StaticClass())
	{
		ASHCard* A = World->SpawnActor<ASHCard>();
		ASHCard* B = World->SpawnActor<ASHCard>();
		A->CardDefinition = Definition; B->CardDefinition = Definition;
		A->SetOwner(Hand); B->SetOwner(Hand);
		Hand->AddActivationPairToLogicalHand(A, B);
		Hand->SetActivationPairState(A, B, EActivationPairState::Ready);
		return A;
	}
	void Advance()
	{
		for (int32 Step = 0; Step < 30; ++Step) { ++GFrameCounter; World->GetTimerManager().Tick(0.1f); }
	}
	void Draw(ASHPlayerState* Player, ASHHand* Source, ASHCard* Card)
	{
		Source->RemoveCard(Card);
		Player->GetHand()->AddCard(Card, Player->GetHand()->GetCardCount());
		Mode->GetTurnComponent()->HandleCardDrawnFromHand(Player, Source, Card);
	}
};

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSHSeaHorseEntersNPCStackTest, "SeaHorse.Gameplay.Effects.SeaHorseEntersNPCStack",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSHSeaHorseEntersNPCStackTest::RunTest(const FString& Parameters)
{
	UClass* SeaHorse = LoadClass<UCardDefinition>(nullptr,
		TEXT("/Game/SeaHorse/Cards/Definitions/Card_SeaHorse.Card_SeaHorse_C"));
	if (!TestNotNull(TEXT("SeaHorse definition exists"), SeaHorse)) { return false; }
	for (bool TargetNPC : {false, true})
	{
		FSHNewEffectsWorld T;
		ASHHand* Source = T.Players[0]->GetHand();
		ASHHand* Target = TargetNPC ? T.Hands[1] : T.Players[1]->GetHand();
		TArray<ASHCard*> Expected;
		for (int32 Index = 0; Index < 12; ++Index) { Expected.Add(T.Card(Target)); }
		ASHCard* Card = T.Card(Source, SeaHorse);
		Expected.Add(Card);
		FMath::RandInit(9137);
		if (TargetNPC) { Algo::RandomShuffle(Expected); }
		FMath::RandInit(9137);
		TestTrue(TEXT("SeaHorse transfers to selected hand"), T.Mode->TransferCardToHand(Source, Target, SeaHorse));
		TestTrue(TEXT("Entering BN shuffles the whole stack; human hand preserves order"), Target->GetCards() == Expected);
		TestFalse(TEXT("SeaHorse leaves its old hand"), Source->ContainsCard(Card));
		TestEqual(TEXT("Transferred SeaHorse belongs to the destination"), Card->GetOwningHand(), Target);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSHNewCardEffectsTest, "SeaHorse.Gameplay.Effects.SixNewAbilities",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSHNewCardEffectsTest::RunTest(const FString& Parameters)
{
	TGuardValue<bool> ScriptGuard(GAllowActorScriptExecutionInEditor, true);
	auto Definition = [this](const TCHAR* Name)
	{
		UClass* Result = LoadClass<UCardDefinition>(nullptr, *FString::Printf(TEXT("/Game/SeaHorse/Cards/Definitions/%s.%s_C"), Name, Name));
		TestNotNull(Name, Result);
		return Result;
	};
	UClass* Bodgy = Definition(TEXT("Card_BodgyVampireHunter"));
	UClass* Dead = Definition(TEXT("Card_GniewDeadHerald"));
	UClass* Living = Definition(TEXT("Card_GniewLivingHerald"));
	UClass* Hans = Definition(TEXT("Card_HansCaptain"));
	UClass* Slayer = Definition(TEXT("Card_ThronriTrollSlayer"));
	UClass* Monk = Definition(TEXT("Card_YeHeshaNightMonk"));
	UClass* Gloria = Definition(TEXT("Card_Gloria"));
	if (!Bodgy || !Dead || !Living || !Hans || !Slayer || !Monk || !Gloria) { return false; }
	for (int32 SourceCount : {1, 2})
	{
		FSHNewEffectsWorld T;
		ASHPlayerState* Player = T.Players[0];
		ASHHand* Hand = Player->GetHand();
		ASHHand* Source = T.Players[1]->GetHand();
		ASHCard* A = T.Pair(Hand, Bodgy);
		ASHCard* Existing = T.Card(Hand);
		ASHCard* First = T.Card(Source);
		ASHCard* Second = SourceCount == 2 ? T.Card(Source) : nullptr;
		T.State->SetTurnPhase(ETurnPhase::SecondPairing);
		TestFalse(TEXT("Bodgy cannot activate after the first phase"), T.Mode->GetTurnComponent()->CanActivatePair(Player, *Hand->FindActivationPair(A)));
		T.State->SetTurnPhase(ETurnPhase::FirstPairing);
		T.Mode->RequestStoredPairActivation(Player, A);
		T.Advance();
		T.Draw(Player, Source, First);
		if (Second)
		{
			TestFalse(TEXT("Second draw cannot come from another hand"), T.Mode->GetTurnComponent()->CanDrawCardFromHand(Player, T.Hands[1]));
			T.Draw(Player, Source, Second);
		}
		TestTrue(TEXT("Returning a drawn card is mandatory"), T.Mode->PendingHandCardSelections.Contains(Player));
		TestFalse(TEXT("Return cannot be cancelled as an uncommitted effect"), T.Mode->CancelEffectTargetSelection(Player, A, Hand->FindActivationPair(A)->CardB));
		T.Mode->SubmitHandCardSelection(T.Players[1], First);
		T.Mode->SubmitHandCardSelection(Player, Existing);
		TestTrue(TEXT("Wrong player and pre-existing card are rejected"), T.Mode->PendingHandCardSelections.Contains(Player));
		T.Mode->SubmitHandCardSelection(Player, First);
		TestTrue(TEXT("Chosen card returns to its source"), Source->ContainsCard(First));
		TestEqual(TEXT("One net card retained only when two were available"), Hand->GetCardCount(), SourceCount);
		TestFalse(TEXT("Return closes the selection"), T.Mode->IsWaitingForPlayerSelection());
		TestFalse(TEXT("Return completes the task"), T.Mode->HasActiveEffectTasks());
		TestEqual(TEXT("Draw sequence reaches second pairing"), T.State->GetTurnPhase(), ETurnPhase::SecondPairing);
	}
	for (UClass* FollowingEffect : {Hans, Monk})
	{
		FSHNewEffectsWorld T;
		ASHPlayerState* Player = T.Players[0];
		ASHHand* Hand = Player->GetHand();
		ASHHand* Source = T.Players[1]->GetHand();
		ASHCard* First = T.Card(Source);
		ASHCard* Second = T.Card(Source);
		ASHCard* DrawPair = T.Pair(Hand, Bodgy);
		ASHCard* FollowingPair = T.Pair(Hand, FollowingEffect);
		T.Mode->RequestStoredPairActivation(Player, DrawPair);
		T.Mode->RequestStoredPairActivation(Player, FollowingPair);
		T.Advance();
		TestTrue(TEXT("Global effect waits for the pending Bodgy draw"), T.Mode->HasActiveEffectTasks());
		T.Draw(Player, Source, First);
		T.Draw(Player, Source, Second);
		TestTrue(TEXT("Pending global effect preserves mandatory return"), T.Mode->PendingHandCardSelections.Contains(Player));
		T.Mode->SubmitHandCardSelection(Player, Second);
		T.Advance();
		TestFalse(TEXT("Global effect resumes after Bodgy returns a card"), T.Mode->HasActiveEffectTasks());
		TestFalse(TEXT("Combined effects release selection"), T.Mode->IsWaitingForPlayerSelection());
	}
	for (bool HasBodgy : {false, true})
	for (bool TargetNPC : {false, true})
	{
		FSHNewEffectsWorld T;
		ASHHand* Source = TargetNPC ? T.Hands[1] : T.Players[1]->GetHand();
		ASHCard* Requested = HasBodgy ? T.Card(Source, Bodgy) : nullptr;
		TArray<ASHCard*> Remaining;
		for (int32 Index = 0; Index < 12; ++Index) { Remaining.Add(T.Card(Source)); }
		ASHCard* A = T.Pair(T.Players[0]->GetHand(), Dead);
		T.Mode->RequestStoredPairActivation(T.Players[0], A);
		TestEqual(TEXT("Target list includes BN regardless of possession of Bodgy"), T.Mode->PendingParticipantSelections.FindChecked(T.Players[0]).Candidates.Num(), 3);
		T.Mode->SubmitParticipantSelection(T.Players[0], Source);
		TArray<ASHCard*> Expected = Remaining;
		FMath::RandInit(9137);
		if (TargetNPC) { Algo::RandomShuffle(Expected); }
		FMath::RandInit(9137);
		T.Advance();
		if (HasBodgy) { TestTrue(TEXT("Dead herald takes Bodgy"), T.Players[0]->GetHand()->ContainsCard(Requested)); }
		TestTrue(TEXT("BN shuffles with or without Bodgy; human hand preserves order"), Source->GetCards() == Expected);
		TestEqual(TEXT("Only Bodgy leaves the selected source"), Source->GetCardCount(), Remaining.Num());
		TestFalse(TEXT("Missing Bodgy never leaves an active task"), T.Mode->HasActiveEffectTasks());
	}
	{
		FSHNewEffectsWorld T;
		ASHHand* Other = T.Players[1]->GetHand();
		ASHCard* Eligible = T.Pair(Other, Gloria);
		ASHCard* Ineligible = T.Pair(Other, Bodgy);
		ASHCard* OtherEligible = T.Pair(T.Players[2]->GetHand(), Gloria);
		ASHCard* OwnEligible = T.Pair(T.Players[0]->GetHand(), Gloria);
		ASHCard* A = T.Pair(T.Players[0]->GetHand(), Living);
		T.Mode->RequestStoredPairActivation(T.Players[0], A);
		TestTrue(TEXT("Living herald skips player selection"), T.Mode->PendingPlayerSelections.IsEmpty() && T.Mode->PendingParticipantSelections.IsEmpty());
		TestTrue(TEXT("Living herald offers the allowed pair"), T.Mode->PendingPairSelections.FindChecked(T.Players[0]).CandidateCards.Contains(Eligible));
		TestTrue(TEXT("Living herald immediately offers all opponent zones"), T.Mode->PendingPairSelections.FindChecked(T.Players[0]).CandidateCards.Contains(OtherEligible));
		TestFalse(TEXT("Living herald excludes its own zone"), T.Mode->PendingPairSelections.FindChecked(T.Players[0]).CandidateCards.Contains(OwnEligible));
		TestFalse(TEXT("Living herald excludes other identities"), T.Mode->PendingPairSelections.FindChecked(T.Players[0]).CandidateCards.Contains(Ineligible));
		T.Mode->SubmitActivationPairSelection(T.Players[0], Eligible);
		T.Advance();
		TestNotNull(TEXT("Stolen pair is in the new activation zone"), T.Players[0]->GetHand()->FindActivationPair(Eligible));
		TestNull(TEXT("Stolen pair leaves its previous zone"), Other->FindActivationPair(Eligible));
		TestEqual(TEXT("Stolen pair has the new network owner"), Eligible->GetOwningHand(), T.Players[0]->GetHand());
	}
	{
		FSHNewEffectsWorld T;
		ASHPlayerState* Player = T.Players[0];
		ASHHand* Hand = Player->GetHand();
		ASHCard* Herald = T.Pair(Hand, Living);
		auto CanActivate = [&]() { return T.Mode->GetTurnComponent()->CanActivatePair(Player, *Hand->FindActivationPair(Herald)); };
		TestFalse(TEXT("Living herald cannot activate on an empty table"), CanActivate());
		T.Pair(Hand, Gloria);
		T.Pair(T.Players[1]->GetHand(), Bodgy);
		TestFalse(TEXT("Own eligible pair and opponent's wrong identity do not enable herald"), CanActivate());
		T.Mode->RequestStoredPairActivation(Player, Herald);
		TestFalse(TEXT("Server rejects herald activation without a target"), T.Mode->HasActiveEffectTasks());
		TestEqual(TEXT("Rejected herald stays ready"), Hand->FindActivationPair(Herald)->State, EActivationPairState::Ready);
		ASHCard* Target = T.Pair(T.Players[1]->GetHand(), Gloria);
		TestTrue(TEXT("Opponent eligible pair enables herald"), CanActivate());
		FindFProperty<FClassProperty>(ASHCard::StaticClass(), TEXT("RevealedCardDefinition"))->SetObjectPropertyValue_InContainer(Target, Gloria);
		Target->CardDefinition = nullptr;
		TestTrue(TEXT("Public revealed definition enables activation on clients"), CanActivate());
		Target->CardDefinition = Gloria;
		T.Mode->RequestStoredPairActivation(Player, Herald);
		T.Mode->SubmitActivationPairSelection(Player, Target);
		T.Mode->RemoveStoredPairFromGame(T.Players[1]->GetHand(), Target);
		T.Advance();
		TestFalse(TEXT("Lost target releases the effect"), T.Mode->HasActiveEffectTasks());
		TestNotNull(TEXT("Lost target does not consume herald"), Hand->FindActivationPair(Herald));
		TestFalse(TEXT("No remaining target disables herald again"), CanActivate());
	}
	for (int32 Seats : {4, 6})
	{
		FSHNewEffectsWorld T(Seats - 1, Seats);
		TArray<ASHCard*> Pairs;
		for (ASHPlayerState* Player : T.Players) { Pairs.Add(T.Pair(Player->GetHand())); }
		ASHCard* A = T.Pair(T.Players[0]->GetHand(), Hans);
		T.Mode->RequestStoredPairActivation(T.Players[0], A);
		T.Advance();
		for (int32 Index = 0; Index < Pairs.Num(); ++Index)
		{
			TestNotNull(TEXT("Hans passes every stored pair right, skipping BN"), T.Players[(Index + Pairs.Num() - 1) % Pairs.Num()]->GetHand()->FindActivationPair(Pairs[Index]));
		}
		TestTrue(TEXT("BN receives no activation pairs"), T.Hands[1]->GetLogicalActivationPairs().IsEmpty());
		TestEqual(TEXT("Hans's consumed pair goes to its activator's victory stack"), T.Players[0]->GetHand()->GetVictoryStack()->GetPairCount(), 1);
	}
	{
		FSHNewEffectsWorld T;
		ASHCard* Victim = T.Pair(T.Players[1]->GetHand());
		ASHCard* A = T.Pair(T.Players[0]->GetHand(), Slayer);
		T.Mode->RequestStoredPairActivation(T.Players[0], A);
		T.Mode->SubmitActivationPairSelection(T.Players[0], Victim);
		TestTrue(TEXT("Removal waits for presentation"), IsValid(Victim));
		T.Advance();
		TestFalse(TEXT("Chosen pair is destroyed"), IsValid(Victim));
		TestFalse(TEXT("Slayer pair is destroyed"), IsValid(A));
		TestEqual(TEXT("Removal awards no victory point"), T.Players[0]->GetHand()->GetVictoryStack()->GetPairCount(), 0);
		TestFalse(TEXT("Removal drains active effects"), T.Mode->HasActiveEffectTasks());
	}
	{
		FSHNewEffectsWorld T;
		ASHCard* A = T.Pair(T.Players[0]->GetHand(), Slayer);
		T.Mode->RequestStoredPairActivation(T.Players[0], A);
		TestFalse(TEXT("No other pair does not block the turn"), T.Mode->HasActiveEffectTasks());
		TestEqual(TEXT("Without a target Slayer remains ready"), T.Players[0]->GetHand()->FindActivationPair(A)->State, EActivationPairState::Ready);
	}
	for (int32 Seats : {4, 6})
	{
		FSHNewEffectsWorld T(Seats - 1, Seats);
		TSet<ASHCard*> Before;
		for (int32 Index = 0; Index < 17; ++Index) { Before.Add(T.Card(T.Hands[Index % Seats])); }
		ASHCard* Stored = T.Pair(T.Players[1]->GetHand());
		ASHCard* A = T.Pair(T.Players[0]->GetHand(), Monk);
		T.Mode->RequestStoredPairActivation(T.Players[0], A);
		T.Advance();
		TSet<ASHCard*> After;
		int32 Count = 0;
		for (ASHHand* Hand : T.Hands)
		{
			TestTrue(TEXT("Redeal is balanced including BN"), Hand->GetCardCount() == 17 / Seats || Hand->GetCardCount() == 17 / Seats + 1);
			for (ASHCard* Card : Hand->GetCards()) { After.Add(Card); ++Count; TestEqual(TEXT("Redealt ownership is correct"), Card->GetOwningHand(), Hand); }
		}
		TestEqual(TEXT("Redeal loses or duplicates no cards"), Count, Before.Num());
		TestEqual(TEXT("Redeal keeps all original identities"), After.Intersect(Before).Num(), Before.Num());
		TestNotNull(TEXT("Redeal leaves activation zones intact"), T.Players[1]->GetHand()->FindActivationPair(Stored));
		TestFalse(TEXT("Redeal completes"), T.Mode->HasActiveEffectTasks());
	}
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSHExpansionEffectsTest, "SeaHorse.Gameplay.Effects.ExchangeProtectionAndPaulusPairing",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSHExpansionEffectsTest::RunTest(const FString& Parameters)
{
	TGuardValue<bool> ScriptGuard(GAllowActorScriptExecutionInEditor, true);
	auto Load = [](const TCHAR* Name) { return LoadClass<UCardDefinition>(nullptr, *FString::Printf(TEXT("/Game/SeaHorse/Cards/Definitions/%s.%s_C"), Name, Name)); };
	UClass* Kurt = Load(TEXT("Card_KurtPriest"));
	UClass* Wu = Load(TEXT("Card_PaulusWitchHunterWu"));
	UClass* Rats = Load(TEXT("Card_RatfolkUnderground"));
	UClass* Silent = Load(TEXT("Card_PaulusSilent"));
	UClass* Dead = Load(TEXT("Card_GniewDeadHerald"));
	if (!TestNotNull(TEXT("Kurt BP exists"), Kurt) || !TestNotNull(TEXT("Wu BP exists"), Wu) || !TestNotNull(TEXT("Ratfolk BP exists"), Rats) || !Silent || !Dead) { return false; }
	for (bool NPC : {false, true})
	for (int32 Count : {1, 3})
	{
		FSHNewEffectsWorld T;
		ASHPlayerState* Player = T.Players[0];
		ASHHand* Hand = Player->GetHand();
		ASHHand* Recipient = NPC ? T.Hands[1] : T.Players[1]->GetHand();
		TArray<ASHCard*> Offered;
		for (int32 Index = 0; Index < Count; ++Index) { Offered.Add(T.Card(Hand)); }
		for (int32 Index = 0; Index < 8; ++Index) { T.Card(Recipient); }
		ASHCard* Effect = T.Pair(Hand, Kurt);
		T.Mode->RequestStoredPairActivation(Player, Effect);
		TestTrue(TEXT("Kurt starts with hand-card selection"), T.Mode->PendingHandCardSelections.Contains(Player));
		T.Mode->SubmitHandCardsSelection(Player, {});
		T.Mode->SubmitHandCardsSelection(Player, {Offered[0], Offered[0]});
		TestTrue(TEXT("Empty and duplicate offers rejected"), T.Mode->PendingHandCardSelections.Contains(Player));
		if (Count == 3)
		{
			ASHPlayerController* PC = CastChecked<ASHPlayerController>(Player->GetOwner());
			PC->TryHandleEffectSelectionClick(Offered[0]);
			PC->TryHandleEffectSelectionClick(Offered[1]);
			TestTrue(TEXT("Two cards still allow changing the offer"), T.Mode->PendingHandCardSelections.Contains(Player));
			PC->TryHandleEffectSelectionClick(Offered[0]);
			TestEqual(TEXT("Clicking selected card deselects it"), PC->LocallySelectedEffectCards.Num(), 1);
			PC->TryHandleEffectSelectionClick(Offered[0]);
			PC->TryHandleEffectSelectionClick(Offered[2]);
			TestFalse(TEXT("Third card automatically confirms without Enter"), T.Mode->PendingHandCardSelections.Contains(Player));
		}
		else { T.Mode->SubmitHandCardsSelection(Player, Offered); }
		TestTrue(TEXT("Kurt then selects a recipient"), T.Mode->PendingParticipantSelections.Contains(Player));
		T.Mode->SubmitParticipantSelection(Player, Recipient);
		TestEqual(TEXT("Offer waits for presentation"), Hand->GetCardCount(), Count);
		T.Advance();
		TestEqual(TEXT("All offered cards transferred before drawing"), Hand->GetCardCount(), 0);
		TestFalse(TEXT("Exchange cannot be cancelled after transfer"), T.Mode->CancelEffectTargetSelection(Player, Effect, Hand->FindActivationPair(Effect)->CardB));
		for (int32 Index = 0; Index < Count; ++Index)
		{
			TestTrue(TEXT("Each exchange draw has an authoritative selection"), T.Mode->PendingHandCardSelections.Contains(Player));
			const auto& Pending = T.Mode->PendingHandCardSelections.FindChecked(Player);
			if (NPC) { TestEqual(TEXT("BN offers only its top card"), Pending.CandidateCards.Num(), 1); }
			T.Mode->SubmitHandCardSelection(Player, NPC ? Recipient->GetTopCard() : Recipient->GetCards()[0]);
		}
		TestEqual(TEXT("Kurt receives exactly the offered number"), Hand->GetCardCount(), Count);
		TestEqual(TEXT("Recipient net card count is unchanged"), Recipient->GetCardCount(), 8);
		TestFalse(TEXT("Exchange releases tasks and selections"), T.Mode->HasActiveEffectTasks() || T.Mode->IsWaitingForPlayerSelection());
		TestEqual(TEXT("Exchange does not consume the ordinary draw phase"), T.State->GetTurnPhase(), ETurnPhase::FirstPairing);
	}
	{
		FSHNewEffectsWorld T;
		ASHCard* Effect = T.Pair(T.Players[0]->GetHand(), Kurt);
		T.Mode->RequestStoredPairActivation(T.Players[0], Effect);
		TestFalse(TEXT("No cards to exchange leaves no pending effect"), T.Mode->HasActiveEffectTasks());
		TestNotNull(TEXT("No cards does not consume Kurt"), T.Players[0]->GetHand()->FindActivationPair(Effect));
	}
	{
		FSHNewEffectsWorld T;
		ASHPlayerState* Protected = T.Players[0];
		for (ASHHand* Hand : T.Hands) { for (int32 Index = 0; Index < 4; ++Index) { T.Card(Hand); } }
		ASHCard* Stored = T.Pair(Protected->GetHand());
		ASHCard* Effect = T.Pair(Protected->GetHand(), Wu);
		T.Mode->RequestStoredPairActivation(Protected, Effect);
		T.Advance();
		TestTrue(TEXT("Wu grants replicated protection"), Protected->IsProtectedFromCardEffects());
		T.State->SetCurrentPlayer(T.Players[1]);
		TestFalse(TEXT("Protected hand cannot be drawn from"), T.Mode->GetTurnComponent()->CanDrawCardFromHand(T.Players[1], Protected->GetHand()));
		const TArray<ASHCard*> ProtectedCards = Protected->GetHand()->GetCards();
		T.Mode->PassHandsToLeft();
		TestTrue(TEXT("Hand rotation skips protected player"), Protected->GetHand()->GetCards() == ProtectedCards);
		T.Mode->ShuffleAndRedealHands();
		TestTrue(TEXT("Global shuffle skips protected player"), Protected->GetHand()->GetCards() == ProtectedCards);
		T.Mode->RotateActivationZonesRight(nullptr);
		TestNotNull(TEXT("Hans rotates the protected player's zone too"), T.Players[2]->GetHand()->FindActivationPair(Stored));
		ASHCard* CollectionStored = T.Pair(Protected->GetHand());
		T.Mode->MoveAllActivationPairsToVictoryStacks();
		TestNotNull(TEXT("Global collection still skips protected player"), Protected->GetHand()->FindActivationPair(CollectionStored));
		ASHCard* Herald = T.Pair(T.Players[1]->GetHand(), Dead);
		T.Mode->RequestStoredPairActivation(T.Players[1], Herald);
		TestFalse(TEXT("Protected player is excluded from targeted effects"), T.Mode->PendingParticipantSelections.FindChecked(T.Players[1]).Candidates.Contains(Protected->GetHand()));
		T.Mode->CancelEffectTargetSelection(T.Players[1], Herald, T.Players[1]->GetHand()->FindActivationPair(Herald)->CardB);
		T.Mode->GetTurnComponent()->EndTurn();
		TestTrue(TEXT("Protection survives other turns"), Protected->IsProtectedFromCardEffects());
		T.Mode->GetTurnComponent()->EndTurn();
		TestEqual(TEXT("Turn returns to protected player"), T.State->GetCurrentPlayer(), Protected);
		TestFalse(TEXT("Protection expires at the beginning of own next turn"), Protected->IsProtectedFromCardEffects());
	}
	for (UClass* Paulus : {Silent, Wu})
	for (int32 Location : {0, 1, 2})
	{
		FSHNewEffectsWorld T;
		ASHPlayerState* Player = T.Players[0];
		ASHCard* A = T.Card(Player->GetHand(), Rats);
		ASHCard* B = T.Card(Player->GetHand(), Paulus);
		ASHCard* Other = T.Card(Location == 0 ? T.Players[1]->GetHand() : T.Hands[1], Paulus);
		if (Location == 2)
		{
			T.Hands[1]->RemoveCard(Other);
			ASHCard* Mate = T.Card(T.Hands[1]);
			T.Hands[1]->RemoveCard(Mate);
			T.Players[1]->GetHand()->GetVictoryStack()->AddPair(Other, Mate);
		}
		TestTrue(TEXT("Ratfolk pair with either Paulus variant"), T.Mode->AreCardsPairCompatible(A, B));
		TestTrue(TEXT("Special pairing is symmetric"), T.Mode->AreCardsPairCompatible(B, A));
		TestFalse(TEXT("Ratfolk cannot pair with another Ratfolk"), UCardDefinition::ArePairDefinitionsCompatible(Rats, Rats));
		TestFalse(TEXT("Ratfolk cannot pair with unrelated card"), UCardDefinition::ArePairDefinitionsCompatible(Rats, Kurt));
		T.Mode->ActivatePair(Player, A, B);
		TestEqual(TEXT("Special pair goes directly to victory"), Player->GetHand()->GetVictoryStack()->GetPairCount(), 1);
		TestTrue(TEXT("No activation zone entry"), Player->GetHand()->GetLogicalActivationPairs().IsEmpty());
		TestFalse(TEXT("Paulus effect never starts"), T.Mode->HasActiveEffectTasks() || Player->IsProtectedFromCardEffects());
		TestFalse(TEXT("Other Paulus copy is removed from its location"), IsValid(Other));
	}
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSHCardReactionsTest, "SeaHorse.Gameplay.Effects.OutOfTurnReactions",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSHCardReactionsTest::RunTest(const FString& Parameters)
{
	TGuardValue<bool> ScriptGuard(GAllowActorScriptExecutionInEditor, true);
	auto Load = [](const TCHAR* Name) { return LoadClass<UCardDefinition>(nullptr, *FString::Printf(TEXT("/Game/SeaHorse/Cards/Definitions/%s.%s_C"), Name, Name)); };
	UClass* Capture = Load(TEXT("Card_GieselbrechtApologist"));
	UClass* Cancel = Load(TEXT("Card_GieselbrechtWizardApprentice"));
	UClass* Bodgy = Load(TEXT("Card_BodgyVampireHunter"));
	UClass* Dead = Load(TEXT("Card_GniewDeadHerald"));
	UClass* Hans = Load(TEXT("Card_HansCaptain"));
	if (!TestNotNull(TEXT("Apologist BP exists"), Capture) || !TestNotNull(TEXT("Apprentice BP exists"), Cancel) || !Bodgy || !Dead || !Hans) { return false; }
	for (UClass* Definition : {Capture, Cancel})
	{
		const auto* Fragment = Cast<UCardReactionFragment>(UCardDefinition::FindFragmentByClass(Definition, UCardReactionFragment::StaticClass()));
		if (!TestNotNull(TEXT("Reaction fragment configured"), Fragment)) { return false; }
		TestNotNull(TEXT("Reaction has an assigned UI class"), Fragment->PromptWidgetClass.Get());
	}
	auto OfferId = [this](FSHNewEffectsWorld& T, ASHPlayerState* Player)
	{
		const auto* Offer = T.Mode->ActiveReactionOffers.Find(Player);
		TestNotNull(TEXT("Player has an active reaction offer"), Offer);
		return Offer ? Offer->OfferId : INDEX_NONE;
	};
	auto ClientOfferId = [](ASHPlayerState* Player)
	{
		return CastChecked<ASHPlayerController>(Player->GetOwner())->ActiveReactionOfferId;
	};
	auto DeclineRemainingOffers = [this](FSHNewEffectsWorld& T)
	{
		// Preserve a scenario's chosen reaction while refusing new counter windows.
		// Snapshot one offer at a time: a decline can offer that player's next pair.
		for (int32 Attempt = 0; Attempt < 20 && !T.Mode->ActiveReactionOffers.IsEmpty(); ++Attempt)
		{
			const auto Offer = T.Mode->ActiveReactionOffers.CreateConstIterator();
			ASHPlayerState* Player = Offer.Key().Get();
			const int32 Id = Offer.Value().OfferId;
			T.Mode->RespondToCardReaction(Player, Id, false);
		}
		TestTrue(TEXT("Declining all remaining offers terminates the reaction window"), T.Mode->ActiveReactionOffers.IsEmpty());
	};
	for (bool OlderWins : {false, true})
	{
		FSHNewEffectsWorld T;
		ASHCard* Older = T.Pair(T.Players[2]->GetHand(), Cancel);
		ASHCard* Newer = T.Pair(T.Players[1]->GetHand(), Cancel);
		ASHCard* Target = T.Pair(T.Players[0]->GetHand(), Bodgy);
		T.Card(T.Players[1]->GetHand());
		T.Mode->RequestStoredPairActivation(T.Players[0], Target);
		TestTrue(TEXT("Activation pauses for reaction"), T.State->bReactionPending);
		TestEqual(TEXT("Both eligible players receive offers immediately"), T.Mode->ActiveReactionOffers.Num(), 2);
		const int32 OlderOffer = OfferId(T, T.Players[2]);
		const int32 NewerOffer = OfferId(T, T.Players[1]);
		TestNotEqual(TEXT("Each player's offer has its own response token"), OlderOffer, NewerOffer);
		TestEqual(TEXT("Older player's client received the prompt"), ClientOfferId(T.Players[2]), OlderOffer);
		TestEqual(TEXT("Newer player's client received the prompt simultaneously"), ClientOfferId(T.Players[1]), NewerOffer);
		TestFalse(TEXT("Normal draw is blocked while deciding"), T.Mode->GetTurnComponent()->CanDrawCardFromHand(T.Players[0], T.Players[1]->GetHand()));
		T.Mode->GetTurnComponent()->SkipCurrentPhase(T.Players[0]);
		TestEqual(TEXT("Cannot advance phase during reaction"), T.State->GetTurnPhase(), ETurnPhase::FirstPairing);
		T.Mode->RespondToCardReaction(T.Players[1], OlderOffer, true);
		T.Mode->RespondToCardReaction(T.Players[0], NewerOffer, true);
		TestEqual(TEXT("A forged player's response cannot close either offer"), T.Mode->ActiveReactionOffers.Num(), 2);
		ASHPlayerState* Reactor = OlderWins ? T.Players[2] : T.Players[1];
		ASHPlayerState* Other = OlderWins ? T.Players[1] : T.Players[2];
		const int32 AcceptedOffer = OlderWins ? OlderOffer : NewerOffer;
		const int32 LosingOffer = OlderWins ? NewerOffer : OlderOffer;
		T.Mode->RespondToCardReaction(Reactor, AcceptedOffer, true);
		TestEqual(TEXT("First acceptance replaces the old window with a counter window"), T.Mode->ActiveReactionOffers.Num(), 1);
		TestEqual(TEXT("Winner's client prompt closes"), ClientOfferId(Reactor), INDEX_NONE);
		const int32 CounterOffer = OfferId(T, Other);
		TestNotEqual(TEXT("Other player receives a fresh offer to counter the accepted reaction"), CounterOffer, LosingOffer);
		TestEqual(TEXT("Counter window targets the accepted reaction pair"), T.Mode->ReactionTargetA.Get(), OlderWins ? Older : Newer);
		T.Mode->RespondToCardReaction(Other, LosingOffer, true);
		T.Mode->RespondToCardReaction(Reactor, AcceptedOffer, true);
		TestEqual(TEXT("Delayed or duplicate acceptance cannot answer the counter window"), OfferId(T, Other), CounterOffer);
		T.Mode->RespondToCardReaction(Other, CounterOffer, false);
		T.Advance();
		TestFalse(TEXT("Accepted counter closes window and starts no target effect"), T.Mode->HasActiveEffectTasks());
		TestFalse(TEXT("Declining the counter window releases gameplay"), T.State->bReactionPending);
		TestEqual(TEXT("Cancelled pair awards its owner one victory pair"), T.Players[0]->GetHand()->GetVictoryStack()->GetPairCount(), 1);
		TestEqual(TEXT("Counter is spent exactly once"), Reactor->GetHand()->GetVictoryStack()->GetPairCount(), 1);
		TestEqual(TEXT("Losing response does not consume another counter"), Other->GetHand()->GetVictoryStack()->GetPairCount(), 0);
		TestNotNull(TEXT("Unaccepted counter remains ready"), Other->GetHand()->FindActivationPair(OlderWins ? Newer : Older));
	}
	for (int32 CounterCount : {2, 3})
	{
		FSHNewEffectsWorld T(4, 4);
		TArray<ASHCard*> Counters;
		for (int32 PlayerIndex = 1; PlayerIndex <= CounterCount; ++PlayerIndex)
		{
			Counters.Add(T.Pair(T.Players[PlayerIndex]->GetHand(), Cancel));
		}
		ASHCard* Target = T.Pair(T.Players[0]->GetHand(), Bodgy);
		T.Card(T.Players[1]->GetHand());
		T.Mode->RequestStoredPairActivation(T.Players[0], Target);
		for (int32 Index = 0; Index < CounterCount; ++Index)
		{
			ASHPlayerState* Reactor = T.Players[Index + 1];
			TestEqual(TEXT("Each counter targets the preceding activation"), T.Mode->ReactionTargetA.Get(), Index == 0 ? Target : Counters[Index - 1]);
			T.Mode->RespondToCardReaction(Reactor, OfferId(T, Reactor), true);
			if (Index + 1 < CounterCount)
			{
				TestTrue(TEXT("Another ready counter opens a new response window"), T.State->bReactionPending);
				TestEqual(TEXT("Accepted counter waits in its zone until the chain resolves"), Reactor->GetHand()->GetVictoryStack()->GetPairCount(), 0);
				for (int32 UsedIndex = 0; UsedIndex <= Index; ++UsedIndex)
				{
					TestFalse(TEXT("A reserved reaction cannot be reused in the same chain"), T.Mode->ActiveReactionOffers.Contains(T.Players[UsedIndex + 1]));
				}
			}
		}
		T.Advance();
		TestFalse(TEXT("Counter chain closes the authoritative response pause"), T.State->bReactionPending);
		TestEqual(TEXT("Even counter count restores the root effect; odd count cancels it"), T.Mode->HasActiveEffectTasks(), CounterCount % 2 == 0);
		TestEqual(TEXT("Only a cancelled root immediately reaches victory"), T.Players[0]->GetHand()->GetVictoryStack()->GetPairCount(), CounterCount % 2);
		for (int32 Index = 0; Index < CounterCount; ++Index)
		{
			TestEqual(TEXT("Every accepted counter is consumed once, including cancelled counters"), T.Players[Index + 1]->GetHand()->GetVictoryStack()->GetPairCount(), 1);
			TestNull(TEXT("Resolved counters leave their activation zones"), T.Players[Index + 1]->GetHand()->FindActivationPair(Counters[Index]));
		}
	}
	{
		FSHNewEffectsWorld T;
		T.Pair(T.Players[1]->GetHand(), Capture);
		T.Pair(T.Players[2]->GetHand(), Cancel);
		ASHCard* Target = T.Pair(T.Players[0]->GetHand(), Dead);
		ASHCard* Requested = T.Card(T.Players[2]->GetHand(), Bodgy);
		T.Mode->RequestStoredPairActivation(T.Players[0], Target);
		TestFalse(TEXT("Apologist is not offered before the effect"), T.Mode->ActiveReactionOffers.Contains(T.Players[1]));
		T.Mode->RespondToCardReaction(T.Players[2], OfferId(T, T.Players[2]), false);
		T.Advance();
		TestTrue(TEXT("Original effect selects its target before capture"), T.Mode->PendingParticipantSelections.Contains(T.Players[0]));
		T.Mode->SubmitParticipantSelection(T.Players[0], T.Players[2]->GetHand());
		T.Advance();
		TestTrue(TEXT("Effect is already applied when capture is offered"), T.Players[0]->GetHand()->ContainsCard(Requested));
		T.Mode->RespondToCardReaction(T.Players[1], OfferId(T, T.Players[1]), true);
		T.Mode->RespondToCardReaction(T.Players[2], OfferId(T, T.Players[2]), true);
		T.Advance();
		TestTrue(TEXT("Countering capture does not undo the completed effect"), T.Players[0]->GetHand()->ContainsCard(Requested));
		TestNull(TEXT("Cancelled capture does not transfer the original pair"), T.Players[1]->GetHand()->FindActivationPair(Target));
		for (ASHPlayerState* Player : T.Players)
		{
			TestEqual(TEXT("Root, cancelled capture and counter each reach their owner's victory stack"), Player->GetHand()->GetVictoryStack()->GetPairCount(), 1);
		}
		TestFalse(TEXT("Countered capture leaves no pending task or response"), T.Mode->HasActiveEffectTasks());
	}
	{
		FSHNewEffectsWorld T;
		ASHCard* Counter = T.Pair(T.Players[1]->GetHand(), Cancel);
		T.Pair(T.Players[2]->GetHand(), Capture);
		ASHCard* Target = T.Pair(T.Players[0]->GetHand(), Bodgy);
		T.Mode->RequestStoredPairActivation(T.Players[0], Target);
		T.Mode->RespondToCardReaction(T.Players[1], OfferId(T, T.Players[1]), true);
		T.Advance();
		TestEqual(TEXT("Capture is offered for the completed counter, never the cancelled root"), T.Mode->ReactionTargetA.Get(), Counter);
		T.Mode->RespondToCardReaction(T.Players[2], OfferId(T, T.Players[2]), true);
		T.Advance();
		TestFalse(TEXT("Capturing a counter lets that counter cancel the root"), T.Mode->HasActiveEffectTasks());
		TestEqual(TEXT("Cancelled root reaches victory"), T.Players[0]->GetHand()->GetVictoryStack()->GetPairCount(), 1);
		TestEqual(TEXT("Captured counter does not award its previous owner a victory point"), T.Players[1]->GetHand()->GetVictoryStack()->GetPairCount(), 0);
		TestEqual(TEXT("Capture itself is consumed once"), T.Players[2]->GetHand()->GetVictoryStack()->GetPairCount(), 1);
		const FActivatedPair* Captured = T.Players[2]->GetHand()->FindActivationPair(Counter);
		if (TestNotNull(TEXT("Resolved reaction pair can itself be captured"), Captured))
		{
			TestEqual(TEXT("Captured reaction is ready for a later activation"), Captured->State, EActivationPairState::Ready);
			TestFalse(TEXT("Captured reaction clears its activated flag"), Captured->bActivated);
			TestFalse(TEXT("Captured reaction clears its reservation"), Captured->bActivationQueued);
		}
		TestEqual(TEXT("Captured reaction receives its new owner"), Counter->GetOwningHand(), T.Players[2]->GetHand());
	}
	{
		FSHNewEffectsWorld T;
		ASHCard* FirstCapture = T.Pair(T.Players[1]->GetHand(), Capture);
		T.Pair(T.Players[2]->GetHand(), Capture);
		ASHCard* Target = T.Pair(T.Players[0]->GetHand(), Dead);
		T.Card(T.Players[2]->GetHand(), Bodgy);
		T.Mode->RequestStoredPairActivation(T.Players[0], Target);
		T.Mode->SubmitParticipantSelection(T.Players[0], T.Players[2]->GetHand());
		T.Advance();
		TestEqual(TEXT("Both Apologists are offered the completed effect concurrently"), T.Mode->ActiveReactionOffers.Num(), 2);
		T.Mode->RespondToCardReaction(T.Players[1], OfferId(T, T.Players[1]), true);
		T.Advance();
		TestEqual(TEXT("Next capture targets the completed Apologist"), T.Mode->ReactionTargetA.Get(), FirstCapture);
		T.Mode->RespondToCardReaction(T.Players[2], OfferId(T, T.Players[2]), true);
		T.Advance();
		// The first Apologist has moved to player two, so it can now react to that
		// player's completed Apologist only if owned by somebody else (it isn't).
		TestNotNull(TEXT("Second Apologist takes the completed first Apologist"), T.Players[2]->GetHand()->FindActivationPair(FirstCapture));
		TestEqual(TEXT("Captured reaction has its new network owner"), FirstCapture->GetOwningHand(), T.Players[2]->GetHand());
		TestNotNull(TEXT("Original completed pair remains with the first capturing player"), T.Players[1]->GetHand()->FindActivationPair(Target));
		TestEqual(TEXT("Only the final uncaptured reaction reaches victory"), T.Players[2]->GetHand()->GetVictoryStack()->GetPairCount(), 1);
		TestFalse(TEXT("Reaction presentations and root rotation fully drain"), T.Mode->HasActiveEffectTasks());
	}
	{
		FSHNewEffectsWorld T;
		T.Pair(T.Players[1]->GetHand(), Cancel);
		T.Pair(T.Players[2]->GetHand(), Cancel);
		ASHCard* Target = T.Pair(T.Players[0]->GetHand(), Bodgy);
		T.Mode->RequestStoredPairActivation(T.Players[0], Target);
		T.Mode->RespondToCardReaction(T.Players[1], OfferId(T, T.Players[1]), true);
		TestTrue(TEXT("Accepted counter waits for the other player's counter decision"), T.State->bReactionPending);
		T.Mode->Logout(CastChecked<ASHPlayerController>(T.Players[2]->GetOwner()));
		T.Advance();
		TestFalse(TEXT("Last pending responder disconnecting resolves the accepted counter"), T.Mode->HasActiveEffectTasks());
		TestFalse(TEXT("Disconnect during a counter window releases the global pause"), T.State->bReactionPending);
		TestEqual(TEXT("Disconnect does not undo the accepted counter"), T.Players[0]->GetHand()->GetVictoryStack()->GetPairCount(), 1);
		TestEqual(TEXT("Accepted counter is consumed once after the disconnect"), T.Players[1]->GetHand()->GetVictoryStack()->GetPairCount(), 1);
		TestEqual(TEXT("Disconnecting without accepting does not consume a pair"), T.Players[2]->GetHand()->GetVictoryStack()->GetPairCount(), 0);
	}
	{
		FSHNewEffectsWorld T;
		T.Pair(T.Players[1]->GetHand(), Cancel);
		ASHCard* Unused = T.Pair(T.Players[2]->GetHand(), Cancel);
		ASHCard* Target = T.Pair(T.Players[0]->GetHand(), Bodgy);
		T.Mode->RequestStoredPairActivation(T.Players[0], Target);
		T.Mode->RespondToCardReaction(T.Players[1], OfferId(T, T.Players[1]), true);
		T.Mode->Logout(CastChecked<ASHPlayerController>(T.Players[0]->GetOwner()));
		T.Advance();
		TestFalse(TEXT("Root activator disconnecting closes the entire pending chain"), T.Mode->HasActiveEffectTasks());
		TestFalse(TEXT("Root disconnect releases the global reaction pause"), T.State->bReactionPending);
		TestTrue(TEXT("Root disconnect clears all remaining response tokens"), T.Mode->ActiveReactionOffers.IsEmpty());
		TestEqual(TEXT("Pending responder's client prompt closes on root disconnect"), ClientOfferId(T.Players[2]), INDEX_NONE);
		TestEqual(TEXT("Already accepted reaction is settled once on root disconnect"), T.Players[1]->GetHand()->GetVictoryStack()->GetPairCount(), 1);
		TestNotNull(TEXT("Root disconnect preserves the unaccepted reaction"), T.Players[2]->GetHand()->FindActivationPair(Unused));
	}
	{
		FSHNewEffectsWorld T;
		ASHCard* First = T.Pair(T.Players[1]->GetHand(), Cancel);
		ASHCard* Second = T.Pair(T.Players[2]->GetHand(), Cancel);
		ASHCard* Target = T.Pair(T.Players[0]->GetHand(), Bodgy);
		T.Mode->RequestStoredPairActivation(T.Players[0], Target);
		const int32 FirstOffer = OfferId(T, T.Players[1]);
		const int32 SecondOffer = OfferId(T, T.Players[2]);
		T.Mode->RespondToCardReaction(T.Players[1], FirstOffer, false);
		TestTrue(TEXT("One decline keeps gameplay paused for the other player"), T.State->bReactionPending);
		TestEqual(TEXT("Only the undecided offer remains"), T.Mode->ActiveReactionOffers.Num(), 1);
		TestEqual(TEXT("Declining player closes their own prompt"), ClientOfferId(T.Players[1]), INDEX_NONE);
		TestEqual(TEXT("Decline does not replace another player's prompt"), ClientOfferId(T.Players[2]), SecondOffer);
		T.Mode->RespondToCardReaction(T.Players[1], FirstOffer, true);
		TestTrue(TEXT("Accepting a previously declined offer is ignored"), T.State->bReactionPending);
		T.Mode->RespondToCardReaction(T.Players[2], SecondOffer, false);
		T.Advance();
		TestFalse(TEXT("Last decline releases the reaction pause"), T.State->bReactionPending);
		TestTrue(TEXT("All declines let the original deferred draw start"), T.Mode->HasActiveEffectTasks());
		TestNotNull(TEXT("First declined pair remains in its zone"), T.Players[1]->GetHand()->FindActivationPair(First));
		TestNotNull(TEXT("Second declined pair remains in its zone"), T.Players[2]->GetHand()->FindActivationPair(Second));
	}
	{
		FSHNewEffectsWorld T;
		ASHCard* FirstChoice = T.Pair(T.Players[1]->GetHand(), Cancel);
		ASHCard* SecondChoice = T.Pair(T.Players[1]->GetHand(), Cancel);
		ASHCard* OtherChoice = T.Pair(T.Players[2]->GetHand(), Cancel);
		ASHCard* Target = T.Pair(T.Players[0]->GetHand(), Bodgy);
		T.Mode->RequestStoredPairActivation(T.Players[0], Target);
		const int32 FirstOffer = OfferId(T, T.Players[1]);
		const int32 OtherOffer = OfferId(T, T.Players[2]);
		TestEqual(TEXT("A player first sees their own earliest pair"), T.Mode->ActiveReactionOffers.FindChecked(T.Players[1]).CardA.Get(), FirstChoice);
		T.Mode->RespondToCardReaction(T.Players[1], FirstOffer, false);
		const int32 SecondOffer = OfferId(T, T.Players[1]);
		TestNotEqual(TEXT("Second pair receives a fresh offer ID"), FirstOffer, SecondOffer);
		TestEqual(TEXT("Decline offers that player's next counter"), T.Mode->ActiveReactionOffers.FindChecked(T.Players[1]).CardA.Get(), SecondChoice);
		TestEqual(TEXT("Other player's concurrent offer remains unchanged"), OfferId(T, T.Players[2]), OtherOffer);
		T.Mode->RespondToCardReaction(T.Players[1], FirstOffer, true);
		CastChecked<ASHPlayerController>(T.Players[1]->GetOwner())->ClientCloseCardReaction_Implementation(FirstOffer);
		TestEqual(TEXT("Stale response and close cannot dismiss the replacement prompt"), ClientOfferId(T.Players[1]), SecondOffer);
		T.Mode->RespondToCardReaction(T.Players[1], SecondOffer, true);
		DeclineRemainingOffers(T);
		T.Advance();
		TestFalse(TEXT("Accepting the second own pair closes the reaction"), T.Mode->HasActiveEffectTasks());
		TestNotNull(TEXT("Declined first choice remains unspent"), T.Players[1]->GetHand()->FindActivationPair(FirstChoice));
		TestNotNull(TEXT("Other player's slower pair remains unspent"), T.Players[2]->GetHand()->FindActivationPair(OtherChoice));
		TestEqual(TEXT("Exactly one of the responding player's pairs was spent"), T.Players[1]->GetHand()->GetVictoryStack()->GetPairCount(), 1);
		TestEqual(TEXT("Other player's prompt closes after second-choice acceptance"), ClientOfferId(T.Players[2]), INDEX_NONE);
	}
	{
		FSHNewEffectsWorld T;
		ASHCard* Invalidated = T.Pair(T.Players[1]->GetHand(), Cancel);
		T.Pair(T.Players[2]->GetHand(), Cancel);
		ASHCard* Target = T.Pair(T.Players[0]->GetHand(), Bodgy);
		T.Mode->RequestStoredPairActivation(T.Players[0], Target);
		const int32 InvalidatedOffer = OfferId(T, T.Players[1]);
		const int32 RemainingOffer = OfferId(T, T.Players[2]);
		ASHCard* Partner = T.Players[1]->GetHand()->FindActivationPair(Invalidated)->CardB;
		T.Players[1]->GetHand()->SetActivationPairState(Invalidated, Partner, EActivationPairState::AbilityEffect);
		T.Mode->RespondToCardReaction(T.Players[1], InvalidatedOffer, true);
		TestTrue(TEXT("A pair that is no longer ready cannot win by responding first"), T.State->bReactionPending);
		TestEqual(TEXT("Invalid acceptance closes only its own prompt"), ClientOfferId(T.Players[1]), INDEX_NONE);
		TestEqual(TEXT("Another valid concurrent offer remains available"), OfferId(T, T.Players[2]), RemainingOffer);
		T.Mode->RespondToCardReaction(T.Players[2], RemainingOffer, true);
		T.Advance();
		TestEqual(TEXT("Invalidated pair was not consumed"), T.Players[1]->GetHand()->GetVictoryStack()->GetPairCount(), 0);
		TestEqual(TEXT("The first valid accepting player wins"), T.Players[2]->GetHand()->GetVictoryStack()->GetPairCount(), 1);
		TestFalse(TEXT("Revalidation does not leave gameplay paused"), T.Mode->HasActiveEffectTasks());
	}
	{
		FSHNewEffectsWorld T;
		T.Pair(T.Players[1]->GetHand(), Cancel);
		T.Pair(T.Players[2]->GetHand(), Cancel);
		ASHCard* FirstTarget = T.Pair(T.Players[0]->GetHand(), Bodgy);
		ASHCard* SecondTarget = T.Pair(T.Players[0]->GetHand(), Bodgy);
		T.Mode->RequestStoredPairActivation(T.Players[0], FirstTarget);
		const int32 ExpiredOffer = OfferId(T, T.Players[2]);
		T.Mode->RespondToCardReaction(T.Players[1], OfferId(T, T.Players[1]), true);
		DeclineRemainingOffers(T);
		T.Advance();
		T.Mode->RequestStoredPairActivation(T.Players[0], SecondTarget);
		const int32 CurrentOffer = OfferId(T, T.Players[2]);
		TestNotEqual(TEXT("A new activation uses a new offer ID"), CurrentOffer, ExpiredOffer);
		T.Mode->RespondToCardReaction(T.Players[2], ExpiredOffer, true);
		CastChecked<ASHPlayerController>(T.Players[2]->GetOwner())->ClientCloseCardReaction_Implementation(ExpiredOffer);
		TestTrue(TEXT("Delayed acceptance from a previous activation does not close this window"), T.State->bReactionPending);
		TestEqual(TEXT("Delayed close from a previous activation preserves the current prompt"), ClientOfferId(T.Players[2]), CurrentOffer);
		T.Mode->RespondToCardReaction(T.Players[2], CurrentOffer, true);
		T.Advance();
		TestEqual(TEXT("Each separately countered activation goes to victory exactly once"), T.Players[0]->GetHand()->GetVictoryStack()->GetPairCount(), 2);
		TestFalse(TEXT("Second counter leaves no pending reaction or effect"), T.Mode->HasActiveEffectTasks());
	}
	{
		FSHNewEffectsWorld T;
		ASHCard* Counter = T.Pair(T.Players[2]->GetHand(), Cancel);
		T.Pair(T.Players[1]->GetHand(), Capture);
		ASHCard* Target = T.Pair(T.Players[0]->GetHand(), Dead);
		ASHCard* Requested = T.Card(T.Players[2]->GetHand(), Bodgy);
		T.Mode->RequestStoredPairActivation(T.Players[0], Target);
		DeclineRemainingOffers(T);
		T.Advance();
		TestTrue(TEXT("Capture lets targeted activation execute"), T.Mode->PendingParticipantSelections.Contains(T.Players[0]));
		TestNotNull(TEXT("Pair stays with its owner until effect finishes"), T.Players[0]->GetHand()->FindActivationPair(Target));
		T.Mode->SubmitParticipantSelection(T.Players[0], T.Players[2]->GetHand());
		T.Advance();
		TestTrue(TEXT("Original activation transferred Bodgy before capture"), T.Players[0]->GetHand()->ContainsCard(Requested));
		T.Mode->RespondToCardReaction(T.Players[1], OfferId(T, T.Players[1]), true);
		DeclineRemainingOffers(T);
		T.Advance();
		const FActivatedPair* Captured = T.Players[1]->GetHand()->FindActivationPair(Target);
		if (TestNotNull(TEXT("Finished pair captured into reaction owner's zone"), Captured))
		{
			TestFalse(TEXT("Captured pair resets activated flag"), Captured->bActivated);
			TestEqual(TEXT("Captured pair is ready"), Captured->State, EActivationPairState::Ready);
		}
		TestEqual(TEXT("Captured card gets new network owner"), Target->GetOwningHand(), T.Players[1]->GetHand());
		TestEqual(TEXT("Original owner gets no victory point for captured pair"), T.Players[0]->GetHand()->GetVictoryStack()->GetPairCount(), 0);
		TestNotNull(TEXT("Declined counter remains unspent"), T.Players[2]->GetHand()->FindActivationPair(Counter));
		TestFalse(TEXT("Capture leaves no active task or offer"), T.Mode->HasActiveEffectTasks());
	}
	{
		FSHNewEffectsWorld T;
		T.Pair(T.Players[1]->GetHand(), Capture);
		ASHCard* Target = T.Pair(T.Players[0]->GetHand(), Bodgy);
		ASHHand* Source = T.Players[2]->GetHand();
		ASHCard* First = T.Card(Source);
		ASHCard* Second = T.Card(Source);
		T.Mode->RequestStoredPairActivation(T.Players[0], Target);
		TestTrue(TEXT("No capture prompt during deferred draw"), T.Mode->ActiveReactionOffers.IsEmpty());
		T.Advance();
		TestNotNull(TEXT("Deferred draw pair not captured prematurely"), T.Players[0]->GetHand()->FindActivationPair(Target));
		T.Draw(T.Players[0], Source, First);
		T.Draw(T.Players[0], Source, Second);
		TestTrue(TEXT("No capture prompt before mandatory return"), T.Mode->ActiveReactionOffers.IsEmpty());
		T.Mode->SubmitHandCardSelection(T.Players[0], First);
		T.Advance();
		T.Mode->RespondToCardReaction(T.Players[1], OfferId(T, T.Players[1]), true);
		T.Advance();
		TestNotNull(TEXT("Capture waits through mandatory return"), T.Players[1]->GetHand()->FindActivationPair(Target));
		TestFalse(TEXT("Deferred capture drains effects"), T.Mode->HasActiveEffectTasks());
	}
	{
		FSHNewEffectsWorld T;
		T.Pair(T.Players[2]->GetHand(), Cancel);
		T.Pair(T.Players[1]->GetHand(), Cancel);
		ASHCard* Target = T.Pair(T.Players[0]->GetHand(), Bodgy);
		T.Mode->RequestStoredPairActivation(T.Players[0], Target);
		const int32 RemainingOffer = OfferId(T, T.Players[1]);
		T.Mode->Logout(CastChecked<ASHPlayerController>(T.Players[2]->GetOwner()));
		TestTrue(TEXT("Disconnecting responder does not freeze the reaction queue"), T.Mode->HasPendingCardReaction());
		TestEqual(TEXT("Disconnected responder's offer is removed"), T.Mode->ActiveReactionOffers.Num(), 1);
		TestEqual(TEXT("Disconnect leaves the other concurrent offer unchanged"), OfferId(T, T.Players[1]), RemainingOffer);
		T.Mode->RespondToCardReaction(T.Players[1], RemainingOffer, false);
		T.Advance();
		TestFalse(TEXT("All declines resume normal activation"), T.State->bReactionPending);
		TestTrue(TEXT("Uncountered deferred draw starts normally"), T.Mode->HasActiveEffectTasks());
	}
	{
		FSHNewEffectsWorld T;
		ASHCard* Apologist = T.Pair(T.Players[2]->GetHand(), Capture);
		ASHCard* OtherPair = T.Pair(T.Players[1]->GetHand());
		ASHCard* Target = T.Pair(T.Players[0]->GetHand(), Hans);
		T.Mode->RequestStoredPairActivation(T.Players[0], Target);
		TestTrue(TEXT("Hans starts without asking Apologists"), T.Mode->ActiveReactionOffers.IsEmpty());
		T.Advance();
		TestEqual(TEXT("Hans has already moved the Apologist before the question"), Apologist->GetOwningHand(), T.Players[1]->GetHand());
		TestEqual(TEXT("Hans has already moved other pairs before the question"), OtherPair->GetOwningHand(), T.Players[0]->GetHand());
		TestFalse(TEXT("Apologist's previous owner gets no prompt"), T.Mode->ActiveReactionOffers.Contains(T.Players[2]));
		TestEqual(TEXT("Capture targets completed Hans"), T.Mode->ReactionTargetA.Get(), Target);
		TestEqual(TEXT("Hans is not scored while capture is undecided"), T.Players[0]->GetHand()->GetVictoryStack()->GetPairCount(), 0);
		T.Mode->RespondToCardReaction(T.Players[1], OfferId(T, T.Players[1]), true);
		T.Advance();
		TestNotNull(TEXT("New owner can use the moved Apologist to capture Hans"), T.Players[1]->GetHand()->FindActivationPair(Target));
		TestEqual(TEXT("Captured Hans does not rotate pairs a second time"), OtherPair->GetOwningHand(), T.Players[0]->GetHand());
		TestFalse(TEXT("Rotation and capture release the turn"), T.Mode->HasActiveEffectTasks());
	}
	for (int32 ExitMode : {0, 1, 2})
	{
		FSHNewEffectsWorld T;
		ASHCard* Apologist = T.Pair(T.Players[1]->GetHand(), Capture);
		ASHCard* Target = T.Pair(T.Players[0]->GetHand(), Dead);
		ASHCard* Requested = T.Card(T.Players[2]->GetHand(), Bodgy);
		T.Mode->RequestStoredPairActivation(T.Players[0], Target);
		T.Mode->SubmitParticipantSelection(T.Players[0], T.Players[2]->GetHand());
		T.Advance();
		const int32 PostOffer = OfferId(T, T.Players[1]);
		TestTrue(TEXT("Post-effect decision holds the authoritative turn pause"), T.State->bReactionPending);
		TestTrue(TEXT("Post-effect decision sees the already transferred card"), T.Players[0]->GetHand()->ContainsCard(Requested));
		if (ExitMode == 0) { T.Mode->RespondToCardReaction(T.Players[1], PostOffer, false); }
		else { T.Mode->Logout(CastChecked<ASHPlayerController>(T.Players[ExitMode == 1 ? 1 : 0]->GetOwner())); }
		T.Advance();
		T.Mode->RespondToCardReaction(T.Players[1], PostOffer, true);
		TestEqual(TEXT("Decline or disconnect finalizes completed root once"), T.Players[0]->GetHand()->GetVictoryStack()->GetPairCount(), 1);
		TestNotNull(TEXT("Unaccepted Apologist remains unspent"), T.Players[1]->GetHand()->FindActivationPair(Apologist));
		TestTrue(TEXT("Post-effect cleanup clears completion queue"), T.Mode->PendingSuccessfulActivations.IsEmpty());
		TestFalse(TEXT("Post-effect cleanup releases all response and task locks"), T.State->bReactionPending || T.Mode->HasActiveEffectTasks());
	}
	{
		FSHNewEffectsWorld T;
		UClass* Collector = Load(TEXT("Card_Gnushor"));
		if (!TestNotNull(TEXT("Collector definition exists"), Collector)) { return false; }
		ASHCard* Apologist = T.Pair(T.Players[1]->GetHand(), Capture);
		ASHCard* Target = T.Pair(T.Players[0]->GetHand(), Collector);
		T.Mode->RequestStoredPairActivation(T.Players[0], Target);
		T.Advance(); T.Advance();
		TestTrue(TEXT("Apologist collected by the completed effect gets no prompt"), T.Mode->ActiveReactionOffers.IsEmpty());
		TestEqual(TEXT("Collected Apologist is already in victory"), Apologist->GetCardZone(), ECardZone::Victory);
		TestEqual(TEXT("Collector finalizes its own pair only once"), T.Players[0]->GetHand()->GetVictoryStack()->GetPairCount(), 1);
		TestFalse(TEXT("Collector drains deferred own-pair completion"), T.Mode->HasActiveEffectTasks());
	}
	for (bool Accept : {false, true})
	{
		FSHNewEffectsWorld T;
		ASHPlayerState* Activator = T.Players[0];
		ASHPlayerState* Reactor = T.Players[1];
		ASHPlayerController* ReactorController = CastChecked<ASHPlayerController>(Reactor->GetOwner());
		ReactorController->bShowMouseCursor = true;
		T.Pair(Reactor->GetHand(), Cancel);
		ASHCard* Target = T.Pair(Activator->GetHand(), Dead);
		for (ASHHand* Hand : T.Hands) { T.Card(Hand); }
		T.Card(T.Hands[1]);
		T.Card(T.Hands[1]);
		// Open headlessly, then attach an isolated viewport before the real response
		// closes the offer. A rules-only fixture misses persistent mouse capture.
		T.Mode->RequestStoredPairActivation(Activator, Target);
		TestTrue(TEXT("Input regression has a pending reaction offer"), T.Mode->HasPendingCardReaction());
		UGameViewportClient* ViewportClient = NewObject<UGameViewportClient>(GEngine);
		FWorldContext& WorldContext = GEngine->GetWorldContextFromWorldChecked(T.World);
		WorldContext.GameViewport = ViewportClient;
		const TSharedRef<SViewport> ViewportWidget = SNew(SViewport);
		TUniquePtr<FSceneViewport> SceneViewport(ViewportClient->CreateGameViewport(ViewportWidget));
		// No game instance or split-screen players are required by this input fixture.
		ViewportClient->Viewport = SceneViewport.Get();
		ULocalPlayer* LocalPlayer = NewObject<ULocalPlayer>(GEngine);
		LocalPlayer->PlayerController = ReactorController;
		ReactorController->Player = LocalPlayer;
		FInputModeGameAndUI InitialMode;
		InitialMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		InitialMode.SetHideCursorDuringCapture(false);
		ReactorController->SetInputMode(InitialMode);

		T.Mode->RespondToCardReaction(Reactor, OfferId(T, Reactor), Accept);
		TestEqual(TEXT("Closing accepted or declined reaction restores temporary mouse capture"),
			ViewportClient->GetMouseCaptureMode(), EMouseCaptureMode::CaptureDuringMouseDown);
		TestEqual(TEXT("Closing reaction leaves mouse unlocked for HUD and card drag"),
			ViewportClient->GetMouseLockMode(), EMouseLockMode::DoNotLock);
		TestFalse(TEXT("Closing reaction does not hide cursor during card drag"), ViewportClient->HideCursorDuringCapture());
		TestTrue(TEXT("Closing reaction preserves visible table cursor"), ReactorController->bShowMouseCursor);

		T.Advance();
		TestFalse(TEXT("Closing reaction and finishing its presentation releases authoritative pause"), T.State->bReactionPending);
		if (!Accept)
		{
			T.Mode->SubmitParticipantSelection(Activator, T.Hands[1]);
			T.Advance();
		}
		UTurnComponent* Turns = T.Mode->GetTurnComponent();
		Turns->SkipCurrentPhase(Activator);
		TestTrue(TEXT("Original activator can draw after reaction closes"), Turns->CanDrawCardFromHand(Activator, T.Hands[1]));
		T.Draw(Activator, T.Hands[1], T.Hands[1]->GetTopCard());
		Turns->SkipCurrentPhase(Activator);
		TestEqual(TEXT("Turn reaches the player who answered the reaction"), T.State->GetCurrentPlayer(), Reactor);
		TestTrue(TEXT("Reactor can draw on their next turn"), Turns->CanDrawCardFromHand(Reactor, T.Hands[1]));
		ASHCard* DrawnCard = T.Hands[1]->GetTopCard();
		// Native fixture cards have no rendered face; exercise draw resolution without its cosmetic RPC.
		T.Draw(Reactor, T.Hands[1], DrawnCard);
		TestTrue(TEXT("Reactor receives their normal turn draw"), Reactor->GetHand()->ContainsCard(DrawnCard));
		TestEqual(TEXT("Reactor reaches second pairing"), T.State->GetTurnPhase(), ETurnPhase::SecondPairing);
		ReactorController->ServerSkipCurrentPhase_Implementation();
		TestEqual(TEXT("Reactor can finish their next turn"), T.State->GetCurrentPlayer(), T.Players[2]);

		ReactorController->Player = nullptr;
		LocalPlayer->PlayerController = nullptr;
		ViewportClient->SetViewport(nullptr);
		WorldContext.GameViewport = nullptr;
	}
	for (bool Protected : {false, true})
	{
		FSHNewEffectsWorld T;
		ASHCard* OwnReaction = T.Pair(T.Players[0]->GetHand(), Cancel);
		if (Protected) { T.Pair(T.Players[1]->GetHand(), Cancel); T.Players[0]->SetProtectedFromCardEffects(true); }
		ASHCard* Target = T.Pair(T.Players[0]->GetHand(), Bodgy);
		TestFalse(TEXT("Reaction cannot be manually activated"), T.Mode->GetTurnComponent()->CanActivatePair(T.Players[0], *T.Players[0]->GetHand()->FindActivationPair(OwnReaction)));
		T.Mode->RequestStoredPairActivation(T.Players[0], Target);
		TestFalse(TEXT("Own turn or protected target does not produce an offer"), T.State->bReactionPending);
	}
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSHSupportPairEffectsTest, "SeaHorse.Gameplay.Effects.SupportPairs",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSHSupportPairEffectsTest::RunTest(const FString& Parameters)
{
	TGuardValue<bool> ScriptGuard(GAllowActorScriptExecutionInEditor, true);
	auto Definition = [this](const TCHAR* Name)
	{
		UClass* Result = LoadClass<UCardDefinition>(nullptr, *FString::Printf(TEXT("/Game/SeaHorse/Cards/Definitions/%s.%s_C"), Name, Name));
		TestNotNull(Name, Result);
		return Result;
	};
	UClass* Dogs = Definition(TEXT("Card_DachshundsSpectralHounds"));
	UClass* Pancho = Definition(TEXT("Card_Pancho"));
	UClass* Counter = Definition(TEXT("Card_GieselbrechtWizardApprentice"));
	UClass* Capture = Definition(TEXT("Card_GieselbrechtApologist"));
	UClass* Dead = Definition(TEXT("Card_GniewDeadHerald"));
	UClass* Bodgy = Definition(TEXT("Card_BodgyVampireHunter"));
	UClass* Slayer = Definition(TEXT("Card_ThronriTrollSlayer"));
	UClass* Collector = Definition(TEXT("Card_Gnushor"));
	if (!Dogs || !Pancho || !Counter || !Capture || !Dead || !Bodgy || !Slayer || !Collector) { return false; }
	auto Accept = [this](FSHNewEffectsWorld& T, ASHPlayerState* Player)
	{
		const ASHGameMode::FReactionOption* Offer = T.Mode->ActiveReactionOffers.Find(Player);
		if (!TestNotNull(TEXT("Expected reaction offered"), Offer)) { return; }
		T.Mode->RespondToCardReaction(Player, Offer->OfferId, true);
	};
	auto Boost = [this, Pancho](FSHNewEffectsWorld& T, ASHCard* Target)
	{
		ASHPlayerState* Player = T.Players[0];
		ASHCard* Support = T.Pair(Player->GetHand(), Pancho);
		T.Mode->RequestStoredPairActivation(Player, Support);
		T.Advance();
		TestTrue(TEXT("Pancho accepts a direct click on the own stored pair"), T.Mode->SubmitActivationPairSelection(Player, Target));
		T.Advance();
		const FActivatedPair* Pair = Player->GetHand()->FindActivationPair(Target);
		TestTrue(TEXT("Selected pair stays Ready with a replicated bonus"), Pair && Pair->bDoubleEffectThisTurn && Pair->State == EActivationPairState::Ready && !Pair->bActivated);
		TestFalse(TEXT("Pancho leaves the activation zone immediately after selection"), Player->GetHand()->FindActivationPair(Support) != nullptr);
		TestFalse(TEXT("Choosing a pair does not start its effect"), T.Mode->HasActiveEffectTasks());
	};
	for (UClass* Reaction : {Counter, Capture})
	{
		FSHNewEffectsWorld T;
		ASHPlayerState* Reactor = T.Players[1];
		ASHHand* Hand = Reactor->GetHand();
		ASHCard* DogPair = T.Pair(Hand, Dogs);
		ASHCard* SecondDogs = T.Pair(Hand, Dogs);
		ASHCard* Gieselbrecht = T.Pair(Hand, Reaction);
		ASHCard* Root = T.Pair(T.Players[0]->GetHand(), Dead);
		TestFalse(TEXT("Dogs are passive and cannot be manually activated"), T.Mode->GetTurnComponent()->CanActivatePair(Reactor, *Hand->FindActivationPair(DogPair)));
		T.Mode->RequestStoredPairActivation(T.Players[0], Root);
		if (Reaction == Capture) { T.Mode->SubmitParticipantSelection(T.Players[0], T.Hands[1]); T.Advance(); }
		Accept(T, Reactor);
		T.Advance();
		TestFalse(TEXT("Oldest dogs pay for a successful Gieselbrecht"), Hand->FindActivationPair(DogPair) != nullptr);
		TestNotNull(TEXT("Only one dog pair is spent"), Hand->FindActivationPair(SecondDogs));
		const FActivatedPair* Kept = Hand->FindActivationPair(Gieselbrecht);
		TestTrue(TEXT("Gieselbrecht is restored and unreserved"), Kept && Kept->State == EActivationPairState::Ready && !Kept->bActivated && !Kept->bActivationQueued);
		TestEqual(TEXT("Dogs award one victory pair"), Hand->GetVictoryStack()->GetPairCount(), 1);
		if (Reaction == Capture)
		{
			TestNotNull(TEXT("Apologist still captures the root after its effect"), Hand->FindActivationPair(Root));
		}
		TestFalse(TEXT("Support leaves no pending task or reaction"), T.Mode->HasActiveEffectTasks());
		ASHCard* NextRoot = T.Pair(T.Players[0]->GetHand(), Dead);
		T.Mode->RequestStoredPairActivation(T.Players[0], NextRoot);
		if (Reaction == Capture) { T.Mode->SubmitParticipantSelection(T.Players[0], T.Hands[1]); T.Advance(); }
		Accept(T, Reactor);
		T.Advance();
		TestFalse(TEXT("Restored Gieselbrecht can use the second dogs on another activation"), Hand->FindActivationPair(SecondDogs) != nullptr);
	}
	{
		FSHNewEffectsWorld T;
		ASHCard* DogPair = T.Pair(T.Players[1]->GetHand(), Dogs);
		ASHCard* Gieselbrecht = T.Pair(T.Players[1]->GetHand(), Counter);
		T.Pair(T.Players[2]->GetHand(), Counter);
		ASHCard* Root = T.Pair(T.Players[0]->GetHand(), Dead);
		T.Mode->RequestStoredPairActivation(T.Players[0], Root);
		Accept(T, T.Players[1]); Accept(T, T.Players[2]); T.Advance();
		TestNotNull(TEXT("Countered Gieselbrecht does not consume dogs"), T.Players[1]->GetHand()->FindActivationPair(DogPair));
		TestFalse(TEXT("Countered Gieselbrecht is spent normally"), T.Players[1]->GetHand()->FindActivationPair(Gieselbrecht) != nullptr);
		T.Mode->SubmitParticipantSelection(T.Players[0], T.Hands[1]); T.Advance();
		TestFalse(TEXT("Counter chain with dogs releases all gameplay locks"), T.Mode->HasActiveEffectTasks());
	}
	{
		FSHNewEffectsWorld T;
		ASHCard* DogPair = T.Pair(T.Players[1]->GetHand(), Dogs);
		ASHCard* Gieselbrecht = T.Pair(T.Players[1]->GetHand(), Counter);
		T.Pair(T.Players[2]->GetHand(), Capture);
		ASHCard* Root = T.Pair(T.Players[0]->GetHand(), Dead);
		T.Mode->RequestStoredPairActivation(T.Players[0], Root);
		Accept(T, T.Players[1]); T.Advance(); Accept(T, T.Players[2]); T.Advance();
		TestNotNull(TEXT("Capture takes Gieselbrecht instead of invoking a victory substitute"), T.Players[2]->GetHand()->FindActivationPair(Gieselbrecht));
		TestNotNull(TEXT("Dogs remain when Gieselbrecht is captured"), T.Players[1]->GetHand()->FindActivationPair(DogPair));
		TestFalse(TEXT("Capturing Gieselbrecht leaves no unresolved effects"), T.Mode->HasActiveEffectTasks());
	}
	{
		FSHNewEffectsWorld T;
		ASHPlayerState* Reactor = T.Players[1];
		ASHCard* DogPair = T.Pair(Reactor->GetHand(), Dogs);
		ASHCard* Gieselbrecht = T.Pair(Reactor->GetHand(), Counter);
		ASHCard* Root = T.Pair(T.Players[0]->GetHand(), Dead);
		T.Mode->RequestStoredPairActivation(T.Players[0], Root);
		UTurnComponent* Turns = T.Mode->GetTurnComponent();
		Turns->BeginTurnTransitionBlock(TEXT("SupportAnimation"));
		Accept(T, Reactor);
		TestNotNull(TEXT("Dogs stay on the table until presentation completes"), Reactor->GetHand()->FindActivationPair(DogPair));
		TestTrue(TEXT("Gieselbrecht remains reserved during replacement presentation"), Reactor->GetHand()->FindActivationPair(Gieselbrecht)->bActivationQueued);
		Turns->FinishTurnTransitionBlock(TEXT("SupportAnimation")); T.Advance();
		TestFalse(TEXT("Animation completion moves dogs to victory"), Reactor->GetHand()->FindActivationPair(DogPair) != nullptr);
		TestFalse(TEXT("Animation completion releases Gieselbrecht reservation"), Reactor->GetHand()->FindActivationPair(Gieselbrecht)->bActivationQueued);
		TestTrue(TEXT("Animation completion restores Gieselbrecht readiness"), Reactor->GetHand()->FindActivationPair(Gieselbrecht)->State == EActivationPairState::Ready);
		TestTrue(TEXT("Replacement drains the root queue and pending presentation moves"), T.Mode->PendingPairActivations.IsEmpty() && T.Mode->CompletedEffectPairsWaitingForPresentation.IsEmpty());
	}
	for (UClass* Reaction : {Counter, Capture})
	{
		FSHNewEffectsWorld T;
		ASHPlayerState* Player = T.Players[0];
		ASHCard* Target = T.Pair(Player->GetHand(), Dead);
		T.Card(T.Hands[1], Bodgy); T.Card(T.Hands[1], Bodgy);
		Boost(T, Target);
		T.Pair(T.Players[1]->GetHand(), Reaction);
		T.Mode->RequestStoredPairActivation(Player, Target);
		if (Reaction == Capture)
		{
			TestTrue(TEXT("Doubled effect has no early capture prompt"), T.Mode->ActiveReactionOffers.IsEmpty());
			T.Mode->SubmitParticipantSelection(Player, T.Hands[1]); T.Advance();
			TestNotNull(TEXT("Capture waits until both doubled executions finish"), Player->GetHand()->FindActivationPair(Target));
			TestTrue(TEXT("No capture prompt between doubled executions"), T.Mode->ActiveReactionOffers.IsEmpty());
			T.Mode->SubmitParticipantSelection(Player, T.Hands[1]); T.Advance();
			Accept(T, T.Players[1]); T.Advance();
			const FActivatedPair* Captured = T.Players[1]->GetHand()->FindActivationPair(Target);
			TestTrue(TEXT("Captured pair is Ready without retaining its used bonus"), Captured && Captured->State == EActivationPairState::Ready && !Captured->bDoubleEffectThisTurn);
			TestEqual(TEXT("Both executions occurred before capture"), Player->GetHand()->GetCardCount(), 2);
		}
		else
		{
			Accept(T, T.Players[1]); T.Advance();
			TestEqual(TEXT("Counter cancels both executions of the doubled activation"), Player->GetHand()->GetCardCount(), 0);
			TestEqual(TEXT("Cancelled doubled pair is consumed once"), Player->GetHand()->GetVictoryStack()->GetPairCount(), 2);
		}
		TestTrue(TEXT("Reaction on a doubled activation leaves no repeat bookkeeping"), T.Mode->RepeatedPairEffects.IsEmpty());
		TestFalse(TEXT("Reaction on a doubled activation releases gameplay"), T.Mode->HasActiveEffectTasks());
	}
	{
		FSHNewEffectsWorld T;
		ASHPlayerState* Player = T.Players[0];
		ASHHand* Hand = Player->GetHand();
		ASHCard* Target = T.Pair(Hand, Dead);
		ASHCard* First = T.Card(T.Hands[1], Bodgy);
		ASHCard* Second = T.Card(T.Players[1]->GetHand(), Bodgy);
		Boost(T, Target);
		T.Mode->RequestStoredPairActivation(Player, Target); T.Advance();
		T.Mode->SubmitParticipantSelection(Player, T.Hands[1]); T.Advance();
		TestTrue(TEXT("First execution transfers one Bodgy"), Hand->ContainsCard(First));
		TestNotNull(TEXT("Pair is not spent between executions"), Hand->FindActivationPair(Target));
		TestTrue(TEXT("Second execution requests a new target"), T.Mode->PendingParticipantSelections.Contains(Player));
		TestFalse(TEXT("Second execution cannot be rolled back with ESC"), T.Mode->CancelEffectTargetSelection(Player, Target, Hand->FindActivationPair(Target)->CardB));
		T.Mode->SubmitParticipantSelection(Player, T.Players[1]->GetHand()); T.Advance();
		TestTrue(TEXT("Second execution can use a different source"), Hand->ContainsCard(Second));
		TestEqual(TEXT("Pancho and the doubled target are each spent once"), Hand->GetVictoryStack()->GetPairCount(), 2);
		TestFalse(TEXT("Two executions finish all targeting and tasks"), T.Mode->HasActiveEffectTasks() || T.Mode->IsWaitingForPlayerSelection());
	}
	for (int32 Targets : {1, 2})
	{
		FSHNewEffectsWorld T;
		ASHPlayerState* Player = T.Players[0];
		ASHCard* Target = T.Pair(Player->GetHand(), Slayer);
		ASHCard* First = T.Pair(T.Players[1]->GetHand());
		ASHCard* Second = Targets == 2 ? T.Pair(T.Players[2]->GetHand()) : nullptr;
		Boost(T, Target);
		T.Mode->RequestStoredPairActivation(Player, Target); T.Advance();
		T.Mode->SubmitActivationPairSelection(Player, First); T.Advance();
		if (Second) { T.Mode->SubmitActivationPairSelection(Player, Second); T.Advance(); }
		TestFalse(TEXT("Doubled slayer is removed even when the second execution has no target"), IsValid(Target));
		TestFalse(TEXT("First chosen pair is removed"), IsValid(First));
		if (Second) { TestFalse(TEXT("Second chosen pair is removed"), IsValid(Second)); }
		TestFalse(TEXT("Removal releases the activation queue"), T.Mode->HasActiveEffectTasks());
	}
	{
		FSHNewEffectsWorld T;
		ASHCard* Target = T.Pair(T.Players[0]->GetHand(), Collector);
		T.Pair(T.Players[1]->GetHand());
		Boost(T, Target);
		T.Mode->RequestStoredPairActivation(T.Players[0], Target); T.Advance();
		T.Advance(); // Two separate VFX periods, including the final victory presentation.
		TestFalse(TEXT("Bulk collection can finish both executions without an early own-pair removal"), T.Mode->HasActiveEffectTasks());
		TestEqual(TEXT("Doubled collector only scores itself once"), T.Players[0]->GetHand()->GetVictoryStack()->GetPairCount(), 2);
	}
	{
		FSHNewEffectsWorld T;
		ASHPlayerState* Player = T.Players[0];
		ASHHand* Source = T.Hands[1];
		ASHCard* Target = T.Pair(Player->GetHand(), Bodgy);
		TArray<ASHCard*> Cards;
		for (int32 I = 0; I < 4; ++I) { Cards.Add(T.Card(Source)); }
		Boost(T, Target);
		T.Mode->RequestStoredPairActivation(Player, Target); T.Advance();
		T.Draw(Player, Source, Cards[0]); T.Draw(Player, Source, Cards[1]);
		T.Mode->SubmitHandCardSelection(Player, Cards[0]); T.Advance();
		TestTrue(TEXT("Repeat continues the deferred draw after the first return"), T.Mode->GetTurnComponent()->CanDrawCardFromHand(Player, Source));
		T.Draw(Player, Source, Cards[2]);
		TestFalse(TEXT("Repeated Bodgy waits for its own second card before asking for a return"), T.Mode->PendingHandCardSelections.Contains(Player));
		T.Draw(Player, Source, Cards[3]);
		T.Mode->SubmitHandCardSelection(Player, Cards[1]);
		TestTrue(TEXT("Second return cannot use a card retained from the first execution"), T.Mode->PendingHandCardSelections.Contains(Player));
		T.Mode->SubmitHandCardSelection(Player, Cards[2]); T.Advance();
		TestEqual(TEXT("Four draws and two returns retain two cards"), Player->GetHand()->GetCardCount(), 2);
		TestEqual(TEXT("Repeated draw returns to second pairing"), T.State->GetTurnPhase(), ETurnPhase::SecondPairing);
		TestFalse(TEXT("Repeated deferred draw releases tasks"), T.Mode->HasActiveEffectTasks());
	}
	{
		FSHNewEffectsWorld T;
		for (ASHHand* Hand : T.Hands) { T.Card(Hand); }
		ASHCard* Target = T.Pair(T.Players[0]->GetHand(), Dead);
		Boost(T, Target);
		T.State->SetTurnPhase(ETurnPhase::SecondPairing);
		T.Mode->GetTurnComponent()->SkipCurrentPhase(T.Players[0]);
		TestEqual(TEXT("Unused bonus does not block ending the turn"), T.State->GetCurrentPlayer(), T.Players[1]);
		TestFalse(TEXT("Unused bonus expires at turn end"), T.Players[0]->GetHand()->FindActivationPair(Target)->bDoubleEffectThisTurn);
	}
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSHDoubledZoneRotationTest, "SeaHorse.Gameplay.Effects.DoubledZoneRotation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSHDoubledZoneRotationTest::RunTest(const FString& Parameters)
{
	TGuardValue<bool> ScriptGuard(GAllowActorScriptExecutionInEditor, true);
	UClass* Pancho = LoadClass<UCardDefinition>(nullptr, TEXT("/Game/SeaHorse/Cards/Definitions/Card_Pancho.Card_Pancho_C"));
	UClass* Hans = LoadClass<UCardDefinition>(nullptr, TEXT("/Game/SeaHorse/Cards/Definitions/Card_HansCaptain.Card_HansCaptain_C"));
	if (!TestNotNull(TEXT("Pancho definition exists"), Pancho) || !TestNotNull(TEXT("Hans definition exists"), Hans)) { return false; }
	for (int32 Humans : {2, 3, 5, 6})
	for (bool Protected : {false, true})
	{
		if (Protected && Humans == 2) { continue; }
		FSHNewEffectsWorld T(Humans, FMath::Min(Humans + 1, 6));
		ASHPlayerState* Activator = T.Players[0];
		TArray<ASHPlayerState*> Participants;
		TArray<ASHCard*> Pairs;
		for (int32 Index = 0; Index < T.Players.Num(); ++Index)
		{
			ASHPlayerState* Player = T.Players[Index];
			ASHCard* Pair = T.Pair(Player->GetHand());
			if (Protected && Index == 1) { Player->SetProtectedFromCardEffects(true); }
			Participants.Add(Player); Pairs.Add(Pair);
		}
		ASHCard* Target = T.Pair(Activator->GetHand(), Hans);
		ASHCard* Support = T.Pair(Activator->GetHand(), Pancho);
		T.Mode->RequestStoredPairActivation(Activator, Support); T.Advance();
		T.Mode->SubmitActivationPairSelection(Activator, Target); T.Advance();
		TestTrue(TEXT("Pancho applies to Hans without activating him"), Activator->GetHand()->FindActivationPair(Target)->bDoubleEffectThisTurn);
		auto Tick = [&T]() { ++GFrameCounter; T.World->GetTimerManager().Tick(0.05f); };
		auto CheckStep = [this, &T, &Participants, &Pairs](int32 Step)
		{
			for (int32 Index = 0; Index < Pairs.Num(); ++Index)
			{
				ASHHand* Expected = Participants[(Index + Pairs.Num() - Step) % Pairs.Num()]->GetHand();
				TestEqual(*FString::Printf(TEXT("Step %d moves pair %d to the correct right-hand neighbour"), Step, Index), Pairs[Index]->GetOwningHand(), Expected);
				TestNotNull(TEXT("Destination contains the pair"), Expected->FindActivationPair(Pairs[Index]));
				int32 Copies = 0;
				for (ASHHand* Hand : T.Hands) { if (Hand->FindActivationPair(Pairs[Index])) { ++Copies; } }
				TestEqual(TEXT("Each pair exists in exactly one zone"), Copies, 1);
			}
			for (ASHHand* Hand : T.Hands) { if (Hand->IsLogicalNPC()) { TestTrue(TEXT("BN is skipped by both rotations"), Hand->GetLogicalActivationPairs().IsEmpty()); } }
		};
		T.Mode->RequestStoredPairActivation(Activator, Target);
		TestTrue(TEXT("First execution starts its own circle"), T.Mode->GetTurnComponent()->HasNamedTurnTransitionBlocks());
		for (int32 TickIndex = 0; TickIndex < 200 && Pairs[0]->GetOwningHand() == Participants[0]->GetHand(); ++TickIndex) { Tick(); }
		CheckStep(1);
		TestTrue(TEXT("First rotation retains the activation until the repeat"), T.Mode->HasActiveEffectTasks());
		TestFalse(TEXT("First destination is not frozen by an overlapping second circle"), T.Mode->GetTurnComponent()->HasNamedTurnTransitionBlocks());
		for (int32 TickIndex = 0; TickIndex < 10; ++TickIndex) { Tick(); }
		CheckStep(1);
		for (int32 TickIndex = 0; TickIndex < 200 && !T.Mode->GetTurnComponent()->HasNamedTurnTransitionBlocks(); ++TickIndex) { Tick(); }
		TestTrue(TEXT("Second execution visibly replays the circle"), T.Mode->GetTurnComponent()->HasNamedTurnTransitionBlocks());
		CheckStep(1);
		for (int32 TickIndex = 0; TickIndex < 200 && T.Mode->HasActiveEffectTasks(); ++TickIndex) { Tick(); }
		CheckStep(2);
		TestFalse(TEXT("Both executions release gameplay and presentation"), T.Mode->HasActiveEffectTasks() || T.Mode->GetTurnComponent()->HasNamedTurnTransitionBlocks());
		TestTrue(TEXT("Repeated rotation leaves no queue entries"), T.Mode->PendingPairActivations.IsEmpty() && T.Mode->RepeatedPairEffects.IsEmpty());
		TestEqual(TEXT("Pancho and Hans each score exactly one pair for the activator"), Activator->GetHand()->GetVictoryStack()->GetPairCount(), 2);
		TestFalse(TEXT("Hans does not rotate with the other pairs"), Activator->GetHand()->FindActivationPair(Target) != nullptr);
	}
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSHPanchoAllCardsTest, "SeaHorse.Gameplay.Effects.PanchoAllDefinitions",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSHPanchoAllCardsTest::RunTest(const FString& Parameters)
{
	TGuardValue<bool> ScriptGuard(GAllowActorScriptExecutionInEditor, true);
	auto Load = [this](const TCHAR* Name)
	{
		UClass* Class = LoadClass<UCardDefinition>(nullptr, *FString::Printf(TEXT("/Game/SeaHorse/Cards/Definitions/%s.%s_C"), Name, Name));
		TestNotNull(Name, Class); return Class;
	};
	UClass* Pancho = Load(TEXT("Card_Pancho"));
	UClass* Gloria = Load(TEXT("Card_Gloria"));
	UClass* Otfried = Load(TEXT("Card_Otfried"));
	UClass* Bodgy = Load(TEXT("Card_BodgyVampireHunter"));
	UClass* SeaHorse = Load(TEXT("Card_SeaHorse"));
	if (!Pancho || !Gloria || !Otfried || !Bodgy || !SeaHorse) { return false; }
	const TArray<FString> Names = {TEXT("Card_Aramdila"), TEXT("Card_BodgyVampireHunter"), TEXT("Card_CrumoUrsula"),
		TEXT("Card_Fimarik"), TEXT("Card_Gloria"), TEXT("Card_GniewDeadHerald"), TEXT("Card_GniewLivingHerald"),
		TEXT("Card_Gnushor"), TEXT("Card_HansCaptain"), TEXT("Card_KurtPriest"), TEXT("Card_OlgaPriest"),
		TEXT("Card_Otfried"), TEXT("Card_Pancho"), TEXT("Card_PaulusSilent"), TEXT("Card_PaulusWitchHunterWu"),
		TEXT("Card_ThronriTrollSlayer"), TEXT("Card_Wilhelm"), TEXT("Card_YeHeshaNightMonk")};
	for (const FString& Name : Names)
	for (int32 Variant = 1; Variant <= (Name == TEXT("Card_Wilhelm") ? 2 : 1); ++Variant)
	{
		UClass* Definition = Load(*Name);
		if (!Definition) { continue; }
		AddInfo(TEXT("Pancho compatibility: ") + Name);
		FSHNewEffectsWorld T;
		ASHPlayerState* Player = T.Players[0];
		ASHHand* Hand = Player->GetHand();
		UTurnComponent* Turns = T.Mode->GetTurnComponent();
		TArray<ASHCard*> Anchors;
		for (ASHHand* H : T.Hands) { Anchors.Add(T.Card(H)); for (int32 I = 1; I < 6; ++I) { T.Card(H); } }
		ASHCard* Victim1 = T.Pair(T.Players[1]->GetHand(), Gloria);
		ASHCard* Victim2 = T.Pair(T.Players[2]->GetHand(), Otfried);
		if (Definition == Pancho)
		{
			Victim1 = T.Pair(Hand, Gloria); Victim2 = T.Pair(Hand, Otfried);
		}
		TArray<ASHCard*> SpecialCards;
		if (Name == TEXT("Card_GniewDeadHerald"))
		{
			SpecialCards.Add(T.Card(T.Players[1]->GetHand(), Bodgy)); SpecialCards.Add(T.Card(T.Players[2]->GetHand(), Bodgy));
		}
		if (Name == TEXT("Card_Wilhelm")) { for (int32 I = 0; I < Variant; ++I) { SpecialCards.Add(T.Card(Hand, SeaHorse)); } }
		ASHCard* Target = T.Pair(Hand, Definition);
		ASHCard* Support = T.Pair(Hand, Pancho);
		T.Mode->RequestStoredPairActivation(Player, Support); T.Advance();
		T.Mode->SubmitActivationPairSelection(Player, Target); T.Advance();
		if (!TestTrue(*Name, Hand->FindActivationPair(Target)->bDoubleEffectThisTurn)) { continue; }
		const bool DrawEffect = Name == TEXT("Card_BodgyVampireHunter") || Name == TEXT("Card_CrumoUrsula") || Name == TEXT("Card_Otfried");
		int32 Draws = 0, Returns = 0, PlayerChoices = 0, ParticipantChoices = 0, PairChoices = 0, Exchanges = 0;
		TArray<ASHCard*> CollectedTargets;
		T.Mode->RequestStoredPairActivation(Player, Target);
		for (int32 Step = 0; Step < 800 && (T.Mode->HasActiveEffectTasks() || !T.Mode->PendingPairActivations.IsEmpty()); ++Step)
		{
			++GFrameCounter; T.World->GetTimerManager().Tick(0.05f);
			if (Turns->HasNamedTurnTransitionBlocks()) { continue; }
			if (const auto* Pending = T.Mode->PendingPlayerSelections.Find(Player))
			{
				ASHPlayerState* Choice = T.Players[1 + PlayerChoices % 2];
				TestTrue(TEXT("Each execution offers its own player target"), Pending->Candidates.Contains(Choice));
				++PlayerChoices; T.Mode->SubmitPlayerSelection(Player, Choice);
			}
			else if (const auto* ParticipantSelection = T.Mode->PendingParticipantSelections.Find(Player))
			{
				ASHHand* Choice = Name == TEXT("Card_GniewDeadHerald") ? T.Players[1 + ParticipantChoices % 2]->GetHand() : T.Hands[1];
				TestTrue(TEXT("Each execution offers a fresh hand target"), ParticipantSelection->Candidates.Contains(Choice));
				++ParticipantChoices; T.Mode->SubmitParticipantSelection(Player, Choice);
			}
			else if (const auto* PairSelection = T.Mode->PendingPairSelections.Find(Player))
			{
				ASHCard* Choice = PairChoices == 0 ? Victim1 : Victim2;
				TestTrue(TEXT("Both executions can select different pairs"), PairSelection->CandidateCards.Contains(Choice));
				for (ASHCard* Previous : CollectedTargets) { TestFalse(TEXT("Olga cannot reselect an already collected pair"), PairSelection->CandidateCards.Contains(Previous)); }
				if (Name == TEXT("Card_OlgaPriest")) { CollectedTargets.Add(Choice); }
				++PairChoices; T.Mode->SubmitActivationPairSelection(Player, Choice);
			}
			else if (const auto* CardSelection = T.Mode->PendingHandCardSelections.Find(Player))
			{
				TArray<ASHCard*> Choice;
				const bool Offering = Name == TEXT("Card_KurtPriest") && CardSelection->SourceHand == Hand;
				const int32 Count = Offering ? FMath::Min(3, CardSelection->CandidateCards.Num()) : 1;
				for (int32 I = 0; I < Count; ++I) { Choice.Add(CardSelection->CandidateCards[I]); }
				if (Offering) { ++Exchanges; }
				if (Name == TEXT("Card_BodgyVampireHunter")) { ++Returns; }
				T.Mode->SubmitHandCardsSelection(Player, Choice);
			}
			else if (DrawEffect && (Draws == 0 || Turns->bWaitingForAdditionalDraw))
			{
				ASHHand* Source = Draws > 0 && Name == TEXT("Card_CrumoUrsula") ? T.Hands[1] : T.Players[1]->GetHand();
				if (Turns->CanDrawCardFromHand(Player, Source)) { T.Draw(Player, Source, Source->GetCards()[0]); ++Draws; }
			}
		}
		T.Advance();
		TestFalse(*FString::Printf(TEXT("%s finishes both executions without a stuck selection"), *Name), T.Mode->HasActiveEffectTasks() || T.Mode->IsWaitingForPlayerSelection());
		TestTrue(TEXT("No repeated activation or queue remains"), T.Mode->RepeatedPairEffects.IsEmpty() && T.Mode->PendingPairActivations.IsEmpty());
		TestEqual(TEXT("The activating pair is spent exactly once"), Hand->GetVictoryStack()->GetPairCount(), Name == TEXT("Card_ThronriTrollSlayer") ? 1 : 2);
		if (DrawEffect)
		{
			TestEqual(TEXT("Additional draws execute twice"), Draws, Name == TEXT("Card_BodgyVampireHunter") ? 4 : 3);
			TestEqual(TEXT("Draw sequence reaches the same turn's second pairing"), T.State->GetTurnPhase(), ETurnPhase::SecondPairing);
			if (Name == TEXT("Card_BodgyVampireHunter")) { TestEqual(TEXT("Bodgy returns one card per execution"), Returns, 2); }
		}
		if (Name == TEXT("Card_Gloria")) { for (int32 I : {1, 2}) { TestEqual(TEXT("Gloria schedules each selected skip"), Turns->PendingSkippedTurns.FindRef(T.Players[I]), 1); } }
		if (Name == TEXT("Card_PaulusSilent")) { for (int32 I : {1, 2}) { TestEqual(TEXT("Paulus sets each selected player's source"), Turns->GetFirstForcedDrawSourceHand(T.Players[I]), T.Hands[1]); } }
		if (Name == TEXT("Card_KurtPriest")) { TestEqual(TEXT("Kurt requests two separate exchanges"), Exchanges, 2); TestEqual(TEXT("Both exchanges preserve own hand size"), Hand->GetCardCount(), 6); }
		if (Name == TEXT("Card_GniewDeadHerald")) { for (ASHCard* Card : SpecialCards) { TestTrue(TEXT("Dead Herald transfers both requested cards"), Hand->ContainsCard(Card)); } }
		if (Name == TEXT("Card_GniewLivingHerald")) { for (ASHCard* Card : {Victim1, Victim2}) { TestNotNull(TEXT("Living Herald steals both eligible pairs"), Hand->FindActivationPair(Card)); } }
		if (Name == TEXT("Card_OlgaPriest") || Name == TEXT("Card_Gnushor")) { for (int32 I : {1, 2}) { TestEqual(TEXT("Collected pairs score once for their own owners"), T.Players[I]->GetHand()->GetVictoryStack()->GetPairCount(), 1); } }
		if (Name == TEXT("Card_ThronriTrollSlayer")) { TestFalse(TEXT("Slayer removes itself and both chosen pairs"), IsValid(Target) || IsValid(Victim1) || IsValid(Victim2)); }
		if (Name == TEXT("Card_Aramdila")) { for (int32 I = 0; I < Anchors.Num(); ++I) { TestEqual(TEXT("Aramdila passes hands left twice, including BN"), Anchors[I]->GetOwningHand(), T.Hands[(I + 2) % T.Hands.Num()]); } }
		if (Name == TEXT("Card_HansCaptain")) { TestNotNull(TEXT("Hans passes the first opponent's pair two human seats right"), T.Players[2]->GetHand()->FindActivationPair(Victim1)); }
		if (Definition == Pancho) { for (ASHCard* Card : {Victim1, Victim2}) { TestTrue(TEXT("Doubled Pancho boosts two separate pairs"), Hand->FindActivationPair(Card)->bDoubleEffectThisTurn); } }
		if (Name == TEXT("Card_Wilhelm")) { for (ASHCard* Card : SpecialCards) { TestEqual(TEXT("Wilhelm transfers available SeaHorses without duplicating them"), Card->GetOwningHand(), T.Hands[1]); } }
		if (Name == TEXT("Card_YeHeshaNightMonk"))
		{
			TSet<ASHCard*> Unique;
			for (ASHHand* H : T.Hands) { TestEqual(TEXT("Two shuffles still deal balanced hands"), H->GetCardCount(), 6); for (ASHCard* Card : H->GetCards()) { Unique.Add(Card); } }
			TestEqual(TEXT("Two shuffles preserve every card exactly once"), Unique.Num(), 24);
		}
		if (Name == TEXT("Card_PaulusWitchHunterWu"))
		{
			TestTrue(TEXT("Doubled protection is active"), Player->IsProtectedFromCardEffects());
			Turns->EndTurn(); Turns->EndTurn(); Turns->EndTurn();
			TestFalse(TEXT("Repeating protection does not extend its next-turn expiry"), Player->IsProtectedFromCardEffects());
		}
	}
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSHDogsReactionChainTest, "SeaHorse.Gameplay.Effects.DogsReactionChainDestinations",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSHDogsReactionChainTest::RunTest(const FString& Parameters)
{
	TGuardValue<bool> ScriptGuard(GAllowActorScriptExecutionInEditor, true);
	auto Load = [this](const TCHAR* Name)
	{
		UClass* Class = LoadClass<UCardDefinition>(nullptr, *FString::Printf(TEXT("/Game/SeaHorse/Cards/Definitions/%s.%s_C"), Name, Name));
		TestNotNull(Name, Class); return Class;
	};
	UClass* Dogs = Load(TEXT("Card_DachshundsSpectralHounds"));
	UClass* Counter = Load(TEXT("Card_GieselbrechtWizardApprentice"));
	UClass* Capture = Load(TEXT("Card_GieselbrechtApologist"));
	UClass* Dead = Load(TEXT("Card_GniewDeadHerald"));
	UClass* Hans = Load(TEXT("Card_HansCaptain"));
	UClass* Collector = Load(TEXT("Card_Gnushor"));
	if (!Dogs || !Counter || !Capture || !Dead || !Hans || !Collector) { return false; }
	for (int32 First : {1, 2})
	for (bool DogsWithFirst : {true, false})
	for (bool SecondCaptures : {false, true})
	for (bool FirstCaptures : {false, true})
	for (UClass* RootDefinition : {Dead, Hans, Collector})
	{
		if (FirstCaptures && SecondCaptures) { continue; }
		// Mixed reactions now occur in separate phases. Rotation/collection of
		// unused Apologists is covered by the post-activation timing scenarios.
		if ((FirstCaptures || SecondCaptures) && RootDefinition != Dead) { continue; }
		FSHNewEffectsWorld T;
		const int32 Second = 3 - First;
		const int32 DogsOwner = DogsWithFirst ? First : Second;
		ASHCard* Root = T.Pair(T.Players[0]->GetHand(), RootDefinition);
		ASHCard* A = T.Pair(T.Players[First]->GetHand(), FirstCaptures ? Capture : Counter);
		ASHCard* B = T.Pair(T.Players[Second]->GetHand(), SecondCaptures ? Capture : Counter);
		ASHCard* Hounds = T.Pair(T.Players[DogsOwner]->GetHand(), Dogs);
		T.Mode->RequestStoredPairActivation(T.Players[0], Root);
		if (FirstCaptures)
		{
			const auto* InitialCounter = T.Mode->ActiveReactionOffers.Find(T.Players[Second]);
			if (!TestNotNull(TEXT("Only the counter is offered before the effect"), InitialCounter)) { return false; }
			T.Mode->RespondToCardReaction(T.Players[Second], InitialCounter->OfferId, false);
			T.Advance();
			T.Mode->SubmitParticipantSelection(T.Players[0], T.Hands[1]); T.Advance();
		}
		for (int32 Index : {First, Second})
		{
			if (SecondCaptures && Index == Second) { T.Advance(); }
			const auto* Offer = T.Mode->ActiveReactionOffers.Find(T.Players[Index]);
			if (!TestNotNull(TEXT("Both reactions are offered in the chosen order"), Offer)) { return false; }
			T.Mode->RespondToCardReaction(T.Players[Index], Offer->OfferId, true);
		}
		T.Advance(); T.Advance();
		if (!FirstCaptures && !SecondCaptures && RootDefinition == Dead)
		{
			TestTrue(TEXT("A countered counter restores the original effect"), T.Mode->PendingParticipantSelections.Contains(T.Players[0]));
			T.Mode->SubmitParticipantSelection(T.Players[0], T.Hands[1]);
			T.Advance();
		}
		// Describe the expected result before the original effect (if restored) runs.
		const TArray<ASHCard*> Cards = {Root, A, B, Hounds};
		TArray<int32> Owners = {0, SecondCaptures ? Second : First, Second, DogsOwner};
		TArray<bool> Victory = {true, !SecondCaptures, DogsWithFirst, !DogsWithFirst};
		if (!SecondCaptures)
		{
			for (int32 Index = 1; Index < Cards.Num(); ++Index)
			{
				if (Victory[Index]) { continue; }
				if (RootDefinition == Hans) { Owners[Index] = (Owners[Index] + 2) % 3; }
				else if (RootDefinition == Collector) { Victory[Index] = true; }
			}
		}
		TArray<int32> ExpectedScores = {0, 0, 0};
		for (int32 Index = 0; Index < Cards.Num(); ++Index)
		{
			ASHHand* Hand = T.Players[Owners[Index]]->GetHand();
			if (Victory[Index])
			{
				++ExpectedScores[Owners[Index]];
				TestEqual(TEXT("Spent pair reaches the expected victory stack"), Cards[Index]->GetOwner(), static_cast<AActor*>(Hand->GetVictoryStack()));
				TestEqual(TEXT("Spent pair has victory zone"), Cards[Index]->GetCardZone(), ECardZone::Victory);
			}
			else
			{
				TestEqual(TEXT("Remaining or moved pair has the expected activation-zone owner"), Cards[Index]->GetOwningHand(), Hand);
				const FActivatedPair* Pair = Hand->FindActivationPair(Cards[Index]);
				TestTrue(TEXT("Surviving pair is ready and unreserved"), Pair && Pair->State == EActivationPairState::Ready && !Pair->bActivated && !Pair->bActivationQueued);
			}
		}
		for (int32 Index = 0; Index < 3; ++Index) { TestEqual(TEXT("Scores match all four pairs without duplication"), T.Players[Index]->GetHand()->GetVictoryStack()->GetPairCount(), ExpectedScores[Index]); }
		TestTrue(TEXT("Chain drains every pending move and activation"), T.Mode->CompletedEffectPairsWaitingForPresentation.IsEmpty() && T.Mode->PendingPairActivations.IsEmpty());
		TestFalse(TEXT("Chain releases all gameplay and presentation locks"), T.Mode->HasActiveEffectTasks() || T.Mode->IsWaitingForPlayerSelection() || T.Mode->GetTurnComponent()->HasNamedTurnTransitionBlocks());
	}
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSHReportedEffectRegressionsTest, "SeaHorse.Gameplay.Effects.ReportedDrawAndRotationRegressions",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSHReportedEffectRegressionsTest::RunTest(const FString& Parameters)
{
	TGuardValue<bool> ScriptGuard(GAllowActorScriptExecutionInEditor, true);
	auto Load = [](const TCHAR* Name) { return LoadClass<UCardDefinition>(nullptr, *FString::Printf(TEXT("/Game/SeaHorse/Cards/Definitions/%s.%s_C"), Name, Name)); };
	UClass* Rotation = Load(TEXT("Card_Aramdila"));
	UClass* Monk = Load(TEXT("Card_YeHeshaNightMonk"));
	UClass* Crumo = Load(TEXT("Card_CrumoUrsula"));
	UClass* Kurt = Load(TEXT("Card_KurtPriest"));
	if (!Rotation || !Monk || !Crumo || !Kurt) { AddError(TEXT("Missing regression card definitions")); return false; }
	{
		FSHNewEffectsWorld T;
		ASHPlayerState* Player = T.Players[0];
		ASHHand* Hand = Player->GetHand();
		ASHPlayerController* PC = CastChecked<ASHPlayerController>(Player->GetOwner());
		T.World->AddController(PC);
		Hand->SetRepresentedPlayerState(Player);
		T.State->OnTurnStateChanged.AddDynamic(PC, &ASHPlayerController::HandleTurnStateChanged);
		ASHCard* ReadyMonk = T.Pair(Hand, Monk);
		ASHCard* Effect = T.Pair(Hand, Rotation);
		for (ASHHand* Other : T.Hands) { T.Card(Other); }
		Hand->RefreshPairActivationAvailability();
		TestEqual(TEXT("Both pairs initially have their indicators"), Hand->LocallyActivatablePairs.Num(), 2);
		T.Mode->RequestStoredPairActivation(Player, Effect); T.Advance();
		TestTrue(TEXT("Monk remains legally activatable after hand rotation"), Hand->CanLocalPlayerActivatePair(ReadyMonk));
		TestTrue(TEXT("Monk indicator is restored without clicking or changing phase"), Hand->LocallyActivatablePairs.Contains(*Hand->FindActivationPair(ReadyMonk)));
		T.State->SetReactionPending(true);
		TestTrue(TEXT("Reaction pause immediately hides the listen-host indicator"), Hand->LocallyActivatablePairs.IsEmpty());
		T.State->SetReactionPending(false);
		TestEqual(TEXT("Ending the pause immediately restores the listen-host indicator"), Hand->LocallyActivatablePairs.Num(), 1);
		const FProperty* PauseProperty = FindFProperty<FProperty>(ASHGameState::StaticClass(), TEXT("bReactionPending"));
		TestTrue(TEXT("Remote clients are notified when reaction pause changes"), PauseProperty && PauseProperty->HasAnyPropertyFlags(CPF_RepNotify));
	}
	for (bool FirstSourceNPC : {false, true})
	for (bool ProtectedAlternative : {false, true})
	for (bool Doubled : {false, true})
	for (bool WithCapture : {false, true})
	{
		FSHNewEffectsWorld T;
		ASHPlayerState* Player = T.Players[0];
		ASHPlayerController* PC = CastChecked<ASHPlayerController>(Player->GetOwner());
		T.World->AddController(PC);
		Player->GetHand()->SetRepresentedPlayerState(Player);
		T.State->OnTurnStateChanged.AddDynamic(PC, &ASHPlayerController::HandleTurnStateChanged);
		ASHCard* StillReady = T.Pair(Player->GetHand(), Monk);
		UTurnComponent* Turns = T.Mode->GetTurnComponent();
		ASHHand* Source = FirstSourceNPC ? T.Hands[1] : T.Players[1]->GetHand();
		ASHCard* Effect = T.Pair(Player->GetHand(), Crumo);
		ASHCard* First = T.Card(Source);
		for (int32 I = 0; I < 6; ++I) { T.Card(Source); }
		if (ProtectedAlternative) { T.Card(T.Players[2]->GetHand()); T.Players[2]->SetProtectedFromCardEffects(true); }
		if (Doubled)
		{
			ASHCard* Pancho = T.Pair(Player->GetHand(), Load(TEXT("Card_Pancho")));
			T.Mode->RequestStoredPairActivation(Player, Pancho); T.Advance();
			T.Mode->SubmitActivationPairSelection(Player, Effect); T.Advance();
		}
		if (WithCapture) { T.Pair(T.Players[1]->GetHand(), Load(TEXT("Card_GieselbrechtApologist"))); }
		T.Mode->RequestStoredPairActivation(Player, Effect); T.Advance();
		Turns->SkipCurrentPhase(Player);
		TestTrue(TEXT("First draw is allowed even without a second source"), Turns->CanDrawCardFromHand(Player, Source));
		// In PIE, Blueprint card-movement timelines can still hold a presentation
		// block when the final draw automatically completes the deferred effect.
		Turns->BeginTurnTransitionBlock(TEXT("DrawPresentation"));
		T.Draw(Player, Source, First);
		TestEqual(TEXT("Impossible extra draw automatically advances to second pairing"), T.State->GetTurnPhase(), ETurnPhase::SecondPairing);
		TestFalse(TEXT("Impossible extra draw releases its waiting flag"), Turns->bWaitingForAdditionalDraw);
		TestTrue(TEXT("Effect completion or its Pancho repeat waits for the outstanding presentation"), T.Mode->HasActiveEffectTasks());
		Turns->FinishTurnTransitionBlock(TEXT("DrawPresentation")); T.Advance();
		if (WithCapture)
		{
			const auto* Offer = T.Mode->ActiveReactionOffers.Find(T.Players[1]);
			if (TestNotNull(TEXT("Completed Crumo can be captured even when its extra draw was unavailable"), Offer))
			{
				T.Mode->RespondToCardReaction(T.Players[1], Offer->OfferId, false); T.Advance();
			}
		}
		TestFalse(TEXT("Impossible extra draw releases all effect and response locks"), T.Mode->HasActiveEffectTasks() || T.State->bReactionPending);
		TestTrue(TEXT("Finishing the draw presentation restores second-pairing controls"), Player->GetHand()->LocallyActivatablePairs.Contains(*Player->GetHand()->FindActivationPair(StillReady)));
		TestEqual(TEXT("Crumo is consumed exactly once without an extra draw"), Player->GetHand()->GetVictoryStack()->GetPairCount(), Doubled ? 2 : 1);
		Turns->SkipCurrentPhase(Player);
		TestEqual(TEXT("Player can end the turn after the unavailable extra draw"), T.State->GetCurrentPlayer(), T.Players[1]);
	}
	int32 ExchangesReturningOldCards = 0;
	for (int32 Seed = 1; Seed <= 12; ++Seed)
	{
		FSHNewEffectsWorld T;
		ASHPlayerState* Player = T.Players[0];
		ASHHand* Hand = Player->GetHand();
		ASHHand* Stack = T.Hands[1];
		TArray<ASHCard*> Original;
		for (int32 I = 0; I < 8; ++I) { Original.Add(T.Card(Stack)); }
		TArray<ASHCard*> Offered;
		for (int32 I = 0; I < 3; ++I) { Offered.Add(T.Card(Hand)); }
		ASHCard* Effect = T.Pair(Hand, Kurt);
		T.Mode->RequestStoredPairActivation(Player, Effect);
		T.Mode->SubmitHandCardsSelection(Player, Offered);
		T.Mode->SubmitParticipantSelection(Player, Stack);
		FMath::RandInit(Seed);
		T.Advance();
		const TArray<ASHCard*> Shuffled = Stack->GetCards();
		TestEqual(TEXT("Shuffle includes both old BN cards and all three offered cards"), Shuffled.Num(), 11);
		bool bDrewOld = false;
		for (int32 I = 0; I < 3; ++I)
		{
			ASHCard* Top = Stack->GetTopCard();
			const auto* Pending = T.Mode->PendingHandCardSelections.Find(Player);
			if (!TestNotNull(TEXT("Exchange offers the next card after shuffling"), Pending)) { break; }
			TestTrue(TEXT("Only current shuffled top can be selected"), Pending->CandidateCards.Num() == 1 && Pending->CandidateCards[0] == Top);
			TestEqual(TEXT("Draw follows the shuffled stack order"), Top, Shuffled[Shuffled.Num() - 1 - I]);
			bDrewOld |= Original.Contains(Top);
			T.Mode->SubmitHandCardSelection(Player, Top);
		}
		ExchangesReturningOldCards += bDrewOld ? 1 : 0;
		TestEqual(TEXT("Exchange returns three cards"), Hand->GetCardCount(), 3);
		TestEqual(TEXT("BN keeps its original card count"), Stack->GetCardCount(), 8);
		TestFalse(TEXT("Exchange fully resolves"), T.Mode->HasActiveEffectTasks());
	}
	TestTrue(TEXT("BN exchange shuffles into the old stack rather than returning every offered trio"), ExchangesReturningOldCards > 0);
	return true;
}
#if WITH_EDITOR
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSHCardSelectionWidgetTest, "SeaHorse.Gameplay.Effects.ConfigurableCardSelectionWidget",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSHCardSelectionWidgetTest::RunTest(const FString& Parameters)
{
	TGuardValue<bool> ScriptGuard(GAllowActorScriptExecutionInEditor, true);
	UClass* Kurt = LoadClass<UCardDefinition>(nullptr, TEXT("/Game/SeaHorse/Cards/Definitions/Card_KurtPriest.Card_KurtPriest_C"));
	auto* Fragment = const_cast<UCardEffectFragment*>(Cast<UCardEffectFragment>(UCardDefinition::FindFragmentByClass(Kurt, UCardEffectFragment::StaticClass())));
	if (!TestNotNull(TEXT("Kurt effect exists"), Fragment)) { return false; }
	TestTrue(TEXT("Selection base is abstract and intended for Designer subclasses"), UCardSelectionPrompt::StaticClass()->HasAnyClassFlags(CLASS_Abstract));
	UWidgetBlueprint* BP = CastChecked<UWidgetBlueprint>(FKismetEditorUtilities::CreateBlueprint(
		UCardSelectionPrompt::StaticClass(), GetTransientPackage(), MakeUniqueObjectName(GetTransientPackage(), UWidgetBlueprint::StaticClass(), TEXT("TestCardSelection")),
		BPTYPE_Normal, UWidgetBlueprint::StaticClass(), UWidgetBlueprintGeneratedClass::StaticClass()));
	if (!BP->WidgetTree) { BP->WidgetTree = NewObject<UWidgetTree>(BP); }
	BP->WidgetTree->RootWidget = BP->WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("CustomConfirm"));
	FKismetEditorUtilities::CompileBlueprint(BP);
	if (!TestNotNull(TEXT("A custom selection Widget Blueprint compiles"), BP->GeneratedClass.Get())) { return false; }
	TGuardValue<TSubclassOf<UCardSelectionPrompt>> ClassGuard(Fragment->SelectionWidgetClass, BP->GeneratedClass.Get());
	FSHNewEffectsWorld T;
	ASHPlayerState* Player = T.Players[0];
	ASHPlayerController* PC = CastChecked<ASHPlayerController>(Player->GetOwner());
	T.World->AddController(PC);
	ULocalPlayer* LocalPlayer = NewObject<ULocalPlayer>(GEngine);
	LocalPlayer->PlayerController = PC; PC->Player = LocalPlayer;
	UGameViewportClient* Viewport = NewObject<UGameViewportClient>(GEngine);
	const TSharedRef<SOverlay> ViewportOverlay = SNew(SOverlay);
	Viewport->SetViewportOverlayWidget(nullptr, ViewportOverlay);
	FWorldContext& Context = GEngine->GetWorldContextFromWorldChecked(T.World);
	Context.GameViewport = Viewport;
	TArray<ASHCard*> Cards;
	for (int32 I = 0; I < 3; ++I) { Cards.Add(T.Card(Player->GetHand())); }
	ASHCard* Effect = T.Pair(Player->GetHand(), Kurt);
	T.Mode->RequestStoredPairActivation(Player, Effect);
	UCardSelectionPrompt* Prompt = PC->SelectionPromptWidget;
	if (TestNotNull(TEXT("Effect-assigned widget opens on the selecting controller"), Prompt))
	{
		TestEqual(TEXT("Widget reports minimum from current request"), Prompt->GetMinimumCards(), 1);
		TestEqual(TEXT("Widget reports maximum from current request"), Prompt->GetMaximumCards(), 3);
		TestEqual(TEXT("Widget exposes the server's candidate list"), Prompt->GetCandidateCards().Num(), 3);
		UButton* Confirm = Cast<UButton>(Prompt->GetWidgetFromName(TEXT("CustomConfirm")));
		if (TestNotNull(TEXT("Custom Designer layout is retained"), Confirm))
		{
			Confirm->OnClicked.AddDynamic(Prompt, &UCardSelectionPrompt::ConfirmSelection);
			Confirm->OnClicked.Broadcast();
			TestTrue(TEXT("Button cannot confirm an empty selection"), T.Mode->PendingHandCardSelections.Contains(Player));
			Prompt->ToggleCardSelection(Effect);
			TestEqual(TEXT("Widget cannot select a card outside the candidates"), Prompt->GetSelectedCardCount(), 0);
			Prompt->ToggleCardSelection(Cards[0]);
			TestTrue(TEXT("Selecting one card enables confirmation"), Prompt->CanConfirmSelection());
			Prompt->ClearSelection();
			TestFalse(TEXT("Clear only deselects cards"), Prompt->CanConfirmSelection());
			Prompt->ToggleCardSelection(Cards[1]);
			Confirm->OnClicked.Broadcast();
			TestTrue(TEXT("Custom button submits one card and advances to recipient selection"), T.Mode->PendingParticipantSelections.Contains(Player));
			TestNull(TEXT("Widget is released after submitting"), PC->SelectionPromptWidget.Get());
			T.Mode->SubmitParticipantSelection(Player, T.Hands[1]); T.Advance();
			TestNotNull(TEXT("The same effect can provide UI for its next selection step"), PC->SelectionPromptWidget.Get());
			Prompt->ToggleCardSelection(T.Hands[1]->GetTopCard()); Prompt->ConfirmSelection(); Prompt->ClearSelection();
			TestTrue(TEXT("Closed widget cannot respond to the new draw request"), T.Mode->PendingHandCardSelections.Contains(Player));
			T.Mode->SubmitHandCardSelection(Player, T.Hands[1]->GetTopCard());
			TestNull(TEXT("Effect completion removes its custom widget"), PC->SelectionPromptWidget.Get());
		}
	}
	Fragment->SelectionWidgetClass = nullptr;
	PC->ClientRequestHandCardsSelection_Implementation(Cards, 1, 3, nullptr);
	TestNull(TEXT("No configured UI means no fallback overlay"), PC->SelectionPromptWidget.Get());
	PC->ClearLocalEffectSelectionState();
	Context.GameViewport = nullptr;
	PC->Player = nullptr; LocalPlayer->PlayerController = nullptr;
	return true;
}
#endif
#endif
