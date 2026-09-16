#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Algo/RandomShuffle.h"
#include "Engine/World.h"
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
	~FSHNewEffectsWorld() { World->DestroyWorld(false); }
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
#endif
