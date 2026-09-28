#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Camera/CameraActor.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/InputComponent.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/InputDelegateBinding.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "EnhancedInputComponent.h"
#include "EnhancedPlayerInput.h"
#include "GameFramework/PlayerInput.h"
#include "Gameplay/Cards/CardDefinition.h"
#include "Gameplay/Cards/SHCard.h"
#include "Gameplay/Components/CardsLayoutComponent.h"
#include "Gameplay/Core/SHGameState.h"
#include "Gameplay/Core/SHPlayerController.h"
#include "Gameplay/Core/SHPlayerState.h"
#include "Gameplay/Presentation/SHHandRevealPawn.h"
#include "Gameplay/SHHand.h"
#include "InputKeyEventArgs.h"
#include "UnrealClient.h"
#include "UObject/UnrealType.h"

namespace
{
class FHandRevealDragViewport final : public FViewport
{
public:
	explicit FHandRevealDragViewport(FViewportClient* Client) : FViewport(Client)
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
	FIntPoint Mouse = FIntPoint(960, 540);
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSHHandRevealInterruptsDragTest, "SeaHorse.Gameplay.UI.HandRevealInterruptsDrag",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSHHandRevealInterruptsDragTest::RunTest(const FString& Parameters)
{
	TGuardValue<bool> ScriptGuard(GAllowActorScriptExecutionInEditor, true);
	UClass* HandClass = LoadClass<ASHHand>(nullptr, TEXT("/Game/SeaHorse/Cards/BP_Hand.BP_Hand_C"));
	UClass* CardClass = LoadClass<ASHCard>(nullptr, TEXT("/Game/SeaHorse/Cards/BP_Card.BP_Card_C"));
	UClass* ControllerClass = LoadClass<ASHPlayerController>(nullptr, TEXT("/Game/SeaHorse/Core/BP_SHPlayerController.BP_SHPlayerController_C"));
	UClass* StateClass = LoadClass<ASHGameState>(nullptr, TEXT("/Game/SeaHorse/Core/BP_SHGameState.BP_SHGameState_C"));
	UClass* SeaHorse = LoadClass<UCardDefinition>(nullptr, TEXT("/Game/SeaHorse/Cards/Definitions/Card_SeaHorse.Card_SeaHorse_C"));
	if (!TestNotNull(TEXT("Saved hand Blueprint"), HandClass) || !TestNotNull(TEXT("Saved card Blueprint"), CardClass) ||
		!TestNotNull(TEXT("Saved controller Blueprint"), ControllerClass) || !TestNotNull(TEXT("Saved GameState Blueprint"), StateClass) ||
		!TestNotNull(TEXT("Sea Horse definition"), SeaHorse)) { return false; }

	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
	ASHGameState* State = World->SpawnActor<ASHGameState>(StateClass);
	World->SetGameState(State);
	ASHPlayerController* PC = World->SpawnActor<ASHPlayerController>(ControllerClass);
	PC->SetAsLocalPlayerController(); World->AddController(PC);
	auto* LocalPlayer = NewObject<ULocalPlayer>(GEngine);
	auto* ViewportClient = NewObject<UGameViewportClient>(GEngine);
	FHandRevealDragViewport Viewport(ViewportClient);
	ViewportClient->Viewport = &Viewport;
	LocalPlayer->ViewportClient = ViewportClient;
	LocalPlayer->Origin = FVector2D::ZeroVector; LocalPlayer->Size = FVector2D::UnitVector;
	LocalPlayer->PlayerController = PC; PC->Player = LocalPlayer; PC->bShowMouseCursor = true;
	// Match DefaultInput.ini. Saved DebugKey nodes bind only to an Enhanced input component.
	PC->PlayerInput = NewObject<UEnhancedPlayerInput>(PC);
	PC->InputComponent = NewObject<UEnhancedInputComponent>(PC);
	PC->InputComponent->RegisterComponent();
	UInputDelegateBinding::BindInputDelegates(ControllerClass, PC->InputComponent, PC);
	TArray<UInputComponent*> InputStack { PC->InputComponent };
#if DEV_ONLY_KEY_BINDINGS_AVAILABLE
	TestTrue(TEXT("Saved Blueprint has its actual Enhanced LMB release binding"),
		CastChecked<UEnhancedInputComponent>(PC->InputComponent)->GetDebugKeyBindings().ContainsByPredicate(
			[](const TUniquePtr<FInputDebugKeyBinding>& Binding)
			{
				return Binding->Chord.Key == EKeys::LeftMouseButton && Binding->KeyEvent == IE_Released;
			}));
#endif
	ASHPlayerState* Player = World->SpawnActor<ASHPlayerState>();
	Player->SetOwner(PC); PC->PlayerState = Player;
	ASHPlayerState* OtherPlayer = World->SpawnActor<ASHPlayerState>();
	State->SetCurrentPlayer(OtherPlayer);
	State->SetTurnPhase(ETurnPhase::FirstPairing);
	ASHHand* Hand = World->SpawnActor<ASHHand>(HandClass);
	Hand->SetOwner(PC); Player->SetHand(Hand); Hand->SetRepresentedPlayerState(Player);
	ASHCard* Card = World->SpawnActor<ASHCard>(CardClass);
	Card->SetCardDefinition(SeaHorse); Hand->AddCard(Card, 0);
	auto* Layout = Hand->FindComponentByClass<USHHandCardsLayoutComponent>();
	auto* PressedProperty = FindFProperty<FObjectPropertyBase>(ControllerClass, TEXT("PressedCard"));
	auto* DraggingProperty = FindFProperty<FBoolProperty>(ControllerClass, TEXT("IsDragging"));
	auto* MovedDelegate = FindFProperty<FMulticastDelegateProperty>(ControllerClass, TEXT("MovedCard"));
	UFunction* StartDragging = Card->FindFunction(TEXT("StartDragging"));
	UFunction* HandMovedCard = Hand->FindFunction(TEXT("CustomEvent_0"));
	UFunction* CardTick = Card->FindFunction(TEXT("ReceiveTick"));
	if (!TestNotNull(TEXT("Real hand layout"), Layout) || !TestNotNull(TEXT("BP pressed-card state"), PressedProperty) ||
		!TestNotNull(TEXT("BP dragging state"), DraggingProperty) || !TestNotNull(TEXT("BP move delegate"), MovedDelegate) ||
		!TestNotNull(TEXT("BP card drag entry"), StartDragging) || !TestNotNull(TEXT("BP hand move handler"), HandMovedCard) ||
		!TestNotNull(TEXT("BP card drag tick"), CardTick))
	{
		ViewportClient->Viewport = nullptr; LocalPlayer->ViewportClient = nullptr;
		LocalPlayer->PlayerController = nullptr; PC->Player = nullptr;
		World->DestroyWorld(false); GEngine->DestroyWorldContext(World); return false;
	}
	Layout->RegisterAllComponentTickFunctions(true); Layout->BeginPlay();
	Hand->Initialize();
	FScriptDelegate OnMoved;
	OnMoved.BindUFunction(Hand, HandMovedCard->GetFName());
	MovedDelegate->AddDelegate(OnMoved, PC);
	ACameraActor* Camera = World->SpawnActor<ACameraActor>();
	Camera->SetActorLocation(FVector(0, 250, 450));
	Camera->SetActorRotation((FVector(0, 0, 4) - Camera->GetActorLocation()).Rotation());
	if (!PC->PlayerCameraManager)
	{
		PC->PlayerCameraManager = World->SpawnActor<APlayerCameraManager>();
		PC->PlayerCameraManager->InitializeFor(PC);
	}
	PC->SetViewTarget(Camera);
	PC->PlayerCameraManager->UpdateCamera(1.f / 60.f);

	// The player was already dragging on the table when somebody else's effect arrived.
	PC->PlayerInput->InputKey(FInputKeyEventArgs::CreateSimulated(EKeys::LeftMouseButton, IE_Pressed, 1.f));
	PC->PlayerInput->ProcessInputStack({}, 1.f / 60.f, false);
	TestTrue(TEXT("The mouse button was physically held before interruption"), PC->PlayerInput->IsPressed(EKeys::LeftMouseButton));
	PressedProperty->SetObjectPropertyValue_InContainer(PC, Card);
	DraggingProperty->SetPropertyValue_InContainer(PC, true);
	PC->BeginLocalCardDrag(Card);
	Card->ProcessEvent(StartDragging, nullptr);
	TestFalse(TEXT("Real Blueprint starts a drag by disabling card collision"), Card->GetActorEnableCollision());
	TestEqual(TEXT("Native controller tracks the same card"), PC->GetLocallyDraggedCard(), Card);
	const TArray<ASHCard*> OriginalOrder = Hand->GetCards();
	auto* Stage = World->SpawnActor<ASHHandRevealPawn>();
	Stage->SetOwner(PC); Stage->SetActorLocation(FVector(0, 0, 3000));
	PC->Possess(Stage);
	const FGuid Session = FGuid::NewGuid();
	PC->ClientBeginHandReveal_Implementation(Session, Hand, Stage, {}, true, false, nullptr);
	TestTrue(TEXT("Reveal mode is active before the queued release is processed"), PC->IsViewingRevealedHand());
	TestNull(TEXT("Opening reveal clears the native drag immediately"), PC->GetLocallyDraggedCard());

	// Use the actual saved input bindings. A normal InputKey call would now be swallowed by reveal mode.
	PC->PlayerInput->ProcessInputStack(InputStack, 1.f / 60.f, false);
	TestNull(TEXT("Synthetic release clears BP PressedCard through its own release graph"),
		PressedProperty->GetObjectPropertyValue_InContainer(PC));
	TestFalse(TEXT("Synthetic release resets BP IsDragging"), DraggingProperty->GetPropertyValue_InContainer(PC));
	TestTrue(TEXT("Saved MovedCard handler calls BP_Card StopDragging and restores collision"), Card->GetActorEnableCollision());
	TestTrue(TEXT("Cancelling a table drag preserves authoritative hand order"), Hand->GetCards() == OriginalOrder);
	TestNull(TEXT("Board hover remains disabled underneath the reveal camera"), PC->GetHandCardUnderCursor());
	const FTransform BeforeTicks = Card->GetActorTransform();
	for (int32 Frame = 0; Frame < 5; ++Frame)
	{
		struct { float DeltaSeconds = 1.f / 60.f; } TickArgs;
		Card->ProcessEvent(CardTick, &TickArgs);
	}
	TestTrue(TEXT("Interrupted card no longer runs the Blueprint drag gate"), Card->GetActorTransform().Equals(BeforeTicks));
	PC->ClientEndHandReveal_Implementation(Session);
	TestFalse(TEXT("Reveal closes normally after cancelling the drag"), PC->IsViewingRevealedHand());
	TestFalse(TEXT("Old BP drag cannot resume when the reveal closes"), DraggingProperty->GetPropertyValue_InContainer(PC));
	PC->UnPossess();
	ViewportClient->Viewport = nullptr; LocalPlayer->ViewportClient = nullptr;
	LocalPlayer->PlayerController = nullptr; PC->Player = nullptr;
	World->DestroyWorld(false); GEngine->DestroyWorldContext(World);
	return true;
}
#endif
