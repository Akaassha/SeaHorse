#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Engine/World.h"
#include "Engine/Level.h"
#include "Camera/CameraActor.h"
#include "Components/PrimitiveComponent.h"
#include "Components/SplineComponent.h"
#include "Gameplay/Core/SHGameState.h"
#include "Gameplay/Core/SHPlayerController.h"
#include "Gameplay/Cards/SHCard.h"
#include "Gameplay/Components/CardsLayoutComponent.h"
#include "Gameplay/SHHand.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSHHandHoverMapTest, "SeaHorse.Gameplay.Input.HandHoverMap",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSHHandHoverMapTest::RunTest(const FString& Parameters)
{
	TGuardValue<bool> ScriptGuard(GAllowActorScriptExecutionInEditor, true);
	// Clone authored scene actors into an isolated world: camera, floor, player
	// representations, splines, and layout instance overrides remain real data.
	// The editor's current world and the saved map are never modified.
	UWorld* SourceWorld = LoadObject<UWorld>(nullptr, TEXT("/Game/SeaHorse/Maps/L_Test.L_Test"));
	if (!TestNotNull(TEXT("Authored test map"), SourceWorld)) { return false; }
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	World->SetGameState(World->SpawnActor<ASHGameState>());
	ASHPlayerController* PC = World->SpawnActor<ASHPlayerController>();
	PC->SetAsLocalPlayerController(); World->AddController(PC);
	TArray<ASHHand*> Hands;
	FVector CameraLocation = FVector::ZeroVector;
	bool bHasCamera = false;
	for (AActor* Source : SourceWorld->PersistentLevel->Actors)
	{
		if (!IsValid(Source)) { continue; }
		if (ACameraActor* Camera = Cast<ACameraActor>(Source))
		{
			CameraLocation = Camera->GetActorLocation(); bHasCamera = true;
		}
		// Only collision-bearing actors and hand layouts are needed. Omitting
		// lights and the LevelScriptActor avoids running unrelated map behavior.
		bool bHasCollision = false;
		TInlineComponentArray<UPrimitiveComponent*> Components(Source);
		for (UPrimitiveComponent* Component : Components)
		{
			bHasCollision |= Component->IsQueryCollisionEnabled() &&
				Component->GetCollisionResponseToChannel(ECC_Visibility) == ECR_Block;
		}
		if (!bHasCollision && !Source->IsA<ASHHand>()) { continue; }
		FActorSpawnParameters Spawn;
		Spawn.Template = Source;
		Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		AActor* Copy = World->SpawnActor<AActor>(Source->GetClass(), Source->GetActorTransform(), Spawn);
		if (ASHHand* Hand = Cast<ASHHand>(Copy)) { Hands.Add(Hand); }
	}
	UClass* CardClass = LoadClass<ASHCard>(nullptr, TEXT("/Game/SeaHorse/Cards/BP_Card.BP_Card_C"));
	if (!TestTrue(TEXT("Map provides its camera and hand slots"), bHasCamera && Hands.Num() == 4) ||
		!TestNotNull(TEXT("Actual card Blueprint"), CardClass))
	{
		World->DestroyWorld(false); return false;
	}
	TArray<ASHCard*> Cards;
	for (int32 Index = 0; Index < 5; ++Index)
	{
		ASHCard* Card = World->SpawnActor<ASHCard>(CardClass);
		Card->SetCardZone(ECardZone::Hand); Cards.Add(Card);
	}
	int32 AcquiredPaths = 0;
	int32 DroppedPaths = 0;
	FString FirstFailure;
	for (bool bRotateLogicalOwners : {false, true})
	{
		// Multiplayer rotates which logical hand each physical table slot presents.
		// Hover must find the visual layout rather than the owner's original slot.
		for (int32 Index = 0; Index < Hands.Num(); ++Index)
		{
			Hands[Index]->SetRepresentedHand(bRotateLogicalOwners ? Hands[(Index + 1) % Hands.Num()] : nullptr);
		}
		const int32 AcquiredBeforeMapping = AcquiredPaths;
		for (ASHHand* Hand : Hands)
		{
			USHHandCardsLayoutComponent* Layout = Hand->FindComponentByClass<USHHandCardsLayoutComponent>();
			USplineComponent* Spline = nullptr;
			USceneComponent* Forward = nullptr;
			TInlineComponentArray<USceneComponent*> Components(Hand);
			for (USceneComponent* Component : Components)
			{
				if (Component->GetName() == TEXT("HandCards_Spline")) { Spline = Cast<USplineComponent>(Component); }
				if (Component->GetName() == TEXT("TableCenterDirection")) { Forward = Component; }
			}
			if (!TestNotNull(TEXT("Authored hand layout"), Layout) ||
				!TestNotNull(TEXT("Authored hand spline"), Spline) ||
				!TestNotNull(TEXT("Authored hand direction"), Forward)) { continue; }
			Layout->OwningHand = Hand; Layout->OwnerSpline = Spline;
			Layout->TableCenterDirectionComponent = Forward;
			for (ASHCard* Card : Cards) { Card->SetOwner(Hand->GetRepresentedHand()); }
			for (int32 CardCount : {1, 5})
			{
				for (int32 Index = 0; Index < Cards.Num(); ++Index)
				{
					Cards[Index]->SetActorHiddenInGame(Index >= CardCount);
					Cards[Index]->SetActorEnableCollision(Index < CardCount);
				}
				auto ResetLayout = [&]()
				{
					PC->ResetHandCursorHover(); Layout->FocusedCardIndexValue = INDEX_NONE;
					Layout->CardsTransforms.Reset(); Layout->CardsTransforms_WithNoOffsets.Reset();
					for (int32 Index = 0; Index < CardCount; ++Index)
					{
						Layout->UpdateSingleCardPosition(Cards[Index], Index, CardCount);
						Cards[Index]->SetActorTransform(Layout->CardsTransforms_WithNoOffsets.FindChecked(Cards[Index]));
					}
				};
				ResetLayout();
				ASHCard* Target = Cards[CardCount / 2];
				const FTransform Rest = Target->GetActorTransform();
				FBox LocalBounds(ForceInit);
				TInlineComponentArray<UPrimitiveComponent*> CardComponents(Target);
				for (UPrimitiveComponent* Component : CardComponents)
				{
					if (!Component->IsQueryCollisionEnabled() || Component->GetCollisionResponseToChannel(ECC_Visibility) != ECR_Block) { continue; }
					LocalBounds += Component->CalcBounds(Component->GetComponentTransform().GetRelativeTransform(Rest)).GetBox();
				}
				for (int32 XI = 0; XI < 9; ++XI)
				for (int32 YI = 0; YI < 17; ++YI)
				{
					ResetLayout();
					const FVector LocalPoint(
						FMath::Lerp(LocalBounds.Min.X, LocalBounds.Max.X, (XI + 0.5) / 9.0),
						FMath::Lerp(LocalBounds.Min.Y, LocalBounds.Max.Y, (YI + 0.5) / 17.0), LocalBounds.Max.Z);
					const FVector Point = Rest.TransformPosition(LocalPoint);
					const FVector Start = CameraLocation;
					const FVector End = Start + (Point - Start).GetSafeNormal() * 10000;
					FHitResult Hit, Resolved;
					World->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility, FCollisionQueryParams(NAME_None, true));
					if (Hit.GetActor() != Target) { continue; }
					++AcquiredPaths;
					bool bDropped = false;
					for (int32 Frame = 0; Frame < 120 && !bDropped; ++Frame)
					for (int32 Query = 0; Query < 3 && !bDropped; ++Query)
					{
						World->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility, FCollisionQueryParams(NAME_None, true));
						PC->ResolveCardCursorHit(Hit, Start, End, Resolved);
						ASHCard* Hovered = Resolved.bBlockingHit ? Cast<ASHCard>(Resolved.GetActor()) : nullptr;
						if (Hovered != Target)
						{
							bDropped = true; ++DroppedPaths;
							if (FirstFailure.IsEmpty())
							{
								FirstFailure = FString::Printf(TEXT("rotated=%d hand=%s cards=%d sample=(%d,%d) frame=%d query=%d raw=%s/%s resolved=%s current=%s rest=%s focused=%s"),
									bRotateLogicalOwners, *Hand->GetName(), CardCount, XI, YI, Frame, Query, *GetNameSafe(Hit.GetActor()),
									*GetNameSafe(Hit.GetComponent()), *GetNameSafe(Hovered), *Target->GetActorLocation().ToString(),
									*PC->HandCursorHoverBaseTransform.GetLocation().ToString(),
									*PC->HandCursorHoverFocusedTransform.GetLocation().ToString());
							}
						}
						Layout->FocusedCardIndexValue = Cards.IndexOfByKey(Hovered);
						for (int32 Index = 0; Index < CardCount; ++Index) { Layout->UpdateSingleCardPosition(Cards[Index], Index, CardCount); }
						Layout->MoveCardsToDesiredPositions(1.0f / 180.0f);
					}
				}
			}
		}
		TestTrue(*FString::Printf(TEXT("Map camera sees card faces with ownership rotation=%d"), bRotateLogicalOwners), AcquiredPaths - AcquiredBeforeMapping > 100);
	}
	TestTrue(TEXT("Real map camera sees sampled card faces"), AcquiredPaths > 100);
	TestEqual(*FString::Printf(TEXT("Authored map hover keeps the same card after its visible face leaves the stationary cursor (%s)"), *FirstFailure), DroppedPaths, 0);
	AddInfo(FString::Printf(TEXT("Real map stationary cursor paths exercised: %d"), AcquiredPaths));
	World->DestroyWorld(false);
	return true;
}
#endif
