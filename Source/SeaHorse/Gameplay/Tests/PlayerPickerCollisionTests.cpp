#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Engine/World.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/WidgetComponent.h"
#include "Gameplay/Cards/SHCard.h"
#include "Gameplay/Player/SHPlayerRepresentation.h"
#include "Gameplay/SHHand.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSHPlayerPickerCollisionTest,
	"SeaHorse.Gameplay.Input.PlayerPickerCollision",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSHPlayerPickerCollisionTest::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	UClass* PickerClass = LoadClass<ASHPlayerRepresentation>(nullptr,
		TEXT("/Game/SeaHorse/Board/BP_PlayerRepresentation.BP_PlayerRepresentation_C"));
	if (!TestNotNull(TEXT("Saved player representation Blueprint"), PickerClass))
	{
		World->DestroyWorld(false);
		return false;
	}
	ASHPlayerRepresentation* Picker = World->SpawnActor<ASHPlayerRepresentation>(PickerClass);
	UWidgetComponent* Widget = Picker->FindComponentByClass<UWidgetComponent>();
	UStaticMeshComponent* Mesh = Picker->FindComponentByClass<UStaticMeshComponent>();
	if (!TestNotNull(TEXT("Representation widget"), Widget) ||
		!TestNotNull(TEXT("Visible selection mesh"), Mesh))
	{
		World->DestroyWorld(false);
		return false;
	}

	// A large transparent widget surface extends beyond the visible mesh.
	Widget->SetDrawAtDesiredSize(false);
	Widget->SetDrawSize(FVector2D(400.0, 400.0));
	// NullRHI does not draw the widget; registration updates CurrentDrawSize.
	Widget->ReregisterComponent();
	Widget->UpdateBodySetup(true);
	Widget->RecreatePhysicsState();
	const FVector Normal = Widget->GetForwardVector();
	const FVector OutsideMesh = Widget->GetComponentLocation() + Widget->GetRightVector() * 70.0;
	ASHCard* Card = World->SpawnActor<ASHCard>();
	UBoxComponent* CardSurface = NewObject<UBoxComponent>(Card);
	Card->AddInstanceComponent(CardSurface);
	Card->SetRootComponent(CardSurface);
	CardSurface->SetBoxExtent(FVector(4.0));
	CardSurface->SetCollisionProfileName(TEXT("BlockAllDynamic"));
	CardSurface->RegisterComponent();
	Card->SetActorLocation(OutsideMesh - Normal * 20.0);
	auto Trace = [World, Normal](const FVector& Point)
	{
		FHitResult Hit;
		World->LineTraceSingleByChannel(Hit, Point + Normal * 100.0,
			Point - Normal * 100.0, ECC_Visibility, FCollisionQueryParams(NAME_None, true));
		return Hit;
	};

	// Reproduce the original collision setup independently of BeginPlay.
	Picker->SetActorEnableCollision(true);
	Widget->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	TestEqual(TEXT("Transparent widget area can intercept a visible card"),
		Trace(OutsideMesh).GetComponent(), static_cast<UPrimitiveComponent*>(Widget));
	Picker->DispatchBeginPlay();
	TestEqual(TEXT("Startup leaves the card clickable without selecting a player"),
		Trace(OutsideMesh).GetActor(), static_cast<AActor*>(Card));
	TestFalse(TEXT("Inactive picker collision disabled at startup"), Picker->GetActorEnableCollision());

	ASHHand* LogicalHand = World->SpawnActor<ASHHand>();
	LogicalHand->SetIsNPC(true);
	ASHHand* VisualHand = World->SpawnActor<ASHHand>();
	VisualHand->SetRepresentedHand(LogicalHand);
	Picker->BindToHand(VisualHand);
	for (int32 Cycle = 0; Cycle < 3; ++Cycle)
	{
		Picker->SetSelectable(true);
		TestTrue(TEXT("Eligible picker enabled"), Picker->IsPlayerSelectionEnabled());
		TestEqual(TEXT("Transparent widget cannot block cards during selection either"),
			Trace(OutsideMesh).GetActor(), static_cast<AActor*>(Card));
		TestEqual(TEXT("Visible mesh can still select the participant"),
			Trace(Mesh->GetComponentLocation()).GetComponent(), static_cast<UPrimitiveComponent*>(Mesh));
		Picker->SetSelectable(false);
		TestFalse(TEXT("Finishing selection disables picker collision"), Picker->GetActorEnableCollision());
		TestFalse(TEXT("Inactive mesh does not intercept cursor traces"),
			Trace(Mesh->GetComponentLocation()).bBlockingHit);
	}
	// Re-applying an already false state must also repair a stale collision flag.
	Picker->SetActorEnableCollision(true);
	Picker->SetSelectable(false);
	TestFalse(TEXT("Idempotent deselection repairs collision"), Picker->GetActorEnableCollision());
	World->DestroyWorld(false);
	return true;
}

#endif
