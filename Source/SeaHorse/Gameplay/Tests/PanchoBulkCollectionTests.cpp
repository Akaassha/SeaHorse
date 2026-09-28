#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Engine/World.h"
#include "UObject/UnrealType.h"
#include "Gameplay/Core/SHGameMode.h"
#include "Gameplay/Core/SHGameState.h"
#include "Gameplay/Core/SHPlayerState.h"
#include "Gameplay/Cards/SHCard.h"
#include "Gameplay/Cards/CardDefinition.h"
#include "Gameplay/Board/VictoryStack.h"
#include "Gameplay/SHHand.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSHPanchoBulkCollectionTest,
	"SeaHorse.Gameplay.Effects.PanchoRefundDuringBulkCollection",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSHPanchoBulkCollectionTest::RunTest(const FString& Parameters)
{
	// The support may belong to a player listed before or after the transferred
	// target. A simultaneous collection must produce the same refund in both cases.
	for (const int32 SupportSeat : {0, 1})
	{
		UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
		ASHGameMode* Mode = World->SpawnActor<ASHGameMode>();
		ASHGameState* State = World->SpawnActor<ASHGameState>();
		World->SetGameState(State);
		FindFProperty<FObjectProperty>(Mode->GetClass(), TEXT("GameState"))->SetObjectPropertyValue_InContainer(Mode, State);
		TArray<ASHPlayerState*> Players;
		TArray<ASHHand*> Hands;
		for (int32 Seat = 0; Seat < 2; ++Seat)
		{
			ASHPlayerState* Player = World->SpawnActor<ASHPlayerState>();
			ASHHand* Hand = World->SpawnActor<ASHHand>();
			AVictoryStack* Stack = World->SpawnActor<AVictoryStack>();
			FindFProperty<FObjectProperty>(Hand->GetClass(), TEXT("VictoryStack"))->SetObjectPropertyValue_InContainer(Hand, Stack);
			Player->SetHand(Hand);
			Player->SetSeatIndex(Seat);
			Hand->SetLayoutSeatIndex(Seat);
			State->AddPlayerState(Player);
			Players.Add(Player);
			Hands.Add(Hand);
		}
		State->SetParticipantHands(Hands);
		auto AddPair = [World](ASHHand* Hand)
		{
			ASHCard* A = World->SpawnActor<ASHCard>();
			ASHCard* B = World->SpawnActor<ASHCard>();
			A->CardDefinition = UCardDefinition::StaticClass();
			B->CardDefinition = UCardDefinition::StaticClass();
			A->SetOwner(Hand);
			B->SetOwner(Hand);
			Hand->AddActivationPairToLogicalHand(A, B);
			Hand->SetActivationPairState(A, B, EActivationPairState::Ready);
			return *Hand->FindActivationPair(A);
		};
		ASHHand* OriginalHand = Hands[SupportSeat];
		ASHHand* TargetHand = Hands[1 - SupportSeat];
		const FActivatedPair Support = AddPair(OriginalHand);
		const FActivatedPair Target = AddPair(OriginalHand);
		const FActivatedPair OtherPair = AddPair(OriginalHand);
		TestTrue(TEXT("Pancho grants a bonus before its cost is paid"),
			Mode->ApplyPanchoBoost(Players[SupportSeat], Support.CardA, Target.CardA));
		OriginalHand->SetActivationPairOutcomePending(Support.CardA, Support.CardB, true);
		TestEqual(TEXT("Pending Pancho has not entered its owner's victory stack"), OriginalHand->GetVictoryStack()->GetPairCount(), 0);
		// Hans transfers a stored pair with its pending bonus intact.
		TestTrue(TEXT("The boosted target can move to another player's zone"),
			Mode->TransferStoredPair(OriginalHand, TargetHand, Target.CardA));
		TestTrue(TEXT("Collection moves pairs which were on the table at its start"),
			Mode->MoveAllActivationPairsToVictoryStacks());

		const FActivatedPair* Restored = OriginalHand->FindActivationPair(Support.CardA);
		TestTrue(TEXT("Refund survives bulk collection regardless of player iteration order"),
			Restored && Restored->CardB == Support.CardB && Restored->State == EActivationPairState::Ready && !Restored->bActivated);
		TestEqual(TEXT("Only the original unrelated pair scores for the support owner"), OriginalHand->GetVictoryStack()->GetPairCount(), 1);
		TestEqual(TEXT("The transferred target scores for its new owner"), TargetHand->GetVictoryStack()->GetPairCount(), 1);
		TestEqual(TEXT("Refunded Pancho remains in the activation zone"), Support.CardA->GetCardZone(), ECardZone::Activation);
		TestEqual(TEXT("The unrelated pair is still collected normally"), OtherPair.CardA->GetCardZone(), ECardZone::Victory);
		TestEqual(TEXT("The boosted target is collected normally"), Target.CardA->GetCardZone(), ECardZone::Victory);
		World->DestroyWorld(false);
	}
	return true;
}
#endif
