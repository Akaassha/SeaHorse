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
#include "Gameplay/SHHand.h"

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
		TestNotNull(TEXT("Zone rotation skips protected player"), Protected->GetHand()->FindActivationPair(Stored));
		T.Mode->MoveAllActivationPairsToVictoryStacks();
		TestNotNull(TEXT("Global collection skips protected player"), Protected->GetHand()->FindActivationPair(Stored));
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
		T.Mode->RespondToCardReaction(T.Players[1], OfferId(T, T.Players[1]), true);
		T.Mode->RespondToCardReaction(T.Players[2], OfferId(T, T.Players[2]), true);
		T.Advance();
		TestTrue(TEXT("Countering capture permits the original effect to execute"), T.Mode->PendingParticipantSelections.Contains(T.Players[0]));
		T.Mode->SubmitParticipantSelection(T.Players[0], T.Players[2]->GetHand());
		T.Advance();
		TestTrue(TEXT("Original effect completes after its capture was cancelled"), T.Players[0]->GetHand()->ContainsCard(Requested));
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
		ASHCard* Target = T.Pair(T.Players[0]->GetHand(), Hans);
		T.Mode->RequestStoredPairActivation(T.Players[0], Target);
		T.Mode->RespondToCardReaction(T.Players[1], OfferId(T, T.Players[1]), true);
		T.Mode->RespondToCardReaction(T.Players[2], OfferId(T, T.Players[2]), true);
		// Keep the definitions' real presentation locks: the root must wait for all
		// completed reaction moves, including capture, before rotating ready pairs.
		T.Advance();
		T.Advance();
		TestNotNull(TEXT("Hans rotates the captured reaction from player two back to player one"), T.Players[1]->GetHand()->FindActivationPair(FirstCapture));
		TestNull(TEXT("Captured reaction was ready before Hans, so it leaves player two's zone"), T.Players[2]->GetHand()->FindActivationPair(FirstCapture));
		TestEqual(TEXT("Rotated reaction has the final zone's network owner"), FirstCapture->GetOwningHand(), T.Players[1]->GetHand());
		TestNotNull(TEXT("Root Hans finishes and is then captured by the first reacting player"), T.Players[1]->GetHand()->FindActivationPair(Target));
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
		ASHCard* FirstChoice = T.Pair(T.Players[1]->GetHand(), Capture);
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
		TestEqual(TEXT("Decline offers that player's other reaction kind"), T.Mode->ActiveReactionOffers.FindChecked(T.Players[1]).CardA.Get(), SecondChoice);
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
		T.Mode->RespondToCardReaction(T.Players[1], OfferId(T, T.Players[1]), true);
		DeclineRemainingOffers(T);
		T.Advance();
		TestTrue(TEXT("Capture lets targeted activation execute"), T.Mode->PendingParticipantSelections.Contains(T.Players[0]));
		TestNotNull(TEXT("Pair stays with its owner until effect finishes"), T.Players[0]->GetHand()->FindActivationPair(Target));
		T.Mode->SubmitParticipantSelection(T.Players[0], T.Players[2]->GetHand());
		T.Advance();
		TestTrue(TEXT("Original activation transferred Bodgy before capture"), T.Players[0]->GetHand()->ContainsCard(Requested));
		const FActivatedPair* Captured = T.Players[1]->GetHand()->FindActivationPair(Target);
		if (TestNotNull(TEXT("Finished pair captured into reaction owner's zone"), Captured))
		{
			TestFalse(TEXT("Captured pair resets activated flag"), Captured->bActivated);
			TestEqual(TEXT("Captured pair is ready"), Captured->State, EActivationPairState::Ready);
		}
		TestEqual(TEXT("Captured card gets new network owner"), Target->GetOwningHand(), T.Players[1]->GetHand());
		TestEqual(TEXT("Original owner gets no victory point for captured pair"), T.Players[0]->GetHand()->GetVictoryStack()->GetPairCount(), 0);
		TestNotNull(TEXT("Earlier counter loses to the faster capture response and is not consumed"), T.Players[2]->GetHand()->FindActivationPair(Counter));
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
		T.Mode->RespondToCardReaction(T.Players[1], OfferId(T, T.Players[1]), true);
		T.Advance();
		TestNotNull(TEXT("Deferred draw pair not captured prematurely"), T.Players[0]->GetHand()->FindActivationPair(Target));
		T.Draw(T.Players[0], Source, First);
		T.Draw(T.Players[0], Source, Second);
		T.Mode->SubmitHandCardSelection(T.Players[0], First);
		T.Advance();
		TestNotNull(TEXT("Capture waits through mandatory return"), T.Players[1]->GetHand()->FindActivationPair(Target));
		TestFalse(TEXT("Deferred capture drains effects"), T.Mode->HasActiveEffectTasks());
	}
	{
		FSHNewEffectsWorld T;
		T.Pair(T.Players[2]->GetHand(), Cancel);
		T.Pair(T.Players[1]->GetHand(), Capture);
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
		TestFalse(TEXT("Closing reaction releases authoritative pause"), T.State->bReactionPending);

		T.Advance();
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
#endif
