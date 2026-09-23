#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Engine/World.h"
#include "Components/BoxComponent.h"
#include "Components/SplineComponent.h"
#include "Gameplay/Cards/SHCard.h"
#include "Gameplay/Components/CardsLayoutComponent.h"
#include "Gameplay/Core/SHGameState.h"
#include "Gameplay/Core/SHPlayerController.h"
#include "Gameplay/SHHand.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSHHandHoverCorridorTest, "SeaHorse.Gameplay.Input.HandHoverCorridor",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSHHandHoverCorridorTest::RunTest(const FString& Parameters)
{
	TGuardValue<bool> ScriptGuard(GAllowActorScriptExecutionInEditor, true);
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	World->SetGameState(World->SpawnActor<ASHGameState>());
	auto* PC = World->SpawnActor<ASHPlayerController>();
	PC->SetAsLocalPlayerController(); World->AddController(PC);
	auto* Hand = World->SpawnActor<ASHHand>();
	auto* Layout = NewObject<USHHandCardsLayoutComponent>(Hand);
	Hand->AddInstanceComponent(Layout); Layout->RegisterComponent(); Layout->OwningHand = Hand;
	auto* Spline = NewObject<USplineComponent>(Hand);
	Hand->AddInstanceComponent(Spline); Spline->RegisterComponent();
	auto* Forward = NewObject<USceneComponent>(Hand);
	Hand->AddInstanceComponent(Forward); Forward->RegisterComponent();
	Layout->OwnerSpline = Spline; Layout->TableCenterDirectionComponent = Forward;
	Layout->ForwardFocusedOffser = 100.0f; Layout->FocusLiftHeight = 4.0f;
	Layout->FocusCardScale = 1.3f; Layout->FocusSpreadDistance = 0.0f;
	auto MakeBoxCard = [World, Hand](const FVector& Extent)
	{
		auto* Card = World->SpawnActor<ASHCard>();
		Card->SetOwner(Hand); Card->SetCardZone(ECardZone::Hand);
		auto* Surface = NewObject<UBoxComponent>(Card);
		Card->AddInstanceComponent(Surface); Card->SetRootComponent(Surface);
		Surface->SetBoxExtent(Extent); Surface->SetCollisionProfileName(TEXT("BlockAllDynamic"));
		Surface->RegisterComponent();
		return Card;
	};
	ASHCard* Card = MakeBoxCard(FVector(15, 20, 0.1));
	ASHCard* OtherCard = MakeBoxCard(FVector(3, 3, 0.1));
	OtherCard->SetActorEnableCollision(false);
	FVector AlongHand = FVector::ForwardVector, TowardTable = FVector::RightVector;
	double RaySlope = 0.0;
	auto Trace = [&](const FVector& Point)
	{
		FHitResult Hit;
		const FVector RayOffset = FVector::UpVector * 1000.0 - TowardTable * (1000.0 * RaySlope);
		World->LineTraceSingleByChannel(Hit, Point + RayOffset, Point - RayOffset,
			ECC_Visibility, FCollisionQueryParams(NAME_None, true));
		return Hit;
	};
	auto Resolve = [&](const FVector& Point)
	{
		FHitResult Resolved;
		const FVector RayOffset = FVector::UpVector * 1000.0 - TowardTable * (1000.0 * RaySlope);
		PC->ResolveCardCursorHit(Trace(Point), Point + RayOffset, Point - RayOffset, Resolved);
		return Resolved.bBlockingHit ? Cast<ASHCard>(Resolved.GetActor()) : nullptr;
	};
	auto ResetAtRest = [&]()
	{
		PC->ResetHandCursorHover(); Layout->FocusedCardIndexValue = INDEX_NONE;
		Layout->CardsTransforms.Reset(); Layout->CardsTransforms_WithNoOffsets.Reset();
		Layout->UpdateSingleCardPosition(Card, 0, 1);
		const FTransform Rest = Layout->CardsTransforms_WithNoOffsets.FindChecked(Card);
		Card->SetActorTransform(Rest);
		return Rest;
	};
	for (double SeatAngle : {0.0, 90.0, 180.0, -90.0})
	{
		AlongHand = FRotator(0, SeatAngle, 0).RotateVector(FVector::ForwardVector);
		TowardTable = FRotator(0, SeatAngle, 0).RotateVector(FVector::RightVector);
		Spline->SetSplinePoints({AlongHand * -30.0, AlongHand * 30.0}, ESplineCoordinateSpace::World);
		Forward->SetWorldRotation(TowardTable.Rotation());
		for (double Slope : {0.0, 0.6, -0.6})
		{
			RaySlope = Slope;
			const FTransform Rest = ResetAtRest();
			const FVector RestPoint = Rest.GetLocation();
			const FVector GapPoint = RestPoint + TowardTable * 50.0;
			const FString Scenario = FString::Printf(TEXT("seat=%.0f slope=%.1f"), SeatAngle, Slope);
			TestNull(*FString::Printf(TEXT("Empty bridge cannot acquire a card (%s)"), *Scenario), Resolve(GapPoint));
			TestEqual(*FString::Printf(TEXT("Direct resting surface acquires a card (%s)"), *Scenario), Resolve(RestPoint), Card);
			TestNull(*FString::Printf(TEXT("Bridge is outside actual resting collision (%s)"), *Scenario), Cast<ASHCard>(Trace(GapPoint).GetActor()));
			const FTransform BeforeQuery = Card->GetActorTransform();
			// The target is not yet in the layout map. Retention must already cover
			// the final resting-to-focused area before the first animation update.
			TestEqual(*FString::Printf(TEXT("Bridge exists immediately after acquisition (%s)"), *Scenario), Resolve(GapPoint), Card);
			TestTrue(TEXT("Virtual hover query does not move collision or the actor"), BeforeQuery.Equals(Card->GetActorTransform()));
			int32 LostHover = 0;
			for (int32 Frame = 0; Frame < 120; ++Frame)
			{
				ASHCard* Hovered = Resolve(GapPoint);
				if (Hovered != Card) { ++LostHover; }
				Layout->FocusedCardIndexValue = Hovered == Card ? 0 : INDEX_NONE;
				Layout->UpdateSingleCardPosition(Card, 0, 1);
				Layout->MoveCardsToDesiredPositions(1.0f / 60.0f);
			}
			TestEqual(*FString::Printf(TEXT("Stationary pointer in bridge retains hover throughout animation (%s)"), *Scenario), LostHover, 0);
			Card->SetActorTransform(Layout->MakeFocusedCardTransform(Rest));
			TestNull(TEXT("Bridge remains outside actual final collision"), Cast<ASHCard>(Trace(GapPoint).GetActor()));
			TestEqual(TEXT("Final raised card retains hover below its visible surface"), Resolve(GapPoint), Card);
			TestNull(TEXT("Leaving the side of the bridge releases hover"), Resolve(GapPoint + AlongHand * 50.0));
			TestNull(TEXT("Previously hovered card cannot be reacquired from empty bridge"), Resolve(GapPoint));
			TestEqual(TEXT("Direct raised surface can be acquired again"), Resolve(Card->GetActorLocation()), Card);
			TestNull(TEXT("Moving below the resting edge releases hover"), Resolve(RestPoint - TowardTable * 35.0));
			ResetAtRest(); Resolve(RestPoint);
			Card->SetActorTransform(Layout->MakeFocusedCardTransform(Rest));
			// A real card under the cursor takes precedence in the extra retention
			// area, even though the previous card remains inside its virtual bridge.
			OtherCard->SetActorLocation(GapPoint + FVector::UpVector * 10.0);
			OtherCard->SetActorEnableCollision(true);
			const FVector OtherPoint = OtherCard->GetActorLocation();
			TestEqual(TEXT("Another visible card actually occupies the bridge"), Cast<ASHCard>(Trace(OtherPoint).GetActor()), OtherCard);
			TestEqual(TEXT("Another visible card wins over the virtual bridge"), Resolve(OtherPoint), OtherCard);
			OtherCard->SetActorEnableCollision(false);
		}
	}
	// Use the saved card's real mesh and the current large forward offset with
	// no vertical lift. This covers the enlarged lower corner reported in play.
	Card->SetActorEnableCollision(false);
	UClass* CardClass = LoadClass<ASHCard>(nullptr, TEXT("/Game/SeaHorse/Cards/BP_Card.BP_Card_C"));
	if (TestNotNull(TEXT("Actual card Blueprint loads for the lower-corner case"), CardClass))
	{
		Card = World->SpawnActor<ASHCard>(CardClass);
		Card->SetOwner(Hand); Card->SetCardZone(ECardZone::Hand);
		Spline->SetSplinePoints({FVector(-30, 0, 0), FVector(30, 0, 0)}, ESplineCoordinateSpace::World);
		Forward->SetWorldRotation(FRotator(0, 90, 0));
		TowardTable = FVector::RightVector; RaySlope = 0.0;
		Layout->ForwardFocusedOffser = 30.0f; Layout->FocusLiftHeight = 0.0f;
		const FTransform Rest = ResetAtRest();
		const FBox RestBounds = Card->GetComponentsBoundingBox(true);
		const FTransform Raised = Layout->MakeFocusedCardTransform(Rest);
		Card->SetActorTransform(Raised);
		const FBox RaisedBounds = Card->GetComponentsBoundingBox(true);
		const FVector BelowExpandedCorner(
			(RestBounds.Max.X + RaisedBounds.Max.X) * 0.5,
			(RestBounds.Min.Y + RaisedBounds.Min.Y) * 0.5,
			Rest.GetLocation().Z);
		TestNull(TEXT("Lower corner lies below the actual raised mesh"), Cast<ASHCard>(Trace(BelowExpandedCorner).GetActor()));
		Card->SetActorTransform(Rest);
		TestNull(TEXT("Lower corner lies beside the actual resting mesh"), Cast<ASHCard>(Trace(BelowExpandedCorner).GetActor()));
		TestNull(TEXT("Expanded lower corner cannot start hover on an unhovered card"), Resolve(BelowExpandedCorner));
		TestEqual(TEXT("Actual card acquires hover from its resting face"), Resolve(Rest.GetLocation()), Card);
		int32 LostCornerHover = 0;
		for (int32 Frame = 0; Frame < 120; ++Frame)
		{
			if (Resolve(BelowExpandedCorner) != Card) { ++LostCornerHover; }
			Layout->FocusedCardIndexValue = 0;
			Layout->UpdateSingleCardPosition(Card, 0, 1);
			Layout->MoveCardsToDesiredPositions(1.0f / 60.0f);
		}
		TestEqual(TEXT("Expanded lower corner remains part of hover area throughout lifting"), LostCornerHover, 0);
	}
	World->DestroyWorld(false);
	return true;
}
#endif
