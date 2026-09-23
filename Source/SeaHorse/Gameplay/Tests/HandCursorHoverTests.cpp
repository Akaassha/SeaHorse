#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Engine/World.h"
#include "Components/BoxComponent.h"
#include "Components/SplineComponent.h"
#include "Gameplay/Cards/SHCard.h"
#include "Gameplay/Core/SHPlayerController.h"
#include "Gameplay/Components/CardsLayoutComponent.h"
#include "Gameplay/SHHand.h"
#include "UObject/UnrealType.h"
#include "UObject/StructOnScope.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSHHandCursorHoverTest, "SeaHorse.Gameplay.Input.HandCursorHover",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSHHandCursorHoverTest::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	auto* PC = World->SpawnActor<ASHPlayerController>();
	PC->SetAsLocalPlayerController();
	World->AddController(PC);
	auto* Hand = World->SpawnActor<ASHHand>();
	auto* OtherHand = World->SpawnActor<ASHHand>();
	auto MakeCard = [World, Hand](const FVector& Location)
	{
		auto* Card = World->SpawnActor<ASHCard>();
		Card->SetOwner(Hand); Card->SetCardZone(ECardZone::Hand);
		auto* Surface = NewObject<UBoxComponent>(Card);
		Card->AddInstanceComponent(Surface); Card->SetRootComponent(Surface);
		Surface->SetBoxExtent(FVector(15, 20, 0.1));
		Surface->SetCollisionProfileName(TEXT("BlockAllDynamic")); Surface->RegisterComponent();
		Card->SetActorLocation(Location);
		return Card;
	};
	ASHCard* Bottom = MakeCard(FVector(0, 0, 0));
	ASHCard* Top = MakeCard(FVector(10, 0, 3));
	auto Trace = [World](double X, double Y)
	{
		FHitResult Hit;
		World->LineTraceSingleByChannel(Hit, FVector(X, Y, 100), FVector(X, Y, -100),
			ECC_Visibility, FCollisionQueryParams(NAME_None, true));
		return Hit;
	};
	auto Resolve = [PC, &Trace](double X, double Y)
	{
		FHitResult Resolved;
		PC->ResolveCardCursorHit(Trace(X, Y), FVector(X, Y, 100), FVector(X, Y, -100), Resolved);
		return Resolved.bBlockingHit ? Cast<ASHCard>(Resolved.GetActor()) : nullptr;
	};
	TestEqual(TEXT("Overlap ray hits the visible top card despite the bottom center being closer"), Trace(0, 0).GetActor(), static_cast<AActor*>(Top));
	TestEqual(TEXT("Hover uses that actual surface hit"), Resolve(0, 0), Top);
	Top->SetActorLocation(FVector(10, 50, 7));
	for (int32 Step = 0; Step < 20; ++Step)
	{
		TestEqual(TEXT("Movement under lifted card stays inside its resting footprint without jitter"), Resolve(Step * 0.1, Step * 0.2), Top);
	}
	TestEqual(TEXT("Visible lifted surface remains interactive"), Resolve(10, 50), Top);
	TestEqual(TEXT("Returning from raised surface to resting footprint retains the same card"), Resolve(2, 3), Top);
	TestEqual(TEXT("Moving outside the original footprint onto lower card switches focus"), Resolve(-10, 0), Bottom);
	TestNull(TEXT("Empty space next to a card does not choose the nearest card"), Resolve(-16, 0));
	Resolve(1, 0);
	PC->bCardHoverSuppressedForTargeting = true;
	Resolve(1, 0);
	TestNull(TEXT("Effect targeting clears cached hover"), PC->HandCursorHoverCard.Get());
	PC->bCardHoverSuppressedForTargeting = false;
	PC->LocallyDraggedCard = Bottom;
	Resolve(1, 0);
	TestNull(TEXT("Dragging disables ordinary hover"), PC->HandCursorHoverCard.Get());
	PC->LocallyDraggedCard = nullptr;
	Resolve(1, 0);
	Bottom->SetCardZone(ECardZone::Activation);
	Resolve(1, 0);
	TestNull(TEXT("A card entering the activation zone loses hand hover"), PC->HandCursorHoverCard.Get());
	Bottom->SetCardZone(ECardZone::Hand); Bottom->SetOwner(OtherHand); OtherHand->SetIsNPC(true);
	Resolve(1, 0);
	TestNull(TEXT("BN stack does not lift as a human hand"), PC->HandCursorHoverCard.Get());
	OtherHand->SetIsNPC(false);
	TestEqual(TEXT("Another human hand can hover without reading its hidden definition"), Resolve(1, 0), Bottom);
	Bottom->SetActorHiddenInGame(true);
	Resolve(1, 0);
	TestNull(TEXT("Hidden card cannot retain hover"), PC->HandCursorHoverCard.Get());
	Bottom->SetActorHiddenInGame(false);
	PC->ResetHandCursorHover();
	Top->SetActorLocation(FVector(10, 0, 3));
	Resolve(0, 0);
	Top->SetActorLocation(FVector(10, 50, 7));
	auto* Occluder = MakeCard(FVector(0, 0, 10));
	Occluder->SetCardZone(ECardZone::Activation);
	TestEqual(TEXT("Closer blocking activation card wins over resting footprint"), Resolve(0, 0), Occluder);
	TestNull(TEXT("Blocking activation card clears hand hover"), PC->HandCursorHoverCard.Get());

	// Existing BP_Hand calls this setter with its legacy nearest-center result.
	// The native layout must override it even when no pointer/viewport exists.
	auto* Layout = NewObject<USHHandCardsLayoutComponent>(Hand);
	Hand->AddInstanceComponent(Layout); Layout->RegisterComponent();
	auto* Spline = NewObject<USplineComponent>(Hand);
	Hand->AddInstanceComponent(Spline); Spline->RegisterComponent();
	Layout->Initialize({Top}, Spline, nullptr);
	Layout->SetFocusedCardIndex(0);
	Layout->UpdateCardsPositions({Top});
	TestEqual(TEXT("Legacy nearest-card index cannot produce hover without a cursor hit"), Layout->FocusedCardIndexValue, INDEX_NONE);
#if WITH_EDITOR
	TGuardValue<bool> ScriptGuard(GAllowActorScriptExecutionInEditor, true);
	UClass* HandClass = LoadClass<ASHHand>(nullptr, TEXT("/Game/SeaHorse/Cards/BP_Hand.BP_Hand_C"));
	if (TestNotNull(TEXT("Saved hand Blueprint loads"), HandClass))
	{
		auto* BlueprintHand = World->SpawnActor<ASHHand>(HandClass);
		BlueprintHand->AddCard(Bottom, 0); BlueprintHand->AddCard(Top, 1);
		auto* Focus = FindFProperty<FIntProperty>(HandClass, TEXT("FocusedCardIndex"));
		auto* Selected = FindFProperty<FIntProperty>(HandClass, TEXT("SelectedCardIndex"));
		UFunction* Click = BlueprintHand->FindFunction(TEXT("ClickedHandCard"));
		if (TestNotNull(TEXT("BP focus field"), Focus) && TestNotNull(TEXT("BP selected field"), Selected) && TestNotNull(TEXT("BP click function"), Click))
		{
			Focus->SetPropertyValue_InContainer(BlueprintHand, 0);
			Selected->SetPropertyValue_InContainer(BlueprintHand, INDEX_NONE);
			PC->PointerPressedCard = Top;
			PC->HandCursorHoverCard = Bottom; // Mouse already moved after the press.
			TestEqual(TEXT("Press index ignores a stale nearest-center focus"), BlueprintHand->GetPointerPressedHandCardIndex(), 1);
			FStructOnScope Params(Click);
			BlueprintHand->ProcessEvent(Click, Params.GetStructMemory());
			TestEqual(TEXT("Actual saved Blueprint selects the pressed card, not the nearest index"), Selected->GetPropertyValue_InContainer(BlueprintHand), 1);
			PC->PointerPressedCard = nullptr;
			TestEqual(TEXT("Empty press has no candidate"), BlueprintHand->GetPointerPressedHandCardIndex(), INDEX_NONE);
			PC->PointerPressedCard = Top;
			Top->SetOwner(OtherHand);
			TestEqual(TEXT("A transferred pressed card cannot select the old hand's index"), BlueprintHand->GetPointerPressedHandCardIndex(), INDEX_NONE);
		}
	}
#endif
	World->DestroyWorld(false);
	return true;
}
#endif
