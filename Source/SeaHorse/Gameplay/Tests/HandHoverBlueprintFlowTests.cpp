#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Camera/CameraActor.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/BoxComponent.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "Gameplay/Cards/SHCard.h"
#include "Gameplay/Components/CardsLayoutComponent.h"
#include "Gameplay/Core/SHGameState.h"
#include "Gameplay/Core/SHPlayerController.h"
#include "Gameplay/SHHand.h"
#include "UnrealClient.h"
#include "UObject/UnrealType.h"

namespace
{
// Only the OS window and cursor are replaced. Projection, controller queries,
// saved Blueprint graphs and native layout updates use their production paths.
class FHandHoverTestViewport final : public FViewport
{
public:
	explicit FHandHoverTestViewport(FViewportClient* Client) : FViewport(Client)
	{
		SizeX = 1920; SizeY = 1080;
		InitialPositionX = InitialPositionY = 0;
	}
	virtual void* GetWindow() override { return nullptr; }
	virtual void MoveWindow(int32, int32, int32, int32) override {}
	virtual void Destroy() override {}
	virtual bool SetUserFocus(bool) override { return true; }
	virtual bool KeyState(FKey) const override { return false; }
	virtual int32 GetMouseX() const override { return Mouse.X; }
	virtual int32 GetMouseY() const override { return Mouse.Y; }
	virtual void GetMousePos(FIntPoint& Position, const bool = true) override { Position = Mouse; }
	virtual void SetMouse(int32 X, int32 Y) override { Mouse = FIntPoint(X, Y); }
	virtual void ProcessInput(float) override {}
	virtual FVector2D VirtualDesktopPixelToViewport(FIntPoint Point) const override
	{
		return FVector2D(static_cast<double>(Point.X) / SizeX, static_cast<double>(Point.Y) / SizeY);
	}
	virtual FIntPoint ViewportToVirtualDesktopPixel(FVector2D Point) const override
	{
		return FIntPoint(FMath::RoundToInt(Point.X * SizeX), FMath::RoundToInt(Point.Y * SizeY));
	}
	virtual void InvalidateDisplay() override {}
	virtual FViewportFrame* GetViewportFrame() override { return nullptr; }
private:
	FIntPoint Mouse = FIntPoint(-1, -1);
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSHHandHoverBlueprintFlowTest, "SeaHorse.Gameplay.Input.HandHoverBlueprintFlow",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSHHandHoverBlueprintFlowTest::RunTest(const FString& Parameters)
{
	TGuardValue<bool> ScriptGuard(GAllowActorScriptExecutionInEditor, true);
	UClass* HandClass = LoadClass<ASHHand>(nullptr, TEXT("/Game/SeaHorse/Cards/BP_Hand.BP_Hand_C"));
	UClass* CardClass = LoadClass<ASHCard>(nullptr, TEXT("/Game/SeaHorse/Cards/BP_Card.BP_Card_C"));
	UClass* ControllerClass = LoadClass<ASHPlayerController>(nullptr, TEXT("/Game/SeaHorse/Core/BP_SHPlayerController.BP_SHPlayerController_C"));
	if (!TestNotNull(TEXT("Saved hand Blueprint loads"), HandClass) ||
		!TestNotNull(TEXT("Saved card Blueprint loads"), CardClass) ||
		!TestNotNull(TEXT("Saved controller Blueprint loads"), ControllerClass)) { return false; }

	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	World->SetGameState(World->SpawnActor<ASHGameState>());
	auto* PC = World->SpawnActor<ASHPlayerController>(ControllerClass);
	PC->SetAsLocalPlayerController(); World->AddController(PC);
	auto* Client = NewObject<UGameViewportClient>(GEngine);
	auto* LocalPlayer = NewObject<ULocalPlayer>(GEngine);
	FHandHoverTestViewport Viewport(Client);
	Client->Viewport = &Viewport;
	LocalPlayer->ViewportClient = Client;
	LocalPlayer->Origin = FVector2D::ZeroVector; LocalPlayer->Size = FVector2D::UnitVector;
	LocalPlayer->PlayerController = PC; PC->Player = LocalPlayer; PC->bShowMouseCursor = true;
	auto* Camera = World->SpawnActor<ACameraActor>();
	Camera->SetActorLocation(FVector(0, 250, 450));
	Camera->SetActorRotation((FVector(0, 0, 4) - Camera->GetActorLocation()).Rotation());
	if (!PC->PlayerCameraManager)
	{
		PC->PlayerCameraManager = World->SpawnActor<APlayerCameraManager>();
		PC->PlayerCameraManager->InitializeFor(PC);
	}
	PC->SetViewTarget(Camera);
	PC->PlayerCameraManager->UpdateCamera(1.0f / 60.0f);

	auto* Hand = World->SpawnActor<ASHHand>(HandClass);
	Hand->SetActorLocation(FVector(-90, 0, 4));
	auto* Layout = Hand->FindComponentByClass<USHHandCardsLayoutComponent>();
	if (!TestNotNull(TEXT("Saved hand has the production layout component"), Layout))
	{
		Client->Viewport = nullptr; LocalPlayer->PlayerController = nullptr; PC->Player = nullptr;
		World->DestroyWorld(false); return false;
	}
	// BeginPlay assigns the owning hand. The actor's network/UI BeginPlay is
	// unrelated to hover; initializing its real Blueprint below sets its tick flag.
	Layout->RegisterAllComponentTickFunctions(true);
	Layout->BeginPlay();
	auto* Card = World->SpawnActor<ASHCard>(CardClass);
	Hand->AddCard(Card, 0);
	Hand->Initialize();
	auto* OtherHand = World->SpawnActor<ASHHand>();
	auto* CardBehind = World->SpawnActor<ASHCard>(CardClass);
	OtherHand->AddCard(CardBehind, 0);
	CardBehind->SetActorEnableCollision(false);
	auto* Table = World->SpawnActor<AActor>();
	auto* TableSurface = NewObject<UBoxComponent>(Table);
	Table->AddInstanceComponent(TableSurface); Table->SetRootComponent(TableSurface);
	TableSurface->SetBoxExtent(FVector(1000, 1000, 0.1));
	TableSurface->SetCollisionProfileName(TEXT("BlockAllDynamic")); TableSurface->RegisterComponent();
	UFunction* Tick = Hand->FindFunction(TEXT("ReceiveTick"));
	auto* FocusProperty = FindFProperty<FIntProperty>(HandClass, TEXT("FocusedCardIndex"));
	TestNotNull(TEXT("Saved Blueprint has its hover tick graph"), Tick);
	TestNotNull(TEXT("Saved Blueprint exposes its focus result"), FocusProperty);
	float MouseX = 0, MouseY = 0;
	Viewport.SetMouse(960, 540);
	const bool bMouseAvailable = PC->GetMousePosition(MouseX, MouseY);
	TestTrue(TEXT("Production controller reads the viewport cursor"), bMouseAvailable);
	int32 Scenarios = 0;
	if (Tick && FocusProperty && bMouseAvailable)
	{
		for (bool bCardBehind : {false, true})
		for (bool bLowerEdge : {false, true})
		{
			Viewport.SetMouse(-1, -1);
			Hand->UpdateCardPositions();
			Layout->MoveCardsToDesiredPositions(1.0f);
			FTransform Rest;
			if (!TestTrue(TEXT("Blueprint initialization populated resting poses"), Layout->GetUnfocusedCardTransform(Card, Rest))) { break; }
			Card->SetActorTransform(Rest);
			FTransform BehindPose = Rest;
			BehindPose.AddToTranslation(FVector(0, 0, -2));
			CardBehind->SetActorTransform(BehindPose);
			CardBehind->SetActorEnableCollision(bCardBehind);
			const FBox Bounds = Card->GetComponentsBoundingBox(true);
			const FVector CursorWorld = bLowerEdge
				? FVector(Bounds.GetCenter().X, Bounds.Max.Y - 1.0, Bounds.Max.Z)
				: FVector(Bounds.GetCenter().X, Bounds.GetCenter().Y, Bounds.Max.Z);
			FVector2D Screen;
			if (!TestTrue(TEXT("Production camera projects card into viewport"), PC->ProjectWorldLocationToScreen(CursorWorld, Screen))) { break; }
			Viewport.SetMouse(FMath::RoundToInt(Screen.X), FMath::RoundToInt(Screen.Y));
			const FString Scenario = FString::Printf(TEXT("%s over %s"),
				bLowerEdge ? TEXT("lower edge") : TEXT("center"), bCardBehind ? TEXT("another card") : TEXT("empty table"));
			if (!TestEqual(*FString::Printf(TEXT("Production query acquires card from %s"), *Scenario), PC->GetHandCardUnderCursor(), Card)) { continue; }
			++Scenarios;
			int32 LostQueries = 0, LostBlueprintFocus = 0;
			for (int32 Frame = 0; Frame < 120; ++Frame)
			{
				struct { float DeltaSeconds = 1.0f / 60.0f; } TickParameters;
				Hand->ProcessEvent(Tick, &TickParameters);
				if (FocusProperty->GetPropertyValue_InContainer(Hand) != 0) { ++LostBlueprintFocus; }
				Layout->TickComponent(TickParameters.DeltaSeconds, LEVELTICK_All, &Layout->PrimaryComponentTick);
				if (PC->GetHandCardUnderCursor() != Card) { ++LostQueries; }
			}
			TestEqual(*FString::Printf(TEXT("Blueprint keeps stationary %s hover during lift"), *Scenario), LostBlueprintFocus, 0);
			TestEqual(*FString::Printf(TEXT("Production cursor query keeps stationary %s hover after movement"), *Scenario), LostQueries, 0);
			TestTrue(*FString::Printf(TEXT("Card reaches focused transform from %s"), *Scenario), Card->GetActorTransform().Equals(Layout->MakeFocusedCardTransform(Rest), 0.05));
			if (bLowerEdge)
			{
				FHitResult ActualHit;
				PC->GetHitResultUnderCursor(ECC_Visibility, true, ActualHit);
				TestEqual(*FString::Printf(TEXT("Lift actually uncovers %s below the fixed cursor"), *Scenario),
					ActualHit.GetActor(), bCardBehind ? static_cast<AActor*>(CardBehind) : Table);
			}
			Viewport.SetMouse(20, 20);
			TestNull(*FString::Printf(TEXT("Leaving %s releases hover"), *Scenario), PC->GetHandCardUnderCursor());
			for (int32 Frame = 0; Frame < 120; ++Frame)
			{
				struct { float DeltaSeconds = 1.0f / 60.0f; } TickParameters;
				Hand->ProcessEvent(Tick, &TickParameters);
				Layout->TickComponent(TickParameters.DeltaSeconds, LEVELTICK_All, &Layout->PrimaryComponentTick);
			}
			TestNull(*FString::Printf(TEXT("Return animation cannot reacquire %s after cursor leaves"), *Scenario), PC->GetHandCardUnderCursor());
			TestTrue(*FString::Printf(TEXT("Card returns to rest after leaving %s"), *Scenario), Card->GetActorTransform().Equals(Rest, 0.05));
		}
	}
	TestEqual(TEXT("Real Blueprint center and lower-edge paths cover empty table and another card"), Scenarios, 4);
	Client->Viewport = nullptr; LocalPlayer->ViewportClient = nullptr;
	LocalPlayer->PlayerController = nullptr; PC->Player = nullptr;
	World->DestroyWorld(false);
	return true;
}
#endif
