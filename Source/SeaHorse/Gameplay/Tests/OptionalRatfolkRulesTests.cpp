#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Engine/World.h"
#include "Gameplay/Core/SHGameMode.h"
#include "Gameplay/Core/SHGameState.h"
#include "Gameplay/Core/SHPlayerState.h"
#include "Gameplay/Components/TurnComponent.h"
#include "Gameplay/Cards/CardDefinition.h"
#include "Gameplay/Cards/SHCard.h"
#include "Gameplay/Board/VictoryStack.h"
#include "Gameplay/SHHand.h"

struct FSHOptionalRulesWorld
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	ASHGameMode* Mode = World->SpawnActor<ASHGameMode>();
	ASHGameState* State = World->SpawnActor<ASHGameState>();
	TArray<ASHHand*> Hands;
	TArray<ASHPlayerState*> Players;
	FSHOptionalRulesWorld()
	{
		World->SetGameState(State);
		Mode->GameState = State;
		Mode->TurnComponent = NewObject<UTurnComponent>(Mode);
		for (int32 Seat = 0; Seat < 3; ++Seat)
		{
			auto* Hand = World->SpawnActor<ASHHand>();
			Hand->SetLayoutSeatIndex(Seat);
			Hand->VictoryStack = World->SpawnActor<AVictoryStack>();
			Hands.Add(Hand);
			if (Seat < 2)
			{
				auto* Player = World->SpawnActor<ASHPlayerState>();
				Player->SetSeatIndex(Seat);
				Player->SetHand(Hand);
				State->AddPlayerState(Player);
				Players.Add(Player);
			}
			else { Hand->SetIsNPC(true); }
		}
		State->SetParticipantHands(Hands);
		Mode->TurnComponent->InitializeTurns(Players[0]);
	}
	~FSHOptionalRulesWorld() { World->DestroyWorld(false); }
	ASHCard* Card(ASHHand* Hand, UClass* Definition)
	{
		auto* Card = World->SpawnActor<ASHCard>();
		Card->CardDefinition = Definition;
		Hand->AddCard(Card, Hand->GetCardCount());
		return Card;
	}
};

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSHOptionalRatfolkRulesTest, "SeaHorse.Gameplay.Rules.OptionalRatfolk",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSHOptionalRatfolkRulesTest::RunTest(const FString& Parameters)
{
	auto Load = [](const TCHAR* Name) { return LoadClass<UCardDefinition>(nullptr, *FString::Printf(TEXT("/Game/SeaHorse/Cards/Definitions/%s.%s_C"), Name, Name)); };
	UClass* Rats = Load(TEXT("Card_RatfolkUnderground"));
	UClass* Silent = Load(TEXT("Card_PaulusSilent"));
	UClass* Wu = Load(TEXT("Card_PaulusWitchHunterWu"));
	UClass* Kurt = Load(TEXT("Card_KurtPriest"));
	if (!TestNotNull(TEXT("Ratfolk definition"), Rats) || !TestNotNull(TEXT("Silent Paulus"), Silent) ||
		!TestNotNull(TEXT("Wu Paulus"), Wu) || !TestNotNull(TEXT("Unrelated definition"), Kurt)) { return false; }

	for (UClass* Paulus : {Silent, Wu})
	for (bool bRemoveOther : {false, true})
	for (int32 Location = 0; Location < 4; ++Location)
	{
		FSHOptionalRulesWorld T;
		FSHOptionalRules Rules;
		Rules.bRemoveOtherPaulusAfterRatfolkPair = bRemoveOther;
		T.State->SetOptionalRules(Rules);
		ASHCard* Rat = T.Card(T.Hands[0], Rats);
		ASHCard* Partner = T.Card(T.Hands[0], Paulus);
		ASHHand* OtherHand = Location == 1 ? T.Hands[2] : T.Hands[1];
		ASHCard* Other = T.Card(OtherHand, Paulus);
		ASHCard* Mate = nullptr;
		if (Location >= 2)
		{
			Mate = T.Card(OtherHand, Kurt);
			OtherHand->RemoveCard(Other);
			OtherHand->RemoveCard(Mate);
			if (Location == 2) { OtherHand->AddActivationPairToLogicalHand(Other, Mate); }
			else { OtherHand->GetVictoryStack()->AddPair(Other, Mate); }
		}
		T.Mode->ActivatePair(T.Players[0], Rat, Partner);
		TestEqual(TEXT("Mixed pair reaches victory regardless of toggle"), T.Hands[0]->GetVictoryStack()->GetPairCount(), 1);
		TestEqual(TEXT("Toggle controls other Paulus in hand, BN, activation and victory"), IsValid(Other), !bRemoveOther);
		TestFalse(TEXT("Ratfolk pairing never starts a Paulus activation"), T.Mode->HasActiveEffectTasks());
		if (Location == 2)
		{
			TestEqual(TEXT("Surviving activation card returns to hand only when partner removed"), OtherHand->ContainsCard(Mate), bRemoveOther);
			TestEqual(TEXT("Disabled rule preserves the complete activation pair"), OtherHand->GetLogicalActivationPairs().Num(), bRemoveOther ? 0 : 1);
		}
	}

	for (UClass* Paulus : {Silent, Wu})
	for (ECardZone Zone : {ECardZone::Hand, ECardZone::Activation, ECardZone::Deck, ECardZone::Victory, ECardZone::None})
	{
		FSHOptionalRulesWorld T;
		FSHOptionalRules Rules;
		Rules.bAllowOrphanedRatfolkRemoval = true;
		T.State->SetOptionalRules(Rules);
		ASHCard* Rat = T.Card(T.Hands[0], Rats);
		ASHCard* Other = T.Card(T.Hands[2], Paulus);
		Other->SetCardZone(Zone);
		const bool bNoPartnerInPlay = Zone == ECardZone::Victory || Zone == ECardZone::None;
		TestEqual(TEXT("Both Paulus variants block optional removal while still in active play, including BN"),
			T.Mode->CanRemoveOrphanedRatfolk(T.Players[0], Rat), bNoPartnerInPlay);
	}

	{
		FSHOptionalRulesWorld T;
		ASHCard* Rat = T.Card(T.Hands[0], Rats);
		ASHCard* Spare = T.Card(T.Hands[0], Kurt);
		T.Card(T.Hands[1], Kurt);
		T.Card(T.Hands[2], Kurt); // Keep at least one card per participant after the discard.
		TestFalse(TEXT("Disabled optional rule cannot discard an orphaned card"), T.Mode->RequestRemoveOrphanedRatfolk(T.Players[0], Rat));
		FSHOptionalRules Rules;
		Rules.bAllowOrphanedRatfolkRemoval = true;
		T.State->SetOptionalRules(Rules);
		TestFalse(TEXT("Cannot discard someone else's card"), T.Mode->RequestRemoveOrphanedRatfolk(T.Players[1], Rat));
		TestFalse(TEXT("Cannot discard unrelated cards"), T.Mode->RequestRemoveOrphanedRatfolk(T.Players[0], Spare));
		T.State->SetCurrentPlayer(T.Players[1]);
		TestFalse(TEXT("Cannot discard outside own turn"), T.Mode->RequestRemoveOrphanedRatfolk(T.Players[0], Rat));
		T.State->SetCurrentPlayer(T.Players[0]);
		T.State->SetTurnPhase(ETurnPhase::DrawCard);
		TestFalse(TEXT("Discard cannot bypass drawing"), T.Mode->RequestRemoveOrphanedRatfolk(T.Players[0], Rat));
		T.State->SetTurnPhase(ETurnPhase::SecondPairing);
		T.Mode->GetTurnComponent()->BeginTurnTransitionBlock(TEXT("TestPresentation"));
		TestFalse(TEXT("Cannot alter a hand during a pending presentation"), T.Mode->RequestRemoveOrphanedRatfolk(T.Players[0], Rat));
		T.Mode->GetTurnComponent()->FinishTurnTransitionBlock(TEXT("TestPresentation"));
		TestTrue(TEXT("Player can choose to remove an orphaned Ratfolk"), T.Mode->RequestRemoveOrphanedRatfolk(T.Players[0], Rat));
		TestFalse(TEXT("Removed Ratfolk is destroyed"), IsValid(Rat));
		TestEqual(TEXT("Other own cards remain"), T.Hands[0]->GetCardCount(), 1);
		TestEqual(TEXT("Optional discard awards no victory pair"), T.Hands[0]->GetVictoryStack()->GetPairCount(), 0);
		TestFalse(TEXT("Optional discard does not consume pairing action"), T.Mode->GetTurnComponent()->IsPairingActionUsed());
		TestFalse(TEXT("Repeated stale request is rejected"), T.Mode->RequestRemoveOrphanedRatfolk(T.Players[0], Rat));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSHAutomaticRatfolkRulesTest, "SeaHorse.Gameplay.Rules.AutomaticOrphanedRatfolk",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSHAutomaticRatfolkRulesTest::RunTest(const FString& Parameters)
{
	auto Load = [](const TCHAR* Name) { return LoadClass<UCardDefinition>(nullptr, *FString::Printf(TEXT("/Game/SeaHorse/Cards/Definitions/%s.%s_C"), Name, Name)); };
	UClass* Rats = Load(TEXT("Card_RatfolkUnderground"));
	UClass* Silent = Load(TEXT("Card_PaulusSilent"));
	UClass* Wu = Load(TEXT("Card_PaulusWitchHunterWu"));
	UClass* Kurt = Load(TEXT("Card_KurtPriest"));
	if (!Rats || !Silent || !Wu || !Kurt) { AddError(TEXT("Missing card definitions")); return false; }
	for (bool bEnabled : {false, true})
	for (bool bRemoveOther : {false, true})
	for (bool bDeferPresentation : {false, true})
	{
		FSHOptionalRulesWorld T;
		FSHOptionalRules Rules;
		Rules.bAllowOrphanedRatfolkRemoval = bEnabled;
		Rules.bRemoveOtherPaulusAfterRatfolkPair = bRemoveOther;
		T.State->SetOptionalRules(Rules);
		T.Players[1]->SetProtectedFromCardEffects(true);
		T.State->SetTurnPhase(ETurnPhase::SecondPairing);
		TArray<ASHCard*> UnpairedRats;
		for (ASHHand* Hand : T.Hands) { UnpairedRats.Add(T.Card(Hand, Rats)); T.Card(Hand, Kurt); }
		ASHCard* SilentA = T.Card(T.Hands[0], Silent);
		ASHCard* SilentB = T.Card(T.Hands[0], Silent);
		ASHCard* WuA = T.Card(T.Hands[1], Wu);
		ASHCard* WuB = T.Card(T.Hands[1], Wu);
		ASHCard* ScoredRat = T.Card(T.Hands[0], Rats);
		ASHCard* ScoredMate = T.Card(T.Hands[0], Silent);
		T.Hands[0]->RemoveCard(ScoredRat);
		T.Hands[0]->RemoveCard(ScoredMate);
		T.Hands[0]->GetVictoryStack()->AddPair(ScoredRat, ScoredMate);
		T.Mode->ActivatePair(T.Players[0], SilentA, SilentB);
		T.Mode->ActivatePair(T.Players[1], WuA, WuB);
		T.Mode->RemoveOrphanedRatfolkAutomatically();
		TestTrue(TEXT("Pairing all four Paulus cards does not remove Ratfolk while they remain in activation"), IsValid(UnpairedRats[0]));
		T.Mode->MovePairToVictoryStack(T.Players[0], SilentA, SilentB);
		TestTrue(TEXT("The other Paulus variant in activation still blocks removal"), IsValid(UnpairedRats[0]));
		if (bDeferPresentation) { T.Mode->GetTurnComponent()->BeginTurnTransitionBlock(TEXT("RatfolkRegression")); }
		T.Mode->MovePairToVictoryStack(T.Players[1], WuA, WuB);
		if (bDeferPresentation)
		{
			TestTrue(TEXT("Ratfolk survive while the final pair's presentation is pending"), IsValid(UnpairedRats[0]));
			T.Mode->GetTurnComponent()->FinishTurnTransitionBlock(TEXT("RatfolkRegression"));
			// This fixture spawns a GameMode without installing it as the world's authority GameMode.
			T.Mode->FlushCompletedEffectPairs();
		}
		for (ASHCard* Rat : UnpairedRats)
		{
			TestEqual(TEXT("Lobby option removes all human and BN Ratfolk after the last Paulus leaves activation"), IsValid(Rat), !bEnabled);
		}
		TestTrue(TEXT("Scored Ratfolk remain on the victory stack"), IsValid(ScoredRat));
		TestEqual(TEXT("Cleanup awards no extra points"), T.Hands[0]->GetVictoryStack()->GetPairCount(), 2);
		TestEqual(TEXT("Other player's Paulus pair scores normally"), T.Hands[1]->GetVictoryStack()->GetPairCount(), 1);
		TestFalse(TEXT("Cleanup consumes no pairing action"), T.Mode->GetTurnComponent()->IsPairingActionUsed());
		for (ASHHand* Hand : T.Hands) { TestEqual(TEXT("Unrelated cards survive cleanup"), Hand->GetCardCount(), bEnabled ? 1 : 2); }
	}
	for (UClass* Paulus : {Silent, Wu})
	for (ECardZone Zone : {ECardZone::Hand, ECardZone::Deck, ECardZone::Activation})
	{
		FSHOptionalRulesWorld T;
		FSHOptionalRules Rules;
		Rules.bAllowOrphanedRatfolkRemoval = true;
		T.State->SetOptionalRules(Rules);
		ASHCard* Rat = T.Card(T.Hands[0], Rats);
		ASHCard* Partner = T.Card(T.Hands[2], Paulus);
		Partner->SetCardZone(Zone);
		T.Mode->RemoveOrphanedRatfolkAutomatically();
		TestTrue(TEXT("A remaining Paulus in deck, hand or activation blocks automatic removal"), IsValid(Rat));
	}
	{
		FSHOptionalRulesWorld T;
		FSHOptionalRules Rules;
		Rules.bAllowOrphanedRatfolkRemoval = true;
		T.State->SetOptionalRules(Rules);
		ASHCard* Rat = T.Card(T.Hands[0], Rats);
		T.Card(T.Hands[1], Kurt);
		T.Card(T.Hands[2], Kurt);
		TestTrue(TEXT("Game completion counts hands after automatic removal"), T.Mode->TryFinishGame());
		TestFalse(TEXT("Orphaned Ratfolk cannot postpone the end of the game"), IsValid(Rat));
	}
	return true;
}
#endif
