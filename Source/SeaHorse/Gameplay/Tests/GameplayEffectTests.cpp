#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Engine/World.h"
#include "Gameplay/Core/SHGameState.h"
#include "Gameplay/Core/SHPlayerState.h"
#include "Gameplay/Core/SHPlayerController.h"
#include "Gameplay/Player/SHPlayerRepresentation.h"
#include "Gameplay/Cards/SHCard.h"
#include "Gameplay/Components/TurnComponent.h"
#include "UObject/UnrealType.h"

namespace
{
struct FEffectTestWorld
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	ASHGameState* State;
	ASHPlayerState* A;
	ASHPlayerState* B;
	ASHHand* HandA;
	ASHHand* HandB;
	ASHHand* ThirdHand;
	UTurnComponent* Turns;
	FEffectTestWorld()
	{
		State = World->SpawnActor<ASHGameState>();
		World->SetGameState(State);
		A = World->SpawnActor<ASHPlayerState>();
		B = World->SpawnActor<ASHPlayerState>();
		State->AddPlayerState(A);
		State->AddPlayerState(B);
		A->SetSeatIndex(0);
		B->SetSeatIndex(2);
		HandA = World->SpawnActor<ASHHand>();
		HandB = World->SpawnActor<ASHHand>();
		ThirdHand = World->SpawnActor<ASHHand>();
		ASHHand* FourthHand = World->SpawnActor<ASHHand>();
		A->SetHand(HandA);
		B->SetHand(HandB);
		State->SetParticipantHands({HandA, ThirdHand, HandB, FourthHand});
		Turns = NewObject<UTurnComponent>(State);
		Turns->RegisterComponent();
		Turns->InitializeTurns(A);
	}
	~FEffectTestWorld() { World->DestroyWorld(false); }
	void EndTurn()
	{
		State->SetTurnPhase(ETurnPhase::SecondPairing);
		Turns->CompleteCurrentPhase(ETurnPhaseEndReason::PlayerSkipped);
	}
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSHSkippedTurnDurationTest, "SeaHorse.Gameplay.Effects.SkippedTurnDuration",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSHSkippedTurnDurationTest::RunTest(const FString& Parameters)
{
	FEffectTestWorld T;
	T.Turns->ScheduleSkippedTurn(T.B);
	T.EndTurn();
	TestEqual(TEXT("Skipping the other human gives the current player another turn"), T.State->GetCurrentPlayer(), T.A);
	T.EndTurn();
	TestEqual(TEXT("The affected player gets their following turn"), T.State->GetCurrentPlayer(), T.B);
	T.EndTurn();
	T.EndTurn();
	TestEqual(TEXT("The consumed effect does not skip later rounds"), T.State->GetCurrentPlayer(), T.B);
	T.Turns->ScheduleSkippedTurn(T.B);
	T.EndTurn();
	TestEqual(TEXT("Self-targeting does not skip somebody else"), T.State->GetCurrentPlayer(), T.A);
	T.EndTurn();
	TestEqual(TEXT("Self-targeting skips the next scheduled turn"), T.State->GetCurrentPlayer(), T.A);
	T.EndTurn();
	TestEqual(TEXT("Self-targeted skip also expires"), T.State->GetCurrentPlayer(), T.B);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSHGameplayEffectInputTest, "SeaHorse.Gameplay.Effects.SelectionAndGuidedDrag",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSHGameplayEffectInputTest::RunTest(const FString& Parameters)
{
	FEffectTestWorld T;
	UClass* ControllerClass = LoadClass<ASHPlayerController>(nullptr,
		TEXT("/Game/SeaHorse/Core/BP_SHPlayerController.BP_SHPlayerController_C"));
	if (!TestNotNull(TEXT("Actual gameplay controller Blueprint"), ControllerClass)) { return false; }
	ASHPlayerController* PC = T.World->SpawnActor<ASHPlayerController>(ControllerClass);
	ASHPlayerRepresentation* Picker = T.World->SpawnActor<ASHPlayerRepresentation>();
	T.HandB->SetRepresentedPlayerState(T.B);
	Picker->BindToHand(T.HandB);
	PC->ClientRequestPlayerSelection_Implementation({T.B}, EPlayerSelectionPurpose::PlayerToDrawFrom);
	if (const FBoolProperty* LegacyFlag = FindFProperty<FBoolProperty>(ControllerClass, TEXT("IsSelectingPlayer")))
	{
		TestFalse(TEXT("Native selection does not arm the competing Blueprint selector"), LegacyFlag->GetPropertyValue_InContainer(PC));
	}
	TestTrue(TEXT("An invalid click is consumed"), PC->TryHandleEffectSelectionClick(nullptr));
	TestEqual(TEXT("An invalid click preserves the current choice"), PC->LocalPlayerSelectionCandidates.Num(), 1);
	TestTrue(TEXT("A picker click is consumed by native selection"), PC->TryHandleEffectSelectionClick(Picker));
	TestTrue(TEXT("A valid picker completes the local choice"), PC->LocalPlayerSelectionCandidates.IsEmpty());
	TestTrue(TEXT("Clicks wait for the next authoritative selection step"), PC->TryHandleEffectSelectionClick(nullptr));
	PC->ClientRequestPlayerSelection_Implementation({T.B}, EPlayerSelectionPurpose::PlayerToDrawFrom);
	TestFalse(TEXT("Next selection request releases the response wait"), PC->bAwaitingPlayerSelectionResponse);
	PC->ClearLocalEffectSelectionState();
	ASHCard* Card = T.World->SpawnActor<ASHCard>();
	Card->SetOwner(T.HandB);
	PC->ClientSetGuidedDrawHands_Implementation({T.HandB});
	TestFalse(TEXT("Guidance does not intercept the normal drag input"), PC->TryHandleEffectSelectionClick(Card));
	TestFalse(TEXT("Guided card click does not perform a draw"), PC->TrySubmitParticipantSelectionForCard(Card));
	TestEqual(TEXT("Card still belongs to its source before drop"), Card->GetOwningHand(), T.HandB);
	TestEqual(TEXT("A click does not consume draw guidance"), PC->LocalGuidedDrawHands.Num(), 1);
	PC->LocalParticipantSelectionCandidates.Add(T.HandB);
	TestTrue(TEXT("Actual participant selection still consumes card clicks"), PC->TryHandleEffectSelectionClick(Card));
	TestTrue(TEXT("Participant choice clears after a valid click"), PC->LocalParticipantSelectionCandidates.IsEmpty());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSHDrawRulesTest, "SeaHorse.Gameplay.Effects.DrawSourceRestrictions",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSHDrawRulesTest::RunTest(const FString& Parameters)
{
	FEffectTestWorld T;
	T.HandB->AddCard(T.World->SpawnActor<ASHCard>(), 0);
	T.ThirdHand->AddCard(T.World->SpawnActor<ASHCard>(), 0);
	T.Turns->SetForcedDrawSourceHand(T.A, T.HandB);
	TestTrue(TEXT("Forced draw permits its chosen source"), T.Turns->CanDrawCardFromHand(T.A, T.HandB));
	TestFalse(TEXT("Forced draw rejects other sources"), T.Turns->CanDrawCardFromHand(T.A, T.ThirdHand));
	T.Turns->HandleCardDrawnFromHand(T.A, T.HandB);
	T.State->SetTurnPhase(ETurnPhase::DrawCard);
	TestTrue(TEXT("Forced source expires after one successful draw"), T.Turns->CanDrawCardFromHand(T.A, T.ThirdHand));
	T.Turns->AdditionalDrawPlayer = T.A;
	T.Turns->FirstDrawSourceHand = T.HandB;
	T.Turns->bWaitingForAdditionalDraw = true;
	T.Turns->AdditionalDrawSourceRule = EAdditionalDrawSourceRule::SamePlayer;
	TestTrue(TEXT("Additional same-source draw remains legal"), T.Turns->CanDrawCardFromHand(T.A, T.HandB));
	TestFalse(TEXT("Additional same-source draw rejects another source"), T.Turns->CanDrawCardFromHand(T.A, T.ThirdHand));
	T.Turns->AdditionalDrawSourceRule = EAdditionalDrawSourceRule::DifferentPlayer;
	TestFalse(TEXT("Additional different-source draw rejects the original source"), T.Turns->CanDrawCardFromHand(T.A, T.HandB));
	TestTrue(TEXT("Additional different-source draw permits another source"), T.Turns->CanDrawCardFromHand(T.A, T.ThirdHand));
	TestFalse(TEXT("Another player cannot perform the additional draw"), T.Turns->CanDrawCardFromHand(T.B, T.ThirdHand));
	return true;
}

#endif
