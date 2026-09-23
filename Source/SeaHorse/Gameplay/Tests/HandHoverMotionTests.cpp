#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Engine/World.h"
#include "Components/SplineComponent.h"
#include "Components/BoxComponent.h"
#include "Gameplay/Core/SHGameState.h"
#include "Gameplay/Core/SHPlayerController.h"
#include "Gameplay/Cards/SHCard.h"
#include "Gameplay/Components/CardsLayoutComponent.h"
#include "Gameplay/SHHand.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSHHandHoverMotionTest, "SeaHorse.Gameplay.Input.HandHoverMotion",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSHHandHoverMotionTest::RunTest(const FString& Parameters)
{
	TGuardValue<bool> ScriptGuard(GAllowActorScriptExecutionInEditor, true);
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	World->SetGameState(World->SpawnActor<ASHGameState>());
	auto* PC = World->SpawnActor<ASHPlayerController>(); PC->SetAsLocalPlayerController(); World->AddController(PC);
	auto* Hand = World->SpawnActor<ASHHand>();
	auto* Layout = NewObject<USHHandCardsLayoutComponent>(Hand);
	Hand->AddInstanceComponent(Layout); Layout->RegisterComponent(); Layout->OwningHand = Hand;
	auto* Spline = NewObject<USplineComponent>(Hand); Hand->AddInstanceComponent(Spline); Spline->RegisterComponent();
	Spline->SetSplinePoints({FVector(-30, 0, 0), FVector(30, 0, 0)}, ESplineCoordinateSpace::World);
	auto* Forward = NewObject<USceneComponent>(Hand); Hand->AddInstanceComponent(Forward); Forward->RegisterComponent();
	Forward->SetWorldRotation(FRotator(0, 90, 0));
	Layout->OwnerSpline = Spline; Layout->TableCenterDirectionComponent = Forward;
	Layout->PreferredCardSpacing = 6; Layout->ForwardFocusedOffser = 30; Layout->FocusLiftHeight = 4;
	Layout->FocusCardScale = 1.3;
	UClass* CardClass = LoadClass<ASHCard>(nullptr, TEXT("/Game/SeaHorse/Cards/BP_Card.BP_Card_C"));
	if (!TestNotNull(TEXT("Actual card Blueprint"), CardClass)) { World->DestroyWorld(false); return false; }
	TArray<ASHCard*> Cards;
	for (int32 I = 0; I < 3; ++I)
	{
		auto* Card = World->SpawnActor<ASHCard>(CardClass); Card->SetOwner(Hand); Card->SetCardZone(ECardZone::Hand); Cards.Add(Card);
	}
	auto* Table = World->SpawnActor<AActor>();
	auto* Surface = NewObject<UBoxComponent>(Table); Table->AddInstanceComponent(Surface); Table->SetRootComponent(Surface);
	Surface->SetBoxExtent(FVector(300, 300, 0.1)); Surface->SetCollisionProfileName(TEXT("BlockAllDynamic")); Surface->RegisterComponent();
	Table->SetActorLocation(FVector(0, 0, -1));
	int32 Acquired = 0;
	int32 Unstable = 0;
	int32 MaximumFailedTransitions = 0;
	FString FirstFailure;
	auto Sweep = [&](int32 CardCount, double Spread, double Lift, int32 QueriesPerFrame,
		bool bMoveBelowLifted, const TArray<double>& CursorXs)
	{
		Layout->FocusSpreadDistance = Spread;
		Layout->FocusLiftHeight = Lift;
		for (int32 I = 0; I < Cards.Num(); ++I)
		{
			Cards[I]->SetActorHiddenInGame(I >= CardCount);
			Cards[I]->SetActorEnableCollision(I < CardCount);
		}
		for (double Angle : {0.0, 0.6, -0.6})
		for (double X : CursorXs)
		for (double Y = -40; Y <= 55; Y += 2.5)
		{
			PC->ResetHandCursorHover(); Layout->FocusedCardIndexValue = INDEX_NONE;
			Layout->CardsTransforms.Reset(); Layout->CardsTransforms_WithNoOffsets.Reset();
			for (int32 I = 0; I < CardCount; ++I)
			{
				Layout->UpdateSingleCardPosition(Cards[I], I, CardCount);
				Cards[I]->SetActorTransform(Layout->CardsTransforms_WithNoOffsets.FindChecked(Cards[I]));
			}
			int32 Transitions = 0;
			FString FirstTransition;
			ASHCard* Last = nullptr;
			for (int32 Frame = 0; Frame < 160; ++Frame)
			{
				const double CursorX = bMoveBelowLifted && Frame < 40 ? 0 : X;
				const double CursorY = bMoveBelowLifted && Frame < 40 ? 0 : Y;
				const FVector Start(CursorX, CursorY - 1000 * Angle, 1000), End(CursorX, CursorY + 1000 * Angle, -1000);
				for (int32 Query = 0; Query < QueriesPerFrame; ++Query)
				{
					FHitResult Hit, Resolved;
					World->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility, FCollisionQueryParams(NAME_None, true));
					PC->ResolveCardCursorHit(Hit, Start, End, Resolved);
					ASHCard* Hovered = Resolved.bBlockingHit ? Cast<ASHCard>(Resolved.GetActor()) : nullptr;
					if (Frame == 0 && Query == 0) { if (Hovered) { ++Acquired; } }
					else if (Hovered != Last && (!bMoveBelowLifted || Frame != 40))
					{
						++Transitions;
						if (FirstTransition.IsEmpty())
						{
							FirstTransition = FString::Printf(TEXT("frame=%d query=%d card%d->card%d raw=card%d"),
								Frame, Query, Cards.IndexOfByKey(Last), Cards.IndexOfByKey(Hovered), Cards.IndexOfByKey(Cast<ASHCard>(Hit.GetActor())));
						}
					}
					Last = Hovered;
					Layout->FocusedCardIndexValue = Cards.IndexOfByKey(Hovered);
					for (int32 I = 0; I < CardCount; ++I) { Layout->UpdateSingleCardPosition(Cards[I], I, CardCount); }
					// Blueprint queries and layout movement can interleave within a frame.
					Layout->MoveCardsToDesiredPositions(1.0f / (60.0f * QueriesPerFrame));
				}
			}
			// Include initially empty positions: a returning card must not repeatedly
			// enter the pointer, lift away, and reacquire hover on its way back down.
			// After a pointer move, a card may enter or leave the stationary ray once
			// as the layout settles (including neighbors returning from their spread).
			// Repeated loss/reacquisition is the hover feedback loop being prevented.
			const int32 AllowedTransitions = bMoveBelowLifted ? 1 : 0;
			if (Transitions > AllowedTransitions)
			{
				++Unstable;
				MaximumFailedTransitions = FMath::Max(MaximumFailedTransitions, Transitions);
				if (FirstFailure.IsEmpty())
				{
					FirstFailure = FString::Printf(TEXT("cards=%d spread=%.1f lift=%.1f queries=%d moved=%d X=%.1f Y=%.1f angle=%.1f transitions=%d; %s"),
						CardCount, Spread, Lift, QueriesPerFrame, bMoveBelowLifted, X, Y, Angle, Transitions, *FirstTransition);
				}
			}
		}
	};
	for (bool bMoveBelowLifted : {false, true})
	{
		Sweep(3, 0.0, 4.0, 1, bMoveBelowLifted, {-12.0, -6.0, 0.0, 6.0, 12.0});
		Sweep(3, 2.0, 4.0, 3, bMoveBelowLifted, {-12.0, -6.0, 0.0, 6.0, 12.0});
	}
	// One isolated card reproduces the reported symptom without a neighboring
	// card stealing focus. Include the enlarged lower corners, not only its center.
	Layout->FocusedCardIndexValue = INDEX_NONE; Layout->FocusSpreadDistance = 0.0;
	Layout->UpdateSingleCardPosition(Cards[0], 0, 1);
	Cards[0]->SetActorTransform(Layout->CardsTransforms.FindChecked(Cards[0]));
	const double RestingHalfWidth = Cards[0]->GetComponentsBoundingBox(true).GetExtent().X;
	TArray<double> SingleCardCursorXs;
	for (double WidthFraction : {-1.28, -1.2, -1.1, -1.025, -0.98, 0.0, 0.98, 1.025, 1.1, 1.2, 1.28})
	{
		SingleCardCursorXs.Add(RestingHalfWidth * WidthFraction);
	}
	for (double Lift : {0.0, 4.0})
	{
		Sweep(1, 0.0, Lift, 3, true, SingleCardCursorXs);
	}
	TestTrue(TEXT("Sweep acquires real visible card surfaces"), Acquired > 0);
	TestEqual(*FString::Printf(TEXT("Full lift animation must retain stationary hover (maximum transitions=%d; %s)"), MaximumFailedTransitions, *FirstFailure), Unstable, 0);
	// A card can already be returning when the pointer enters its swept lower
	// corner. Derive this region from the actual mesh instead of assuming its size:
	// it is visible halfway through the animation, but at neither endpoint.
	int32 ReturningPathsCovered = 0;
	int32 ReturningPathsUnstable = 0;
	FString FirstReturningFailure;
	Layout->FocusSpreadDistance = 0.0;
	for (int32 I = 0; I < Cards.Num(); ++I)
	{
		Cards[I]->SetActorHiddenInGame(I != 0); Cards[I]->SetActorEnableCollision(I == 0);
	}
	for (double Lift : {0.0, 4.0})
	{
		Layout->FocusLiftHeight = Lift;
		Layout->FocusedCardIndexValue = INDEX_NONE;
		Layout->CardsTransforms.Reset(); Layout->CardsTransforms_WithNoOffsets.Reset();
		Layout->UpdateSingleCardPosition(Cards[0], 0, 1);
		const FTransform Rest = Layout->CardsTransforms.FindChecked(Cards[0]);
		Layout->FocusedCardIndexValue = 0; Layout->UpdateSingleCardPosition(Cards[0], 0, 1);
		const FTransform Raised = Layout->CardsTransforms.FindChecked(Cards[0]);
		FTransform Returning; Returning.Blend(Rest, Raised, 0.5f);
		Cards[0]->SetActorTransform(Returning);
		const FBox SweptBounds = Cards[0]->GetComponentsBoundingBox(true);
		bool bFoundPath = false;
		for (int32 XI = 0; XI <= 40 && !bFoundPath; ++XI)
		for (int32 YI = 0; YI <= 60 && !bFoundPath; ++YI)
		{
			const double X = FMath::Lerp(SweptBounds.Min.X, SweptBounds.Max.X, XI / 40.0);
			const double Y = FMath::Lerp(SweptBounds.Min.Y, SweptBounds.Max.Y, YI / 60.0);
			const FVector Start(X, Y, 1000), End(X, Y, -1000);
			auto HitsAt = [&](const FTransform& Pose)
			{
				Cards[0]->SetActorTransform(Pose);
				FHitResult Hit;
				World->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility, FCollisionQueryParams(NAME_None, true));
				return Hit.GetActor() == Cards[0];
			};
			if (!HitsAt(Returning) || HitsAt(Rest) || HitsAt(Raised)) { continue; }
			bFoundPath = true; ++ReturningPathsCovered;
			PC->ResetHandCursorHover(); Layout->FocusedCardIndexValue = INDEX_NONE;
			Layout->UpdateSingleCardPosition(Cards[0], 0, 1); Cards[0]->SetActorTransform(Returning);
			ASHCard* Last = nullptr;
			int32 Transitions = 0;
			for (int32 Frame = 0; Frame < 180; ++Frame)
			for (int32 Query = 0; Query < 3; ++Query)
			{
				FHitResult Hit, Resolved;
				World->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility, FCollisionQueryParams(NAME_None, true));
				PC->ResolveCardCursorHit(Hit, Start, End, Resolved);
				ASHCard* Hovered = Resolved.bBlockingHit ? Cast<ASHCard>(Resolved.GetActor()) : nullptr;
				if ((Frame > 0 || Query > 0) && Hovered != Last) { ++Transitions; }
				Last = Hovered;
				Layout->FocusedCardIndexValue = Hovered == Cards[0] ? 0 : INDEX_NONE;
				Layout->UpdateSingleCardPosition(Cards[0], 0, 1);
				Layout->MoveCardsToDesiredPositions(1.0f / 180.0f);
			}
			if (Transitions > 0)
			{
				++ReturningPathsUnstable;
				if (FirstReturningFailure.IsEmpty())
				{
					FirstReturningFailure = FString::Printf(TEXT("lift=%.1f X=%.3f Y=%.3f transitions=%d"), Lift, X, Y, Transitions);
				}
			}
		}
	}
	TestEqual(TEXT("Actual card mesh exposes a swept lower corner at both lift settings"), ReturningPathsCovered, 2);
	TestEqual(*FString::Printf(TEXT("An isolated returning card must not repeatedly reacquire hover (%s)"), *FirstReturningFailure), ReturningPathsUnstable, 0);
	// Intentionally elevated selection is different from a returning hover animation.
	Layout->FocusSpreadDistance = 0.0; Layout->FocusLiftHeight = 4.0;
	PC->ResetHandCursorHover(); Layout->FocusedCardIndexValue = INDEX_NONE;
	for (int32 I = 0; I < Cards.Num(); ++I)
	{
		Cards[I]->SetActorHiddenInGame(false); Cards[I]->SetActorEnableCollision(true);
		Layout->UpdateSingleCardPosition(Cards[I], I, Cards.Num());
		Cards[I]->SetActorTransform(Layout->CardsTransforms_WithNoOffsets.FindChecked(Cards[I]));
	}
	const FVector Start(-12, 2.5, 1000), End(-12, 2.5, -1000);
	FHitResult Hit, Resolved;
	World->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility, FCollisionQueryParams(NAME_None, true));
	PC->ResolveCardCursorHit(Hit, Start, End, Resolved);
	TestTrue(TEXT("Selected-card scenario returns a blocking hit"), Resolved.bBlockingHit);
	TestEqual(TEXT("Selected-card scenario starts on the middle card"), Resolved.GetActor(), static_cast<AActor*>(Cards[1]));
	Layout->SelectedCardIndexValue = 2;
	Layout->UpdateSingleCardPosition(Cards[2], 2, Cards.Num());
	Cards[2]->SetActorLocation(FVector(6, 24.432, 3.658)); Cards[2]->SetActorScale3D(FVector(1.245));
	World->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility, FCollisionQueryParams(NAME_None, true));
	TestEqual(TEXT("Elevated selected card is visibly under the pointer"), Hit.GetActor(), static_cast<AActor*>(Cards[2]));
	PC->ResolveCardCursorHit(Hit, Start, End, Resolved);
	TestTrue(TEXT("Intentional selection remains a blocking hit"), Resolved.bBlockingHit);
	TestEqual(TEXT("Intentional selection keeps its actual clickable surface"), Resolved.GetActor(), static_cast<AActor*>(Cards[2]));
	World->DestroyWorld(false);
	return true;
}
#endif
