#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Misc/App.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Engine/LocalPlayer.h"
#include "Engine/GameViewportClient.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/Button.h"
#include "Components/PrimitiveComponent.h"
#include "Components/TextBlock.h"
#include "Blueprint/WidgetTree.h"
#include "Blueprint/WidgetBlueprintGeneratedClass.h"
#include "WidgetBlueprint.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "UObject/UnrealType.h"
#include "UObject/StrongObjectPtr.h"
#include "Widgets/SOverlay.h"
#include "Gameplay/Cards/CardDefinition.h"
#include "Gameplay/Cards/SHCard.h"
#include "Gameplay/Core/SHPlayerController.h"
#include "Gameplay/Core/SHPlayerState.h"
#include "Gameplay/Presentation/HandRevealWidget.h"
#include "Gameplay/Presentation/SHHandRevealPawn.h"
#include "Gameplay/SHHand.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSHHandRevealPresentationTest, "SeaHorse.Gameplay.UI.PrivateHandRevealPresentation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSHHandRevealPresentationTest::RunTest(const FString& Parameters)
{
	TGuardValue<bool> ScriptGuard(GAllowActorScriptExecutionInEditor, true);
	UClass* CardClass = LoadClass<ASHCard>(nullptr, TEXT("/Game/SeaHorse/Cards/BP_Card.BP_Card_C"));
	UClass* DefinitionA = LoadClass<UCardDefinition>(nullptr,
		TEXT("/Game/SeaHorse/Cards/Definitions/Card_BodgyVampireHunter.Card_BodgyVampireHunter_C"));
	UClass* DefinitionB = LoadClass<UCardDefinition>(nullptr,
		TEXT("/Game/SeaHorse/Cards/Definitions/Card_SeaHorse.Card_SeaHorse_C"));
	UClass* RevealPawnClass = LoadClass<ASHHandRevealPawn>(nullptr,
		TEXT("/Game/SeaHorse/Cards/BP_HandRevealPawn.BP_HandRevealPawn_C"));
	if (!TestNotNull(TEXT("Actual card Blueprint loads"), CardClass) ||
		!TestNotNull(TEXT("Bodgy definition loads"), DefinitionA) ||
		!TestNotNull(TEXT("Sea Horse definition loads"), DefinitionB) ||
		!TestNotNull(TEXT("Existing designer reveal pawn Blueprint loads"), RevealPawnClass)) { return false; }

	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
	auto* PC = World->SpawnActor<ASHPlayerController>();
	PC->SetAsLocalPlayerController();
	World->AddController(PC);
	auto* PlayerState = World->SpawnActor<ASHPlayerState>();
	PlayerState->SetOwner(PC); PC->PlayerState = PlayerState;
	auto* OwnHand = World->SpawnActor<ASHHand>();
	OwnHand->SetOwner(PC); PlayerState->SetHand(OwnHand);
	auto* LocalPlayer = NewObject<ULocalPlayer>(GEngine);
	LocalPlayer->PlayerController = PC; PC->Player = LocalPlayer;
	auto* Viewport = NewObject<UGameViewportClient>(GEngine);
	const TSharedRef<SOverlay> Overlay = SNew(SOverlay);
	Viewport->SetViewportOverlayWidget(nullptr, Overlay);
	GEngine->GetWorldContextFromWorldChecked(World).GameViewport = Viewport;
	if (!PC->PlayerCameraManager)
	{
		PC->PlayerCameraManager = World->SpawnActor<APlayerCameraManager>();
		PC->PlayerCameraManager->InitializeFor(PC);
	}
	auto* TableCamera = World->SpawnActor<ACameraActor>();
	TableCamera->SetActorLocationAndRotation(FVector(340.f, -270.f, 620.f), FRotator(-58.f, 127.f, 11.f));
	UCameraComponent* TableCameraComponent = TableCamera->GetCameraComponent();
	TableCameraComponent->SetFieldOfView(73.f);
	TableCameraComponent->AspectRatio = 1.9f;
	TableCameraComponent->bConstrainAspectRatio = true;
	TableCameraComponent->bOverrideAspectRatioAxisConstraint = true;
	TableCameraComponent->AspectRatioAxisConstraint = AspectRatio_MaintainYFOV;
	TableCameraComponent->PostProcessBlendWeight = 0.65f;
	TableCameraComponent->PostProcessSettings.bOverride_AutoExposureBias = true;
	TableCameraComponent->PostProcessSettings.AutoExposureBias = -0.7f;
	PC->SetViewTarget(TableCamera);
	// The rendered view uses a camera-manager FOV lock even when the camera's
	// own field of view differs. Capture the same effective view as LocalPlayer.
	PC->PlayerCameraManager->SetFOV(64.f);
	PC->PlayerCameraManager->UpdateCamera(1.f / 60.f);
	auto GetRenderedView = [PC]()
	{
		FMinimalViewInfo View = PC->PlayerCameraManager->GetCameraCacheView();
		View.FOV = PC->PlayerCameraManager->GetFOVAngle();
		PC->GetPlayerViewPoint(View.Location, View.Rotation);
		return View;
	};
	const FMinimalViewInfo PerspectiveTableView = GetRenderedView();
	auto TestSameView = [this](const FString& Context, const FMinimalViewInfo& Actual, const FMinimalViewInfo& Expected)
	{
		TestTrue(Context + TEXT(" preserves camera location"), Actual.Location.Equals(Expected.Location, 0.001));
		TestTrue(Context + TEXT(" preserves camera rotation including roll"), Actual.Rotation.Equals(Expected.Rotation, 0.001));
		TestEqual(Context + TEXT(" preserves projection mode"), Actual.ProjectionMode, Expected.ProjectionMode);
		TestEqual(Context + TEXT(" preserves field of view"), Actual.FOV, Expected.FOV);
		TestEqual(Context + TEXT(" preserves orthographic width"), Actual.OrthoWidth, Expected.OrthoWidth);
		TestEqual(Context + TEXT(" preserves aspect ratio"), Actual.AspectRatio, Expected.AspectRatio);
		TestEqual(Context + TEXT(" preserves constrained framing"), bool(Actual.bConstrainAspectRatio), bool(Expected.bConstrainAspectRatio));
		TestTrue(Context + TEXT(" preserves FOV axis constraint"), Actual.AspectRatioAxisConstraint == Expected.AspectRatioAxisConstraint);
		TestEqual(Context + TEXT(" preserves post-process weight"), Actual.PostProcessBlendWeight, Expected.PostProcessBlendWeight);
		TestEqual(Context + TEXT(" preserves exposure override"), bool(Actual.PostProcessSettings.bOverride_AutoExposureBias),
			bool(Expected.PostProcessSettings.bOverride_AutoExposureBias));
		TestEqual(Context + TEXT(" preserves exposure bias"), Actual.PostProcessSettings.AutoExposureBias,
			Expected.PostProcessSettings.AutoExposureBias);
		TestEqual(Context + TEXT(" preserves orthographic near plane"), Actual.OrthoNearClipPlane, Expected.OrthoNearClipPlane);
		TestEqual(Context + TEXT(" preserves orthographic far plane"), Actual.OrthoFarClipPlane, Expected.OrthoFarClipPlane);
	};
	auto TestCameraRelativeCards = [this](ASHHandRevealPawn* Stage, const FMinimalViewInfo& View)
	{
		TestTrue(TEXT("Private pawn moves to the actual local camera position"), Stage->GetActorLocation().Equals(View.Location, 0.001));
		const FQuat CameraRotation = View.Rotation.Quaternion();
		const FVector Forward = CameraRotation.GetAxisX();
		const FVector RootOffset = Stage->CardsRoot->GetComponentLocation() - View.Location;
		TestTrue(TEXT("Cards are placed in front of the preserved camera"), FVector::DotProduct(RootOffset, Forward) > 0.);
		const FQuat CardPlaneRotation = Stage->CardsRoot->GetComponentQuat();
		TestTrue(TEXT("Card layout horizontal axis follows screen right"), CardPlaneRotation.GetAxisX().Equals(CameraRotation.GetAxisY(), 0.001));
		TestTrue(TEXT("Card layout upward hover follows screen up"), CardPlaneRotation.GetAxisY().Equals(-CameraRotation.GetAxisZ(), 0.001));
		TestTrue(TEXT("Card faces point toward the camera"), CardPlaneRotation.GetAxisZ().Equals(-Forward, 0.001));
		FBox2D ProjectedBounds(ForceInit);
		for (ASHCard* Visual : Stage->GetPresentationCards())
		{
			TestTrue(TEXT("Every private visual remains in front of the camera"),
				FVector::DotProduct(Visual->GetActorLocation() - View.Location, Forward) > 0.);
			const FBox Bounds = Visual->CalculateComponentsBoundingBoxInLocalSpace(true);
			TestTrue(TEXT("Private visual has render bounds despite disabled collision"), bool(Bounds.IsValid));
			for (int32 CornerIndex = 0; Bounds.IsValid && CornerIndex < 8; ++CornerIndex)
			{
				const FVector Corner((CornerIndex & 1) ? Bounds.Max.X : Bounds.Min.X,
					(CornerIndex & 2) ? Bounds.Max.Y : Bounds.Min.Y,
					(CornerIndex & 4) ? Bounds.Max.Z : Bounds.Min.Z);
				const FVector Offset = Visual->GetActorTransform().TransformPosition(Corner) - View.Location;
				ProjectedBounds += FVector2D(FVector::DotProduct(Offset, CameraRotation.GetAxisY()),
					FVector::DotProduct(Offset, CameraRotation.GetAxisZ()));
			}
		}
		// The fit centers the entire fan, including extra space for hover, so its
		// root need not lie exactly on the viewing axis. The visible fan must still
		// occupy the center of the preserved table view.
		const double HoverMargin = Stage->HoverForwardOffset * Stage->CardsRoot->GetComponentScale().GetMax() + 0.001;
		TestTrue(TEXT("Private fan occupies the horizontal screen center"), ProjectedBounds.bIsValid &&
			ProjectedBounds.Min.X <= HoverMargin && ProjectedBounds.Max.X >= -HoverMargin);
		TestTrue(TEXT("Private fan occupies the vertical screen center"), ProjectedBounds.bIsValid &&
			ProjectedBounds.Min.Y <= HoverMargin && ProjectedBounds.Max.Y >= -HoverMargin);
	};
	PC->bShowMouseCursor = false;
	PC->bEnableClickEvents = true;
	PC->bEnableMouseOverEvents = true;
	PC->bAutoManageActiveCameraTarget = true;
	auto* SourceHand = World->SpawnActor<ASHHand>();
	TArray<FSHRevealedHandCard> Cards;
	for (UClass* Definition : {DefinitionA, DefinitionB})
	{
		auto* Source = World->SpawnActor<ASHCard>();
		Source->SetOwner(SourceHand);
		Source->SetCardZone(ECardZone::Hand);
		Source->SetCardDefinition(Definition);
		FSHRevealedHandCard& Entry = Cards.AddDefaulted_GetRef();
		Entry.SourceCard = Source;
		Entry.CardDefinition = Definition;
		Entry.CardActorClass = CardClass;
	}
	const TArray<FSHRevealedHandCard> InitialCards = Cards;
	auto PublicDefinition = [](const ASHCard* Card)
	{
		return FindFProperty<FClassProperty>(ASHCard::StaticClass(), TEXT("RevealedCardDefinition"))
			->GetObjectPropertyValue_InContainer(Card);
	};
	auto CurrentWidget = [PC]()
	{
		return Cast<UHandRevealWidget>(FindFProperty<FObjectPropertyBase>(ASHPlayerController::StaticClass(),
			TEXT("ActiveHandRevealWidget"))->GetObjectPropertyValue_InContainer(PC));
	};
	auto SpawnStage = [World, PC]()
	{
		auto* Pawn = World->SpawnActor<ASHHandRevealPawn>();
		Pawn->SetOwner(PC);
		Pawn->SetActorLocation(FVector(0.f, 0.f, 10000.f));
		return Pawn;
	};
	ASHHandRevealPawn* ViewerStage = SpawnStage();
	// An existing Blueprint may retain old stage-camera defaults. They must not
	// move or zoom the table when the private view starts.
	ViewerStage->RevealCamera->SetRelativeLocation(FVector(15.f, 30.f, 450.f));
	ViewerStage->RevealCamera->SetRelativeRotation(FRotator(-90.f, -90.f, 0.f));
	ViewerStage->RevealCamera->ProjectionMode = ECameraProjectionMode::Orthographic;
	ViewerStage->RevealCamera->OrthoWidth = 41.f;
	const FGuid SessionId = FGuid::NewGuid();
	PC->ClientBeginHandReveal_Implementation(SessionId, SourceHand, nullptr, Cards, false, true, nullptr);
	TestFalse(TEXT("Unmapped pawn waits for retry without opening a partial session"), PC->IsViewingRevealedHand());
	TestNull(TEXT("Unmapped pawn does not create controls"), CurrentWidget());
	TestEqual(TEXT("Unmapped pawn leaves the table camera intact"), PC->GetViewTarget(), static_cast<AActor*>(TableCamera));
	PC->AutoManageActiveCameraTarget(ViewerStage);
	TestEqual(TEXT("Possession camera automation preserves the table before snapshot initialization"),
		PC->GetViewTarget(), static_cast<AActor*>(TableCamera));
	PC->ClientBeginHandReveal_Implementation(SessionId, SourceHand, ViewerStage, Cards, false, true, nullptr);
	TestEqual(TEXT("Private stage becomes the active view target"), PC->GetViewTarget(), static_cast<AActor*>(ViewerStage));
	FMinimalViewInfo RevealView;
	ViewerStage->CalcCamera(0.f, RevealView);
	TestSameView(TEXT("Perspective reveal"), RevealView, PerspectiveTableView);
	PC->PlayerCameraManager->UpdateCamera(1.f / 60.f);
	TestSameView(TEXT("Rendered perspective reveal"), GetRenderedView(), PerspectiveTableView);
	TestCameraRelativeCards(ViewerStage, PerspectiveTableView);
	TestTrue(TEXT("Viewer session is active"), PC->IsViewingRevealedHand());
	TestEqual(TEXT("Presentation stores the exact session identity"), ViewerStage->GetRevealSessionId(), SessionId);
	TestTrue(TEXT("Viewer can finish"), ViewerStage->CanFinishViewing());
	TestFalse(TEXT("Viewer cannot rearrange somebody else's hand"), ViewerStage->CanReorderCards());
	TestTrue(TEXT("Private stage is owner relevant only"), ViewerStage->bOnlyRelevantToOwner);
	TestTrue(TEXT("Mouse is visible while viewing"), PC->bShowMouseCursor);
	TestFalse(TEXT("Board click dispatch is disabled"), PC->bEnableClickEvents);
	TestFalse(TEXT("Board mouse-over dispatch is disabled"), PC->bEnableMouseOverEvents);
	UHandRevealWidget* FirstWidget = CurrentWidget();
	// Compiling the transient Designer Blueprint below performs GC after this
	// widget has left the viewport. Keep the stale reference alive deliberately.
	TStrongObjectPtr<UHandRevealWidget> FirstWidgetKeeper(FirstWidget);
	if (TestNotNull(TEXT("Default reveal controls exist"), FirstWidget))
	{
		TestTrue(TEXT("Default controls are in the viewport"), FirstWidget->IsInViewport());
		TestTrue(TEXT("Viewer's widget may finish this session"), FirstWidget->CanFinishViewing());
		TestEqual(TEXT("Widget exposes source hand"), FirstWidget->GetSourceHand(), SourceHand);
		TestNotNull(TEXT("Default controls contain the Done button"), FirstWidget->GetWidgetFromName(TEXT("FinishButton")));
	}
	const TArray<ASHCard*> Visuals = ViewerStage->GetPresentationCards();
	TestEqual(TEXT("Every card has a local visual copy"), Visuals.Num(), Cards.Num());
	PC->ClientBeginHandReveal_Implementation(SessionId, SourceHand, nullptr, Cards, false, true, nullptr);
	TestEqual(TEXT("An unmapped retry cannot close the active stage"), PC->GetActiveHandRevealPawn(), ViewerStage);
	TestEqual(TEXT("An unmapped retry preserves active controls"), CurrentWidget(), FirstWidget);
	PC->ClientBeginHandReveal_Implementation(SessionId, SourceHand, ViewerStage, Cards, true, true, nullptr);
	TestEqual(TEXT("Repeated Begin preserves the current widget"), CurrentWidget(), FirstWidget);
	TestTrue(TEXT("Repeated Begin applies current reorder permission"), ViewerStage->CanReorderCards());
	TestEqual(TEXT("Repeated Begin preserves the current view target"), PC->GetViewTarget(), static_cast<AActor*>(ViewerStage));
	const TArray<ASHCard*> RetryVisuals = ViewerStage->GetPresentationCards();
	TestTrue(TEXT("Repeated Begin preserves all existing visual actors"), RetryVisuals == Visuals);
	PC->ClientBeginHandReveal_Implementation(SessionId, SourceHand, ViewerStage, Cards, false, true, nullptr);
	TestFalse(TEXT("Repeated Begin may also revoke reorder permission"), ViewerStage->CanReorderCards());
	for (int32 Index = 0; Index < Visuals.Num(); ++Index)
	{
		ASHCard* Visual = Visuals[Index];
		TestNotEqual(TEXT("Presentation never moves the real card actor"), Visual, Cards[Index].SourceCard.Get());
		TestEqual(TEXT("Presentation uses actual Blueprint card visuals"), Visual->GetClass(), CardClass);
		TestEqual(TEXT("Local visual knows its private definition"), Visual->GetCardDefinition().Get(), Cards[Index].CardDefinition.Get());
		TestFalse(TEXT("Local visual cannot replicate"), Visual->GetIsReplicated());
		TestFalse(TEXT("Local visual cannot intercept board collision"), Visual->GetActorEnableCollision());
		TestFalse(TEXT("Local visual does not run the regular card tick"), Visual->IsActorTickEnabled());
		TestEqual(TEXT("Local copy is not a gameplay hand card"), Visual->GetCardZone(), ECardZone::None);
		TestTrue(TEXT("Local card face is visible"), Visual->bFaceUp);
		TestNull(TEXT("Presentation never publishes the copy's definition"), PublicDefinition(Visual));
		TestNull(TEXT("Presentation never publishes the real card's definition"), PublicDefinition(Cards[Index].SourceCard));
		TestFalse(TEXT("The real opponent card remains inaccessible to ordinary inspection"), PC->CanInspectCard(Cards[Index].SourceCard));
		TestEqual(TEXT("The real card retains its hand owner"), Cards[Index].SourceCard->GetOwner(), static_cast<AActor*>(SourceHand));
		TestTrue(TEXT("The real card remains at its original location"), Cards[Index].SourceCard->GetActorLocation().IsNearlyZero());
		TInlineComponentArray<UPrimitiveComponent*> Primitives(Visual);
		TestTrue(TEXT("Actual card meshes exist"), Primitives.Num() > 0);
		for (UPrimitiveComponent* Primitive : Primitives)
		{
			TestTrue(TEXT("Private meshes are only visible to their owning view"), bool(Primitive->bOnlyOwnerSee));
			TestEqual(TEXT("Private mesh collision is disabled"), Primitive->GetCollisionEnabled(), ECollisionEnabled::NoCollision);
		}
		auto* Face = Cast<UUserWidget>(FindFProperty<FObjectPropertyBase>(ASHCard::StaticClass(), TEXT("CardFaceWidget"))
			->GetObjectPropertyValue_InContainer(Visual));
		auto* RenderTarget = Cast<UTextureRenderTarget2D>(FindFProperty<FObjectPropertyBase>(ASHCard::StaticClass(), TEXT("CardFaceRenderTarget"))
			->GetObjectPropertyValue_InContainer(Visual));
		// FWidgetRenderer deliberately returns nullptr with -NullRHI. The real-RHI
		// run verifies the texture; widget data is checked in both configurations.
		if (FApp::CanEverRender())
		{
			TestNotNull(TEXT("The full Blueprint face was rendered"), RenderTarget);
		}
		if (TestNotNull(TEXT("The face widget exists"), Face) && Face->WidgetTree)
		{
			TArray<FString> Texts;
			Face->WidgetTree->ForEachWidget([&Texts](UWidget* Widget)
			{
				if (const auto* Text = Cast<UTextBlock>(Widget)) { Texts.Add(Text->GetText().ToString()); }
			});
			const UCardDefinition* Definition = Cards[Index].CardDefinition->GetDefaultObject<UCardDefinition>();
			TestTrue(TEXT("Rendered Blueprint face includes the card name"), Texts.Contains(Definition->CardName.ToString()));
			TestTrue(TEXT("Rendered Blueprint face includes the complete effect description"), Texts.Contains(Definition->SkillDesc.ToString()));
		}
	}

	Cards.Swap(0, 1);
	PC->ClientUpdateHandReveal_Implementation(FGuid::NewGuid(), Cards, true);
	TestEqual(TEXT("Stale snapshot cannot change the active private order"), ViewerStage->GetCards()[0].SourceCard.Get(), InitialCards[0].SourceCard.Get());
	TestFalse(TEXT("Stale snapshot cannot grant drag permission"), ViewerStage->CanReorderCards());
	PC->ClientUpdateHandReveal_Implementation(SessionId, Cards, false);
	TestEqual(TEXT("Current snapshot changes both displayed identities and order"), ViewerStage->GetCards()[0].SourceCard.Get(), InitialCards[1].SourceCard.Get());
	ViewerStage->CalcCamera(0.f, RevealView);
	TestSameView(TEXT("Reordered perspective reveal"), RevealView, PerspectiveTableView);
	if (Visuals.Num() == 2)
	{
		const auto Reordered = ViewerStage->GetPresentationCards();
		TestEqual(TEXT("A reorder reuses the first visual actor instead of rendering a replacement"), Reordered[0], Visuals[1]);
		TestEqual(TEXT("A reorder reuses the second visual actor"), Reordered[1], Visuals[0]);
	}
	PC->ClientEndHandReveal_Implementation(FGuid::NewGuid());
	TestEqual(TEXT("Stale close leaves the current stage active"), PC->GetActiveHandRevealPawn(), ViewerStage);
	PC->ClientEndHandReveal_Implementation(SessionId);
	TestFalse(TEXT("Session ends"), PC->IsViewingRevealedHand());
	TestNull(TEXT("Controller releases the stage"), PC->GetActiveHandRevealPawn());
	TestEqual(TEXT("Exact prior camera is restored"), PC->GetViewTarget(), static_cast<AActor*>(TableCamera));
	TestFalse(TEXT("Prior cursor setting is restored"), PC->bShowMouseCursor);
	TestTrue(TEXT("Prior click setting is restored"), PC->bEnableClickEvents);
	TestTrue(TEXT("Prior mouse-over setting is restored"), PC->bEnableMouseOverEvents);
	TestTrue(TEXT("Prior camera automation setting is restored"), PC->bAutoManageActiveCameraTarget);
	TestEqual(TEXT("Private snapshot is erased when the panel closes"), ViewerStage->GetCards().Num(), 0);
	TestEqual(TEXT("Every local visual reference is released"), ViewerStage->GetPresentationCards().Num(), 0);
	for (ASHCard* Visual : Visuals) { TestTrue(TEXT("Local visual actors are destroyed on close"), Visual->IsActorBeingDestroyed()); }
	if (FirstWidget)
	{
		TestFalse(TEXT("Closed controls leave the viewport"), FirstWidget->IsInViewport());
		TestFalse(TEXT("Closed controls cannot complete another session"), FirstWidget->CanFinishViewing());
	}

	// The player showing a Sea Horse may rearrange cards, but cannot end the view.
	PC->PlayerCameraManager->UnlockFOV();
	TableCamera->SetActorLocationAndRotation(FVector(-480.f, 170.f, 510.f), FRotator(-74.f, -33.f, -7.f));
	TableCameraComponent->ProjectionMode = ECameraProjectionMode::Orthographic;
	TableCameraComponent->OrthoWidth = 365.f;
	TableCameraComponent->bAutoCalculateOrthoPlanes = false;
	TableCameraComponent->OrthoNearClipPlane = 4.f;
	TableCameraComponent->OrthoFarClipPlane = 5000.f;
	TableCameraComponent->AspectRatio = 1.6f;
	PC->PlayerCameraManager->UpdateCamera(1.f / 60.f);
	const FMinimalViewInfo OrthographicTableView = GetRenderedView();
	ASHHandRevealPawn* ShowingStage = World->SpawnActor<ASHHandRevealPawn>(RevealPawnClass);
	ShowingStage->SetOwner(PC);
	ShowingStage->SetActorLocation(FVector(0.f, 0.f, 10000.f));
	const FGuid ShowingSession = FGuid::NewGuid();
	auto* BP = CastChecked<UWidgetBlueprint>(FKismetEditorUtilities::CreateBlueprint(
		UHandRevealWidget::StaticClass(), GetTransientPackage(),
		MakeUniqueObjectName(GetTransientPackage(), UWidgetBlueprint::StaticClass(), TEXT("TestHandRevealControls")),
		BPTYPE_Normal, UWidgetBlueprint::StaticClass(), UWidgetBlueprintGeneratedClass::StaticClass()));
	if (!BP->WidgetTree) { BP->WidgetTree = NewObject<UWidgetTree>(BP); }
	BP->WidgetTree->RootWidget = BP->WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("InformationText"));
	FKismetEditorUtilities::CompileBlueprint(BP);
	PC->ClientBeginHandReveal_Implementation(ShowingSession, SourceHand, ShowingStage, Cards, true, false, BP->GeneratedClass.Get());
	ShowingStage->CalcCamera(0.f, RevealView);
	TestSameView(TEXT("Orthographic reveal"), RevealView, OrthographicTableView);
	PC->PlayerCameraManager->UpdateCamera(1.f / 60.f);
	TestSameView(TEXT("Rendered orthographic reveal"), GetRenderedView(), OrthographicTableView);
	TestCameraRelativeCards(ShowingStage, OrthographicTableView);
	TestTrue(TEXT("Showing player with Sea Horse can rearrange"), ShowingStage->CanReorderCards());
	TestFalse(TEXT("Showing player cannot end the viewer's session"), ShowingStage->CanFinishViewing());
	if (UHandRevealWidget* Designer = CurrentWidget(); TestNotNull(TEXT("Configured Designer controls are used"), Designer))
	{
		TestEqual(TEXT("Designer class is preserved"), Designer->GetClass(), BP->GeneratedClass.Get());
		TestTrue(TEXT("Native fallback does not replace the Designer root"), Designer->WidgetTree->RootWidget->IsA<UTextBlock>());
		TestFalse(TEXT("Showing player's Designer widget cannot finish"), Designer->CanFinishViewing());
		if (FirstWidget) { TestFalse(TEXT("Old viewer controls remain invalid during a new reveal"), FirstWidget->CanFinishViewing()); }
	}
	PC->ClientUpdateHandReveal_Implementation(SessionId, InitialCards, false);
	PC->ClientEndHandReveal_Implementation(SessionId);
	TestTrue(TEXT("Earlier session messages cannot revoke the showing player's current permissions"), ShowingStage->CanReorderCards());
	TestEqual(TEXT("Earlier close cannot dismiss the newer stage"), PC->GetActiveHandRevealPawn(), ShowingStage);
	PC->ClientUpdateHandReveal_Implementation(ShowingSession, Cards, false);
	ShowingStage->CalcCamera(0.f, RevealView);
	TestSameView(TEXT("Updated orthographic reveal"), RevealView, OrthographicTableView);
	TestFalse(TEXT("Current server snapshot can revoke drag permission"), ShowingStage->CanReorderCards());
	const auto ShowingVisuals = ShowingStage->GetPresentationCards();
	PC->ClientEndHandReveal_Implementation(ShowingSession);
	TestEqual(TEXT("Second reveal also restores the exact table camera"), PC->GetViewTarget(), static_cast<AActor*>(TableCamera));
	for (ASHCard* Visual : ShowingVisuals) { TestTrue(TEXT("Second reveal cleans up every copy"), Visual->IsActorBeingDestroyed()); }
	ViewerStage->Destroy(); ShowingStage->Destroy();
	FirstWidgetKeeper.Reset();
	GEngine->GetWorldContextFromWorldChecked(World).GameViewport = nullptr;
	PC->Player = nullptr; LocalPlayer->PlayerController = nullptr;
	World->DestroyWorld(false); GEngine->DestroyWorldContext(World);
	return true;
}
#endif
