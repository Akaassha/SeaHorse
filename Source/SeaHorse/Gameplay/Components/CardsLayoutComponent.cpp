#include "SeaHorse/Gameplay/Components/CardsLayoutComponent.h"

#include "Components/SceneComponent.h"
#include "Components/SplineComponent.h"
#include "Kismet/KismetMathLibrary.h"
#include "SeaHorse/Gameplay/Cards/SHCard.h"
#include "UObject/UnrealType.h"

USHCardsLayoutComponent::USHCardsLayoutComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = true;
}

void USHCardsLayoutComponent::BeginPlay()
{
	Super::BeginPlay();
	OwningHand = Cast<ASHHand>(GetOwner());
}

void USHCardsLayoutComponent::TickComponent(float DeltaTime, ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	MoveCardsToDesiredPositions(DeltaTime);
}

void USHCardsLayoutComponent::Initialize(const TArray<ASHCard*>& Cards, USplineComponent* Spline,
	USceneComponent* TableCenterDirection)
{
	OwnerSpline = Spline;
	TableCenterDirectionComponent = TableCenterDirection;
	bInitialized = IsValid(OwnerSpline);
	UpdateCardsPositions(Cards);

	for (const TPair<TObjectPtr<ASHCard>, FTransform>& Entry : CardsTransforms)
	{
		if (IsValid(Entry.Key))
		{
			Entry.Key->SetActorTransform(Entry.Value);
		}
	}
}

void USHCardsLayoutComponent::UpdateCardsPositions(const TArray<ASHCard*>& Cards)
{
}

void USHCardsLayoutComponent::MoveCardsToDesiredPositions(float DeltaTime)
{
}

void USHCardsLayoutComponent::UpdateSingleCardPosition(ASHCard* Card, int32 Index, int32 CardsAmount)
{
}

double USHCardsLayoutComponent::CalculateCardDistanceAtSpline(int32 CardsAmount, int32 ArrayIndex) const
{
	if (!IsValid(OwnerSpline))
	{
		return 0.0;
	}

	const double SplineLength = OwnerSpline->GetSplineLength();
	if (CardsAmount <= 1)
	{
		return SplineLength * 0.5;
	}

	const double ActualSpacing = FMath::Min(SplineLength / (CardsAmount - 1), PreferredCardSpacing);
	return ((SplineLength - ActualSpacing * (CardsAmount - 1)) * 0.5) + ActualSpacing * ArrayIndex;
}

void USHHandCardsLayoutComponent::UpdateCardsPositions(const TArray<ASHCard*>& Cards)
{
	if (!bInitialized || !IsValid(OwnerSpline))
	{
		return;
	}

	CardsTransforms.Reset();
	CardsTransforms_WithNoOffsets.Reset();
	for (int32 Index = 0; Index < Cards.Num(); ++Index)
	{
		if (IsValid(Cards[Index]) && Cards[Index] != DraggedCard)
		{
			UpdateSingleCardPosition(Cards[Index], Index, Cards.Num());
		}
	}
}

void USHHandCardsLayoutComponent::MoveCardsToDesiredPositions(float DeltaTime)
{
	for (const TPair<TObjectPtr<ASHCard>, FTransform>& Entry : CardsTransforms)
	{
		ASHCard* Card = Entry.Key;
		if (IsValid(Card) && Card != DraggedCard)
		{
			Card->SetActorTransform(UKismetMathLibrary::TInterpTo(
				Card->GetActorTransform(), Entry.Value, DeltaTime, 5.0f));
		}
	}
}

void USHHandCardsLayoutComponent::UpdateSingleCardPosition(ASHCard* Card, int32 Index, int32 CardsAmount)
{
	if (!IsValid(Card) || !IsValid(OwnerSpline))
	{
		return;
	}

	const double Distance = CalculateCardDistanceAtSpline(CardsAmount, Index) + CalculateFocusOffset(Index);
	FVector Location = OwnerSpline->GetLocationAtDistanceAlongSpline(Distance, ESplineCoordinateSpace::World);
	FRotator Rotation = OwnerSpline->GetRotationAtDistanceAlongSpline(Distance, ESplineCoordinateSpace::World);
	Location.Z += Index * CardDepthSpacing;
	Rotation.Yaw += 90.0;
	const FTransform BaseTransform(Rotation, Location, FVector::OneVector);
	CardsTransforms_WithNoOffsets.Add(Card, BaseTransform);

	if (Index == FocusedCardIndexValue || Index == SelectedCardIndexValue)
	{
		if (IsValid(TableCenterDirectionComponent))
		{
			Location += TableCenterDirectionComponent->GetForwardVector() * ForwardFocusedOffser;
		}
		Location.Z += 2.0;
		CardsTransforms.Add(Card, FTransform(Rotation, Location, FVector(1.3)));
	}
	else
	{
		CardsTransforms.Add(Card, BaseTransform);
	}
}

double USHHandCardsLayoutComponent::CalculateFocusOffset(int32 Index) const
{
	if (FocusedCardIndexValue == INDEX_NONE || Index == FocusedCardIndexValue)
	{
		return 0.0;
	}
	return Index < FocusedCardIndexValue ? -FocusSpreadDistance : FocusSpreadDistance;
}

void USHHandCardsLayoutComponent::SetFocusedCardIndex(int32 FocusedCardIndex)
{
	FocusedCardIndexValue = FocusedCardIndex;
}

void USHHandCardsLayoutComponent::SetSelectedCardIndex(int32 SelectedCardIndex)
{
	SelectedCardIndexValue = SelectedCardIndex;
}

void USHHandCardsLayoutComponent::RemoveCardFromLayout(ASHCard* Card)
{
	CardsTransforms_WithNoOffsets.Remove(Card);
	CardsTransforms.Remove(Card);
}

void USHHandCardsLayoutComponent::SetDreggedCard(ASHCard* Card)
{
	DraggedCard = Card;
}

void USHHandCardsLayoutComponent::UpdateNPCCardsPositons(const TArray<ASHCard*>& Cards)
{
	if (!bInitialized || !IsValid(OwnerSpline))
	{
		return;
	}

	CardsTransforms.Reset();
	CardsTransforms_WithNoOffsets.Reset();
	const double Distance = OwnerSpline->GetSplineLength() * 0.5;
	const FVector SplineLocation = OwnerSpline->GetLocationAtDistanceAlongSpline(Distance, ESplineCoordinateSpace::World);
	const FTransform OwnerTransform = GetOwner() ? GetOwner()->GetActorTransform() : FTransform::Identity;
	for (int32 Index = 0; Index < Cards.Num(); ++Index)
	{
		ASHCard* Card = Cards[Index];
		if (!IsValid(Card) || Card == DraggedCard)
		{
			continue;
		}
		FVector Location = SplineLocation;
		Location.Z += Index;
		FRotator Rotation = OwnerTransform.Rotator();
		Rotation.Yaw += 90.0;
		const FTransform Target(Rotation, Location, OwnerTransform.GetScale3D());
		CardsTransforms.Add(Card, Target);
		CardsTransforms_WithNoOffsets.Add(Card, Target);
	}
}

void USHActivatableCardsLayoutComponent::UpdateCardsPositions(const TArray<ASHCard*>& Cards)
{
	Pairs.SetNum(Cards.Num() / 2);
	for (int32 Index = 0; Index < Pairs.Num(); ++Index)
	{
		Pairs[Index] = FActivatedPair{Cards[Index * 2], Cards[Index * 2 + 1], false};
	}

	PairsTransform.Reset();
	for (int32 Index = 0; Index < Pairs.Num(); ++Index)
	{
		UpdatePairPositions(Pairs[Index], Index, Pairs.Num());
	}
}

void USHActivatableCardsLayoutComponent::MoveCardsToDesiredPositions(float DeltaTime)
{
	MovePairsToDesiredPositions(DeltaTime);
}

void USHActivatableCardsLayoutComponent::UpdatePairPositions(const FActivatedPair& Pair, int32 Index, int32 CardsAmount)
{
	if (!IsPairValid(Pair) || !IsValid(OwnerSpline))
	{
		return;
	}
	const double Distance = CalculateCardDistanceAtSpline(CardsAmount, Index);
	FVector Location = OwnerSpline->GetLocationAtDistanceAlongSpline(Distance, ESplineCoordinateSpace::World);
	FRotator Rotation = OwnerSpline->GetRotationAtDistanceAlongSpline(Distance, ESplineCoordinateSpace::World);
	Location.Z += Index * CardDepthSpacing;
	Rotation.Yaw += 90.0;
	PairsTransform.Add(Pair, FTransform(Rotation, Location, FVector::OneVector));
}

void USHActivatableCardsLayoutComponent::MovePairsToDesiredPositions(float DeltaTime)
{
	for (const TPair<FActivatedPair, FTransform>& Entry : PairsTransform)
	{
		const FActivatedPair& Pair = Entry.Key;
		if (!IsPairValid(Pair))
		{
			continue;
		}

		FTransform CardATarget = Entry.Value;
		CardATarget.AddToTranslation(FVector(0.0, 0.0, 2.0));
		FTransform CardBTarget = Entry.Value;
		FRotator CardBRotation = CardBTarget.Rotator();
		CardBRotation.Yaw += 8.0;
		CardBTarget.SetRotation(CardBRotation.Quaternion());
		Pair.CardA->SetActorTransform(UKismetMathLibrary::TInterpTo(
			Pair.CardA->GetActorTransform(), CardATarget, DeltaTime, 5.0f));
		Pair.CardB->SetActorTransform(UKismetMathLibrary::TInterpTo(
			Pair.CardB->GetActorTransform(), CardBTarget, DeltaTime, 5.0f));

		if (FVector::Dist(Pair.CardA->GetActorLocation(), Entry.Value.GetLocation()) <= 5.0 &&
			FVector::Dist(Pair.CardB->GetActorLocation(), Entry.Value.GetLocation()) <= 5.0)
		{
			SetCardActivatable(Pair.CardA, true);
			SetCardActivatable(Pair.CardB, true);
		}
	}
}

bool USHActivatableCardsLayoutComponent::IsPairValid(const FActivatedPair& Pair) const
{
	return IsValid(Pair.CardA) && IsValid(Pair.CardB);
}

void USHActivatableCardsLayoutComponent::SetCardActivatable(ASHCard* Card, bool bActivatable) const
{
	if (!IsValid(Card))
	{
		return;
	}
	for (TFieldIterator<FBoolProperty> It(Card->GetClass(), EFieldIterationFlags::IncludeSuper); It; ++It)
	{
		FBoolProperty* Property = *It;
		if (Property && (Property->GetName() == TEXT("IsActivatable?") ||
			Property->GetDisplayNameText().ToString() == TEXT("IsActivatable?")))
		{
			Property->SetPropertyValue_InContainer(Card, bActivatable);
			return;
		}
	}
}
