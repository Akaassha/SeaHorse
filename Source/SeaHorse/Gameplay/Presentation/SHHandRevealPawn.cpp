#include "Gameplay/Presentation/SHHandRevealPawn.h"

#include "Camera/CameraComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/PrimitiveComponent.h"
#include "Components/SplineComponent.h"
#include "Engine/World.h"
#include "Engine/LocalPlayer.h"
#include "Engine/GameViewportClient.h"
#include "SceneView.h"
#include "Framework/Application/SlateApplication.h"
#include "InputKeyEventArgs.h"
#include "Gameplay/Cards/SHCard.h"
#include "Gameplay/Cards/CardDefinition.h"
#include "Gameplay/Core/SHPlayerController.h"
#include "Kismet/GameplayStatics.h"

ASHHandRevealPawn::ASHHandRevealPawn()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;
	bReplicates = true;
	bOnlyRelevantToOwner = true;
	bAlwaysRelevant = false;
	SetReplicateMovement(false);
	AutoPossessAI = EAutoPossessAI::Disabled;
	ViewRoot = CreateDefaultSubobject<USceneComponent>(TEXT("ViewRoot"));
	SetRootComponent(ViewRoot);
	CardsRoot = CreateDefaultSubobject<USceneComponent>(TEXT("CardsRoot"));
	CardsRoot->SetupAttachment(ViewRoot);
	CardLayoutSpline = CreateDefaultSubobject<USplineComponent>(TEXT("CardLayoutSpline"));
	CardLayoutSpline->SetupAttachment(CardsRoot);
	CardLayoutSpline->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	RevealCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("RevealCamera"));
	RevealCamera->SetupAttachment(ViewRoot);
}

void ASHHandRevealPawn::InitializePresentation(ASHPlayerController* PC, FGuid SessionId,
	const TArray<FSHRevealedHandCard>& Cards, bool bCanReorder, bool bCanFinish)
{
	ClearPresentation();
	if (!IsValid(PC) || !PC->IsLocalController() || PC->GetWorld() != GetWorld()) { return; }
	PresentationController = PC;
	CaptureTableView(PC);
	RevealSessionId = SessionId;
	bFinishingAllowed = bCanFinish;
	SetActorTickEnabled(true);
	ApplySnapshot(Cards, bCanReorder);
}

void ASHHandRevealPawn::InitializeComparisonPresentation(ASHPlayerController* PC, FGuid SessionId,
	const TArray<FSHRevealedHandCard>& LargerCards,
	const TArray<FSHRevealedHandCard>& InReceivingCards,
	int32 InRemainingTransfers, bool bCanTransfer)
{
	ClearPresentation();
	if (!IsValid(PC) || !PC->IsLocalController() || PC->GetWorld() != GetWorld()) { return; }
	PresentationController = PC;
	CaptureTableView(PC);
	RevealSessionId = SessionId;
	bComparisonMode = true;
	bFinishingAllowed = false;
	SetActorTickEnabled(true);
	ApplyComparisonSnapshot(LargerCards, InReceivingCards, InRemainingTransfers, bCanTransfer);
}

void ASHHandRevealPawn::CaptureTableView(ASHPlayerController* PC)
{
	// Match LocalPlayer's rendered view, including a camera blend and a locked FOV.
	// This is local: the server may not know the client's current table camera.
	PreservedView = FMinimalViewInfo();
	if (PC->PlayerCameraManager)
	{
		PreservedView = PC->PlayerCameraManager->GetCameraCacheView();
		PreservedView.FOV = PC->PlayerCameraManager->GetFOVAngle();
	}
	else if (AActor* Target = PC->GetViewTarget()) { Target->CalcCamera(0.f, PreservedView); }
	PC->GetPlayerViewPoint(PreservedView.Location, PreservedView.Rotation);
	PreservedView.DesiredFOV = PreservedView.FOV;
	PreservedProjection = PreservedView.CalculateProjectionMatrix();
	if (ULocalPlayer* Player = PC->GetLocalPlayer(); Player && Player->ViewportClient && Player->ViewportClient->Viewport)
	{
		FSceneViewProjectionData Projection;
		if (Player->GetProjectionData(Player->ViewportClient->Viewport, Projection))
		{
			PreservedProjection = Projection.ProjectionMatrix;
		}
	}
	bHasPreservedView = true;
	SetActorLocationAndRotation(PreservedView.Location, PreservedView.Rotation);
	SetActorScale3D(FVector::OneVector);
	// Existing Blueprint subclasses can retain transforms from the old overhead camera.
	RevealCamera->AttachToComponent(ViewRoot, FAttachmentTransformRules::SnapToTargetNotIncludingScale);
	RevealCamera->SetRelativeTransform(FTransform::Identity);
	RevealCamera->ProjectionMode = PreservedView.ProjectionMode;
	RevealCamera->FieldOfView = PreservedView.FOV;
	RevealCamera->OrthoWidth = PreservedView.OrthoWidth;
	RevealCamera->OrthoNearClipPlane = PreservedView.OrthoNearClipPlane;
	RevealCamera->OrthoFarClipPlane = PreservedView.OrthoFarClipPlane;
	RevealCamera->AspectRatio = PreservedView.AspectRatio;
	RevealCamera->bConstrainAspectRatio = PreservedView.bConstrainAspectRatio;
	RevealCamera->PostProcessSettings = PreservedView.PostProcessSettings;
	RevealCamera->PostProcessBlendWeight = PreservedView.PostProcessBlendWeight;
	CardsRoot->AttachToComponent(ViewRoot, FAttachmentTransformRules::KeepRelativeTransform);
	// The card plane uses X = screen right, Y = screen down, Z = towards the viewer.
	CardsRoot->SetRelativeRotation(FRotationMatrix::MakeFromXY(FVector::YAxisVector, -FVector::ZAxisVector).Rotator());
}

void ASHHandRevealPawn::FitCardsToView(const FBox& LayoutBounds)
{
	if (!bHasPreservedView) { return; }
	const bool bPerspective = PreservedView.ProjectionMode == ECameraProjectionMode::Perspective;
	const float NearPlane = bPerspective ? PreservedView.GetFinalPerspectiveNearClipPlane() : PreservedView.OrthoNearClipPlane;
	const float Distance = FMath::Max(CardsDistanceFromCamera, FMath::Max(20.f, NearPlane + 20.f));
	const double ProjectionDepth = bPerspective ? Distance : 1.0;
	const double ViewWidth = 2.0 * ProjectionDepth / FMath::Max(FMath::Abs(PreservedProjection.M[0][0]), 0.001);
	const double ViewHeight = 2.0 * ProjectionDepth / FMath::Max(FMath::Abs(PreservedProjection.M[1][1]), 0.001);
	const FVector Size = LayoutBounds.IsValid ? LayoutBounds.GetSize() : FVector::ZeroVector;
	const FVector Center = LayoutBounds.IsValid ? LayoutBounds.GetCenter() : FVector::ZeroVector;
	const double LayoutWidth = FMath::Max(double(MinimumViewWidth), Size.X * 1.1);
	double Scale = FMath::Min(ViewWidth * ScreenWidthFraction / FMath::Max(LayoutWidth, 1.0),
		ViewHeight * ScreenHeightFraction / FMath::Max(Size.Y * 1.1, 1.0));
	// Leave room for hover and dragging in front of the card plane without clipping.
	const double FrontExtent = LayoutBounds.IsValid ? FMath::Max(LayoutBounds.Max.Z + 2.0, 1.0) : 1.0;
	Scale = FMath::Max(0.001, FMath::Min(Scale, (Distance - NearPlane - 1.0) / FrontExtent));
	CardsRoot->SetRelativeScale3D(FVector(Scale));
	CardsRoot->SetRelativeLocation(FVector(Distance, -Center.X * Scale, Center.Y * Scale));
}

const AActor* ASHHandRevealPawn::GetNetOwner() const
{
	// The viewer does not possess this pawn. APawn's default returns the pawn
	// itself, which would prevent an owner-only channel from opening for it.
	if (ASHPlayerController* PC = Cast<ASHPlayerController>(GetOwner())) { return PC; }
	return Super::GetNetOwner();
}

ASHCard* ASHHandRevealPawn::SpawnVisualCard(const FSHRevealedHandCard& Card)
{
	if (!Card.CardActorClass || !GetWorld()) { return nullptr; }
	const FTransform Transform = CardsRoot->GetComponentTransform();
	ASHCard* Visual = GetWorld()->SpawnActorDeferred<ASHCard>(Card.CardActorClass, Transform,
		this, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (!Visual) { return nullptr; }
	// On a listen host this must happen before FinishSpawning creates an actor channel.
	Visual->SetReplicates(false);
	Visual->SetReplicateMovement(false);
	Visual->SetCardDefinition(Card.CardDefinition);
	UGameplayStatics::FinishSpawningActor(Visual, Transform);
	if (!Visual->GetRootComponent()) { Visual->Destroy(); return nullptr; }
	Visual->SetActorTickEnabled(false);
	Visual->SetActorEnableCollision(false);
	Visual->AttachToComponent(CardsRoot, FAttachmentTransformRules::KeepWorldTransform);
	TInlineComponentArray<UPrimitiveComponent*> Primitives(Visual);
	for (UPrimitiveComponent* Primitive : Primitives)
	{
		Primitive->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Primitive->SetGenerateOverlapEvents(false);
		Primitive->SetCastShadow(false);
		Primitive->SetOnlyOwnerSee(true);
	}
	// The Blueprint override fills every field of W_Card before rendering its face.
	// A null definition is an intentionally hidden opponent card in Diego's comparison.
	// Never recover its definition from SourceCard: a listen host knows all real cards.
	if (Card.CardDefinition) { Visual->Initialize(); }
	Visual->SetFaceUp(Card.CardDefinition != nullptr);
	Visual->ClearInteractionHighlight();
	// Presentation copies intentionally have no collision; include their render geometry.
	FBox Bounds = Visual->CalculateComponentsBoundingBoxInLocalSpace(true);
	if (!Bounds.IsValid || Bounds.GetSize().IsNearlyZero())
	{
		Bounds = FBox(FVector(-5.f, -3.5f, -0.1f), FVector(5.f, 3.5f, 0.1f));
	}
	VisualBounds.Add(Visual, Bounds);
	return Visual;
}

void ASHHandRevealPawn::ApplySnapshot(const TArray<FSHRevealedHandCard>& Cards, bool bCanReorder)
{
	if (!IsValid(PresentationController) || !PresentationController->IsLocalController()) { return; }
	bComparisonMode = false;
	bTransferAllowed = false;
	RemainingTransfers = 0;
	SynchronizeVisualCards({}, ReceivingCards, ReceivingVisualCards);
	SynchronizeVisualCards(Cards, RevealedCards, VisualCards);
	bReorderingAllowed = bCanReorder;
	if (!bReorderingAllowed || !VisualCards.Contains(DraggedVisual.Get())) { DraggedVisual.Reset(); }
	if (!VisualCards.Contains(HoveredVisual.Get())) { HoveredVisual.Reset(); }
	RebuildLayout(true);
	OnPresentationChanged();
}

void ASHHandRevealPawn::SynchronizeVisualCards(const TArray<FSHRevealedHandCard>& NewCards,
	TArray<FSHRevealedHandCard>& StoredCards, TArray<TObjectPtr<ASHCard>>& StoredVisuals)
{
	TArray<TObjectPtr<ASHCard>> NewVisuals;
	NewVisuals.Reserve(NewCards.Num());
	for (const FSHRevealedHandCard& Entry : NewCards)
	{
		const int32 PreviousIndex = StoredCards.IndexOfByPredicate([&Entry](const FSHRevealedHandCard& Old)
		{
			return Old.SourceCard == Entry.SourceCard && Old.CardDefinition == Entry.CardDefinition &&
				Old.CardActorClass == Entry.CardActorClass;
		});
		ASHCard* Visual = StoredVisuals.IsValidIndex(PreviousIndex) ? StoredVisuals[PreviousIndex].Get() : nullptr;
		if (!IsValid(Visual)) { Visual = SpawnVisualCard(Entry); }
		NewVisuals.Add(Visual);
	}
	for (ASHCard* OldVisual : StoredVisuals)
	{
		if (IsValid(OldVisual) && !NewVisuals.Contains(OldVisual))
		{
			RestingTransforms.Remove(OldVisual);
			VisualBounds.Remove(OldVisual);
			OldVisual->Destroy();
		}
	}
	StoredCards = NewCards;
	StoredVisuals = MoveTemp(NewVisuals);
}

void ASHHandRevealPawn::ApplyComparisonSnapshot(const TArray<FSHRevealedHandCard>& LargerCards,
	const TArray<FSHRevealedHandCard>& InReceivingCards,
	int32 InRemainingTransfers, bool bCanTransfer)
{
	if (!IsValid(PresentationController) || !PresentationController->IsLocalController()) { return; }
	bComparisonMode = true;
	bReorderingAllowed = false;
	bTransferAllowed = bCanTransfer && InRemainingTransfers > 0;
	RemainingTransfers = FMath::Max(0, InRemainingTransfers);
	SynchronizeVisualCards(LargerCards, RevealedCards, VisualCards);
	SynchronizeVisualCards(InReceivingCards, ReceivingCards, ReceivingVisualCards);
	if (!bTransferAllowed || !VisualCards.Contains(DraggedVisual.Get())) { DraggedVisual.Reset(); }
	if (!VisualCards.Contains(HoveredVisual.Get())) { HoveredVisual.Reset(); }
	RebuildLayout(true);
	OnPresentationChanged();
}

void ASHHandRevealPawn::RebuildLayout(bool bSnapNewCards)
{
	CardLayoutSpline->ClearSplinePoints(false);
	FBox LayoutBounds(ForceInit);
	auto LayoutRow = [this, bSnapNewCards, &LayoutBounds](const TArray<TObjectPtr<ASHCard>>& Row,
		float RowY, bool bWriteSpline)
	{
		const float Spacing = FMath::Min(PreferredCardSpacing,
			MaximumFanWidth / FMath::Max(Row.Num() - 1, 1));
		const float CenterIndex = (Row.Num() - 1) * 0.5f;
		for (int32 Index = 0; Index < Row.Num(); ++Index)
		{
			ASHCard* Visual = Row[Index];
			const float Offset = Index - CenterIndex;
			const float X = Offset * Spacing;
			const FVector Position(X, RowY + X * X * FanCurvature, Index * 0.025f);
			if (bWriteSpline) { CardLayoutSpline->AddSplinePoint(Position, ESplineCoordinateSpace::Local, false); }
			if (!IsValid(Visual)) { continue; }
			FRotator Rotation = CardRotation;
			Rotation.Yaw += Offset * FanAnglePerCard;
			const FTransform Resting(Rotation, Position, FVector(CardScale));
			if (bSnapNewCards && !RestingTransforms.Contains(Visual)) { Visual->SetActorRelativeTransform(Resting); }
			RestingTransforms.Add(Visual, Resting);
			if (const FBox* Bounds = VisualBounds.Find(Visual))
			{
				LayoutBounds += Bounds->TransformBy(Resting);
				LayoutBounds += Bounds->TransformBy(MakeHoveredTransform(Resting));
			}
		}
	};
	const float TopRowY = bComparisonMode ? -ComparisonRowSpacing * 0.5f : 0.f;
	LayoutRow(VisualCards, TopRowY, true);
	if (bComparisonMode) { LayoutRow(ReceivingVisualCards, ComparisonRowSpacing * 0.5f, false); }
	CardLayoutSpline->UpdateSpline();
	FitCardsToView(LayoutBounds);
}

FTransform ASHHandRevealPawn::MakeHoveredTransform(const FTransform& Resting) const
{
	FTransform Result = Resting;
	Result.AddToTranslation(FVector(0.f, -HoverForwardOffset, HoverLiftHeight));
	Result.SetScale3D(Resting.GetScale3D() * HoverScale);
	return Result;
}

bool ASHHandRevealPawn::GetCursorOnCardPlane(FVector& OutLocalPoint) const
{
	FVector Origin, Direction;
	if (!IsValid(PresentationController) ||
		!PresentationController->DeprojectMousePositionToWorld(Origin, Direction)) { return false; }
	const FTransform RootTransform = CardsRoot->GetComponentTransform();
	const FVector LocalOrigin = RootTransform.InverseTransformPosition(Origin);
	const FVector LocalDirection = RootTransform.InverseTransformVectorNoScale(Direction);
	if (FMath::IsNearlyZero(LocalDirection.Z)) { return false; }
	const double Distance = -LocalOrigin.Z / LocalDirection.Z;
	if (Distance < 0.) { return false; }
	OutLocalPoint = LocalOrigin + LocalDirection * Distance;
	return true;
}

bool ASHHandRevealPawn::ContainsPoint(const ASHCard* Card, const FTransform& Transform, const FVector& Point) const
{
	const FBox* Bounds = VisualBounds.Find(const_cast<ASHCard*>(Card));
	if (!Bounds) { return false; }
	const FVector LocalPoint = Transform.InverseTransformPosition(Point);
	return LocalPoint.X >= Bounds->Min.X && LocalPoint.X <= Bounds->Max.X &&
		LocalPoint.Y >= Bounds->Min.Y && LocalPoint.Y <= Bounds->Max.Y;
}

ASHCard* ASHHandRevealPawn::FindVisualUnderCursor(const FVector& LocalPoint) const
{
	if (ASHCard* Hovered = HoveredVisual.Get())
	{
		if (const FTransform* Resting = RestingTransforms.Find(Hovered))
		{
			// Keep the resting footprint active when the visible card rises away from the cursor.
			if (ContainsPoint(Hovered, *Resting, LocalPoint) ||
				ContainsPoint(Hovered, MakeHoveredTransform(*Resting), LocalPoint) ||
				ContainsPoint(Hovered, Hovered->GetRootComponent()->GetRelativeTransform(), LocalPoint))
			{
				return Hovered;
			}
		}
	}
	for (int32 Index = VisualCards.Num() - 1; Index >= 0; --Index)
	{
		ASHCard* Visual = VisualCards[Index];
		if (IsValid(Visual) && ContainsPoint(Visual,
			Visual->GetRootComponent()->GetRelativeTransform(), LocalPoint)) { return Visual; }
	}
	return nullptr;
}

void ASHHandRevealPawn::UpdateDrag(const FVector& Cursor)
{
	ASHCard* Visual = DraggedVisual.Get();
	if (!Visual || (!bReorderingAllowed && !bTransferAllowed)) { return; }
	const int32 CurrentIndex = VisualCards.IndexOfByKey(Visual);
	if (!RevealedCards.IsValidIndex(CurrentIndex)) { DraggedVisual.Reset(); return; }
	FVector Position = Cursor + DragCursorOffset;
	Position.Z = HoverLiftHeight + 2.f;
	Visual->SetActorRelativeLocation(Position);
	if (bComparisonMode) { return; }
	const float Spacing = FMath::Min(PreferredCardSpacing,
		MaximumFanWidth / FMath::Max(VisualCards.Num() - 1, 1));
	const float FloatingIndex = Position.X / FMath::Max(Spacing, 0.01f) + (VisualCards.Num() - 1) * 0.5f;
	const int32 DropIndex = FMath::Clamp(FMath::RoundToInt(FloatingIndex), 0, VisualCards.Num() - 1);
	// A small dead band stops adjacent reorder RPCs while the pointer rests on a slot border.
	if (DropIndex != LastRequestedDropIndex &&
		(LastRequestedDropIndex == INDEX_NONE || FMath::Abs(FloatingIndex - LastRequestedDropIndex) > 0.6f))
	{
		LastRequestedDropIndex = DropIndex;
		if (ASHCard* Source = RevealedCards[CurrentIndex].SourceCard)
		{
			PresentationController->ServerReorderRevealedHand(RevealSessionId, Source, DropIndex);
		}
	}
}

void ASHHandRevealPawn::TryCommitComparisonDrop(const FVector& Cursor)
{
	if (!bComparisonMode || !bTransferAllowed || RemainingTransfers <= 0 || Cursor.Y <= 0.f) { return; }
	ASHCard* Visual = DraggedVisual.Get();
	const int32 SourceIndex = VisualCards.IndexOfByKey(Visual);
	if (!RevealedCards.IsValidIndex(SourceIndex) || !IsValid(PresentationController)) { return; }
	const float Spacing = FMath::Min(PreferredCardSpacing,
		MaximumFanWidth / FMath::Max(ReceivingVisualCards.Num(), 1));
	const float FloatingIndex = Cursor.X / FMath::Max(Spacing, 0.01f) + ReceivingVisualCards.Num() * 0.5f;
	const int32 InsertIndex = FMath::Clamp(FMath::RoundToInt(FloatingIndex), 0, ReceivingVisualCards.Num());
	if (ASHCard* Source = RevealedCards[SourceIndex].SourceCard)
	{
		PresentationController->ServerTransferComparedHandCard(RevealSessionId, Source, InsertIndex);
	}
}

void ASHHandRevealPawn::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!IsValid(PresentationController) || !PresentationController->IsLocalController()) { return; }
	// Controller InputKey consumes board input during this mode, so PlayerInput's key
	// state need not be updated. Slate also detects a release consumed by a UI button.
	if (DraggedVisual.IsValid() && FSlateApplication::IsInitialized() &&
		!FSlateApplication::Get().GetPressedMouseButtons().Contains(EKeys::LeftMouseButton))
	{
		FVector ReleaseCursor;
		if (GetCursorOnCardPlane(ReleaseCursor)) { TryCommitComparisonDrop(ReleaseCursor); }
		DraggedVisual.Reset();
	}
	FVector Cursor;
	if (GetCursorOnCardPlane(Cursor))
	{
		if (DraggedVisual.IsValid()) { UpdateDrag(Cursor); }
		else { HoveredVisual = FindVisualUnderCursor(Cursor); }
	}
	else { HoveredVisual.Reset(); }
	TArray<TObjectPtr<ASHCard>> AllVisuals = VisualCards;
	AllVisuals.Append(ReceivingVisualCards);
	for (ASHCard* Visual : AllVisuals)
	{
		if (!IsValid(Visual) || Visual == DraggedVisual.Get()) { continue; }
		const FTransform* Resting = RestingTransforms.Find(Visual);
		if (!Resting) { continue; }
		const FTransform Desired = Visual == HoveredVisual.Get() ? MakeHoveredTransform(*Resting) : *Resting;
		const FTransform Current = Visual->GetRootComponent()->GetRelativeTransform();
		const float Alpha = FMath::Clamp(DeltaSeconds * AnimationSpeed, 0.f, 1.f);
		FTransform Next;
		Next.Blend(Current, Desired, Alpha);
		Visual->SetActorRelativeTransform(Next);
	}
}

bool ASHHandRevealPawn::HandlePointerInput(const FInputKeyEventArgs& Params)
{
	if (!IsValid(PresentationController)) { return false; }
	if (Params.Key != EKeys::LeftMouseButton && Params.Key != EKeys::RightMouseButton) { return false; }
	if (Params.Key == EKeys::LeftMouseButton)
	{
		if (Params.Event == IE_Released)
		{
			FVector Cursor;
			if (GetCursorOnCardPlane(Cursor)) { TryCommitComparisonDrop(Cursor); }
			DraggedVisual.Reset();
		}
		else if (Params.Event == IE_Pressed && (bReorderingAllowed || bTransferAllowed))
		{
			FVector Cursor;
			if (GetCursorOnCardPlane(Cursor))
			{
				DraggedVisual = FindVisualUnderCursor(Cursor);
				if (ASHCard* Visual = DraggedVisual.Get())
				{
					LastRequestedDropIndex = VisualCards.IndexOfByKey(Visual);
					DragCursorOffset = Visual->GetRootComponent()->GetRelativeLocation() - Cursor;
					Visual->SetActorRelativeScale3D(FVector(CardScale * HoverScale));
				}
			}
		}
	}
	return true;
}

TArray<ASHCard*> ASHHandRevealPawn::GetPresentationCards() const
{
	TArray<ASHCard*> Result;
	for (ASHCard* Visual : VisualCards) { if (IsValid(Visual)) { Result.Add(Visual); } }
	for (ASHCard* Visual : ReceivingVisualCards) { if (IsValid(Visual)) { Result.Add(Visual); } }
	return Result;
}

TArray<ASHCard*> ASHHandRevealPawn::GetReceivingPresentationCards() const
{
	TArray<ASHCard*> Result;
	for (ASHCard* Visual : ReceivingVisualCards) { if (IsValid(Visual)) { Result.Add(Visual); } }
	return Result;
}

void ASHHandRevealPawn::CalcCamera(float DeltaTime, FMinimalViewInfo& OutResult)
{
	if (bHasPreservedView) { OutResult = PreservedView; }
	else { RevealCamera->GetCameraView(DeltaTime, OutResult); }
}

void ASHHandRevealPawn::ClearPresentation()
{
	for (ASHCard* Visual : VisualCards) { if (IsValid(Visual)) { Visual->Destroy(); } }
	for (ASHCard* Visual : ReceivingVisualCards) { if (IsValid(Visual)) { Visual->Destroy(); } }
	VisualCards.Reset();
	RevealedCards.Reset();
	ReceivingVisualCards.Reset();
	ReceivingCards.Reset();
	RestingTransforms.Reset();
	VisualBounds.Reset();
	HoveredVisual.Reset();
	DraggedVisual.Reset();
	PresentationController = nullptr;
	RevealSessionId.Invalidate();
	bReorderingAllowed = false;
	bFinishingAllowed = false;
	bComparisonMode = false;
	bTransferAllowed = false;
	RemainingTransfers = 0;
	bHasPreservedView = false;
	PreservedView = FMinimalViewInfo();
	LastRequestedDropIndex = INDEX_NONE;
	SetActorTickEnabled(false);
}

void ASHHandRevealPawn::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	ClearPresentation();
	Super::EndPlay(EndPlayReason);
}
