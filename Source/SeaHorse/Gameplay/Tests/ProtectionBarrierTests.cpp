#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Engine/World.h"
#include "NiagaraComponent.h"
#include "UObject/UnrealType.h"
#include "Gameplay/Core/SHPlayerState.h"
#include "Gameplay/SHHand.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSHProtectionBarrierTest,
	"SeaHorse.Gameplay.Effects.ProtectionBarrier",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSHProtectionBarrierTest::RunTest(const FString& Parameters)
{
#if WITH_EDITOR
	// Allow reflected callbacks on actors in this lightweight world without starting BP gameplay.
	TGuardValue<bool> ScriptGuard(GAllowActorScriptExecutionInEditor, true);
#endif
	UClass* HandClass = LoadClass<ASHHand>(nullptr,
		TEXT("/Game/SeaHorse/Cards/BP_Hand.BP_Hand_C"));
	if (!TestNotNull(TEXT("Saved hand Blueprint"), HandClass)) { return false; }
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	ASHHand* FirstSeat = World->SpawnActor<ASHHand>(HandClass);
	ASHHand* SecondSeat = World->SpawnActor<ASHHand>(HandClass);
	ASHPlayerState* FirstPlayer = World->SpawnActor<ASHPlayerState>();
	ASHPlayerState* SecondPlayer = World->SpawnActor<ASHPlayerState>();
	auto FindBarrier = [](ASHHand* Hand)
	{
		TInlineComponentArray<UNiagaraComponent*> Components(Hand);
		for (UNiagaraComponent* Component : Components)
		{
			if (Component->GetFName() == TEXT("NS_Barrier")) { return Component; }
		}
		return static_cast<UNiagaraComponent*>(nullptr);
	};
	UNiagaraComponent* FirstBarrier = FindBarrier(FirstSeat);
	UNiagaraComponent* SecondBarrier = FindBarrier(SecondSeat);
	if (!TestNotNull(TEXT("Existing barrier component on first seat"), FirstBarrier) ||
		!TestNotNull(TEXT("Existing barrier component on second seat"), SecondBarrier))
	{
		World->DestroyWorld(false);
		return false;
	}
	TestNotNull(TEXT("Designer-assigned Niagara system"), FirstBarrier->GetAsset());
	const FTransform AuthoredTransform = FirstBarrier->GetRelativeTransform();

	// Clients rotate the visual seats, independently of the logical hand ownership.
	FirstPlayer->SetHand(FirstSeat);
	SecondPlayer->SetHand(SecondSeat);
	FirstSeat->SetRepresentedPlayerState(SecondPlayer);
	SecondSeat->SetRepresentedPlayerState(FirstPlayer);
	TestFalse(TEXT("Initially unprotected seat hides barrier"), FirstBarrier->IsVisible());
	TestFalse(TEXT("Hidden barrier does not keep simulating"), FirstBarrier->IsActive());
	SecondPlayer->SetProtectedFromCardEffects(true);
	TestTrue(TEXT("Host sees protection at the represented player's seat"), FirstBarrier->IsVisible());
	TestFalse(TEXT("Logical hand ownership does not show protection at the wrong seat"), SecondBarrier->IsVisible());
	TestEqual(TEXT("Barrier cannot intercept card interactions"),
		FirstBarrier->GetCollisionEnabled(), ECollisionEnabled::NoCollision);
	TestTrue(TEXT("Designer placement and size are preserved"),
		FirstBarrier->GetRelativeTransform().Equals(AuthoredTransform));
	SecondPlayer->SetProtectedFromCardEffects(false);
	TestFalse(TEXT("Expiring protection hides the barrier immediately"), FirstBarrier->IsVisible());
	TestFalse(TEXT("Expiring protection stops the Niagara system"), FirstBarrier->IsActive());

	// Simulate delivery through the replicated property's notification (not the server setter).
	FBoolProperty* Protection = FindFProperty<FBoolProperty>(ASHPlayerState::StaticClass(),
		TEXT("bProtectedFromCardEffects"));
	if (TestNotNull(TEXT("Replicated protection property"), Protection))
	{
		TestTrue(TEXT("Protection replicates a change notification"),
			Protection->HasAllPropertyFlags(CPF_Net | CPF_RepNotify) &&
			Protection->RepNotifyFunc == TEXT("OnRep_ProtectedFromCardEffects"));
		UFunction* OnRep = SecondPlayer->FindFunction(Protection->RepNotifyFunc);
		if (TestNotNull(TEXT("Protection replication notification"), OnRep))
		{
			Protection->SetPropertyValue_InContainer(SecondPlayer, true);
			SecondPlayer->ProcessEvent(OnRep, nullptr);
			TestTrue(TEXT("Replication notification also displays the barrier"), FirstBarrier->IsVisible());
			Protection->SetPropertyValue_InContainer(SecondPlayer, false);
			SecondPlayer->ProcessEvent(OnRep, nullptr);
			TestFalse(TEXT("Replication notification also removes the barrier"), FirstBarrier->IsVisible());
		}
	}

	FirstPlayer->SetProtectedFromCardEffects(true);
	FirstSeat->SetRepresentedPlayerState(FirstPlayer);
	TestTrue(TEXT("Mapping a player whose protection already arrived shows the barrier"), FirstBarrier->IsVisible());
	FirstSeat->SetRepresentedPlayerState(FirstPlayer);
	SecondPlayer->SetProtectedFromCardEffects(true);
	SecondPlayer->SetProtectedFromCardEffects(false);
	TestTrue(TEXT("Previous player's updates cannot hide the new player's barrier"), FirstBarrier->IsVisible());
	FirstPlayer->SetProtectedFromCardEffects(false);
	TestFalse(TEXT("New player remains subscribed after repeated table setup"), FirstBarrier->IsVisible());

	FirstPlayer->SetProtectedFromCardEffects(true);
	ASHHand* NPCStack = World->SpawnActor<ASHHand>();
	NPCStack->SetIsNPC(true);
	FirstSeat->SetRepresentedHand(NPCStack);
	TestFalse(TEXT("Converting the visual seat to an NPC removes protection"), FirstBarrier->IsVisible());
	FirstPlayer->SetProtectedFromCardEffects(false);
	FirstPlayer->SetProtectedFromCardEffects(true);
	TestFalse(TEXT("Former player's updates cannot restore the NPC's barrier"), FirstBarrier->IsVisible());
	FirstSeat->SetRepresentedPlayerState(FirstPlayer);
	FirstSeat->SetRepresentedPlayerState(nullptr);
	TestFalse(TEXT("Clearing a protected seat hides its barrier"), FirstBarrier->IsVisible());

	World->DestroyWorld(false);
	return true;
}

#endif
