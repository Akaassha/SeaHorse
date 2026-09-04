#include "SeaHorse/Gameplay/Presentation/PairTargetingIndicator.h"

#include "Components/SceneComponent.h"
#include "Components/SplineComponent.h"
#include "Components/SplineMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"

APairTargetingIndicator::APairTargetingIndicator()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = false;
	SetReplicateMovement(false);

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(SceneRoot);
	Spline = CreateDefaultSubobject<USplineComponent>(TEXT("Spline"));
	Spline->SetupAttachment(SceneRoot);
	Spline->SetClosedLoop(false);
	ArrowHead = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ArrowHead"));
	ArrowHead->SetupAttachment(SceneRoot);
	ArrowHead->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

void APairTargetingIndicator::InitializeIndicator(
	const FPairTargetingIndicatorStyle& InStyle, FName InEffectPresentationId)
{
	Style = InStyle;
	CurrentEffectPresentationId = InEffectPresentationId;
	ArrowHead->SetStaticMesh(Style.ArrowHeadMesh);
	ArrowHead->SetWorldScale3D(Style.ArrowHeadScale);
	RebuildSegments();
	ApplyVisualState();
	OnIndicatorInitialized(CurrentEffectPresentationId);
}

void APairTargetingIndicator::RebuildSegments()
{
	for (USplineMeshComponent* Segment : Segments)
	{
		if (IsValid(Segment))
		{
			Segment->DestroyComponent();
		}
	}
	Segments.Reset();

	const int32 Count = FMath::Clamp(Style.SegmentCount, 2, 32);
	for (int32 Index = 0; Index < Count; ++Index)
	{
		USplineMeshComponent* Segment = NewObject<USplineMeshComponent>(this);
		Segment->SetupAttachment(SceneRoot);
		Segment->SetMobility(EComponentMobility::Movable);
		Segment->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Segment->SetStaticMesh(Style.BodyMesh);
		Segment->SetStartScale(Style.BodyScale);
		Segment->SetEndScale(Style.BodyScale);
		Segment->RegisterComponent();
		Segments.Add(Segment);
	}
}

void APairTargetingIndicator::UpdateIndicator(const FVector& Start, const FVector& End, bool bInValidTarget)
{
	const FVector RaisedStart = Start + FVector::UpVector * Style.StartHeightOffset;
	const FVector RaisedEnd = End + FVector::UpVector * Style.TargetClearance;
	const FVector LocalStart = GetActorTransform().InverseTransformPosition(RaisedStart);
	const FVector LocalEnd = GetActorTransform().InverseTransformPosition(RaisedEnd);
	const int32 PointCount = Segments.Num() + 1;
	Spline->ClearSplinePoints(false);
	for (int32 PointIndex = 0; PointIndex < PointCount; ++PointIndex)
	{
		const float Alpha = static_cast<float>(PointIndex) / static_cast<float>(PointCount - 1);
		FVector Point = FMath::Lerp(LocalStart, LocalEnd, Alpha);
		Point.Z += 4.0f * Style.ArcHeight * Alpha * (1.0f - Alpha);
		Spline->AddSplinePoint(Point, ESplineCoordinateSpace::Local, false);
	}
	Spline->UpdateSpline();

	for (int32 Index = 0; Index < Segments.Num(); ++Index)
	{
		FVector SegmentStart;
		FVector StartTangent;
		FVector SegmentEnd;
		FVector EndTangent;
		Spline->GetLocationAndTangentAtSplinePoint(Index, SegmentStart, StartTangent, ESplineCoordinateSpace::Local);
		Spline->GetLocationAndTangentAtSplinePoint(Index + 1, SegmentEnd, EndTangent, ESplineCoordinateSpace::Local);
		Segments[Index]->SetStartAndEnd(SegmentStart, StartTangent, SegmentEnd, EndTangent);
	}

	const FVector EndTangent = Spline->GetTangentAtSplinePoint(PointCount - 1, ESplineCoordinateSpace::World);
	ArrowHead->SetWorldLocation(RaisedEnd);
	if (!EndTangent.IsNearlyZero())
	{
		ArrowHead->SetWorldRotation(EndTangent.Rotation());
	}

	if (!bHasVisualState || bCurrentTargetValid != bInValidTarget)
	{
		bCurrentTargetValid = bInValidTarget;
		bHasVisualState = true;
		ApplyVisualState();
		OnTargetValidityChanged(bCurrentTargetValid);
	}
	OnIndicatorUpdated(RaisedStart, RaisedEnd, bCurrentTargetValid);
}

void APairTargetingIndicator::ApplyVisualState()
{
	DynamicMaterials.Reset();
	UMaterialInterface* Material = bCurrentTargetValid ? Style.ValidMaterial : Style.InvalidMaterial;
	if (!IsValid(Material))
	{
		Material = bCurrentTargetValid ? Style.InvalidMaterial : Style.ValidMaterial;
	}

	auto ApplyToAllSlots = [this, Material](UMeshComponent* MeshComponent)
	{
		if (!IsValid(MeshComponent) || !IsValid(Material))
		{
			return;
		}
		const int32 MaterialCount = FMath::Max(1, MeshComponent->GetNumMaterials());
		for (int32 MaterialIndex = 0; MaterialIndex < MaterialCount; ++MaterialIndex)
		{
			// Assign the source material first. The visual remains correct even if
			// this mesh/slot cannot create a dynamic material instance.
			MeshComponent->SetMaterial(MaterialIndex, Material);
			if (UMaterialInstanceDynamic* DynamicMaterial =
				MeshComponent->CreateDynamicMaterialInstance(MaterialIndex, Material))
			{
				DynamicMaterials.Add(DynamicMaterial);
			}
			else
			{
				UE_LOG(LogTemp, Warning,
					TEXT("[SH_TARGETING_INDICATOR] Failed to create MID for %s material slot %d; using %s directly"),
					*GetNameSafe(MeshComponent), MaterialIndex, *GetNameSafe(Material));
			}
		}
	};

	for (USplineMeshComponent* Segment : Segments)
	{
		ApplyToAllSlots(Segment);
	}
	ApplyToAllSlots(ArrowHead);
	ApplyMaterialParameters(Style.Intensity);
}

void APairTargetingIndicator::ApplyMaterialParameters(float DisplayIntensity)
{
	const FLinearColor Color = bCurrentTargetValid ? Style.ValidColor : Style.InvalidColor;
	for (UMaterialInstanceDynamic* Material : DynamicMaterials)
	{
		if (IsValid(Material))
		{
			Material->SetVectorParameterValue(TEXT("Color"), Color);
			Material->SetScalarParameterValue(TEXT("Intensity"), DisplayIntensity);
		}
	}
}

void APairTargetingIndicator::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	AnimationTime += DeltaSeconds;
	const float Pulse = Style.PulseSpeed > 0.0f
		? FMath::Sin(AnimationTime * Style.PulseSpeed * UE_TWO_PI) * Style.PulseAmount : 0.0f;
	ApplyMaterialParameters(FMath::Max(0.0f, Style.Intensity + Pulse));
}
