#include "SeaHorse/Gameplay/Components/CardsLayoutComponent.h"

#include "Components/SceneComponent.h"
#include "Components/SplineComponent.h"
#include "Kismet/KismetMathLibrary.h"
#include "SeaHorse/Gameplay/Cards/SHCard.h"
#include "SeaHorse/Gameplay/Core/SHPlayerController.h"
#include "SeaHorse/Gameplay/Core/SHPlayerState.h"
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

	ASHCard* PreviewCard = nullptr;
	int32 PreviewInsertIndex = INDEX_NONE;
	const bool bShowDropPreview = GetExternalCardDropPreview(PreviewCard, PreviewInsertIndex);
	const bool bOwnCardReorder = bShowDropPreview && IsValid(PreviewCard) &&
		PreviewCard->GetOwningHand() == OwningHand->GetRepresentedHand();

	CardsTransforms.Reset();
	CardsTransforms_WithNoOffsets.Reset();
	int32 CompactIndex = 0;
	for (int32 Index = 0; Index < Cards.Num(); ++Index)
	{
		if (IsValid(Cards[Index]) && Cards[Index] != DraggedCard)
		{
			const int32 SourceIndex = bOwnCardReorder ? CompactIndex : Index;
			const int32 LayoutIndex = bShowDropPreview && SourceIndex >= PreviewInsertIndex
				? SourceIndex + 1 : SourceIndex;
			const int32 LayoutCount = Cards.Num() + (bShowDropPreview && !bOwnCardReorder ? 1 : 0);
			UpdateSingleCardPosition(Cards[Index], LayoutIndex, LayoutCount);
			++CompactIndex;
		}
	}
}

void USHHandCardsLayoutComponent::MoveCardsToDesiredPositions(float DeltaTime)
{
	for (const TPair<TObjectPtr<ASHCard>, FTransform>& Entry : CardsTransforms)
	{
		ASHCard* Card = Entry.Key;
		if (CanLayoutHandCard(Card) && Card != DraggedCard && !IsLocallyDraggedCard(Card))
		{
			Card->SetActorTransform(UKismetMathLibrary::TInterpTo(
				Card->GetActorTransform(), Entry.Value, DeltaTime, 5.0f));
		}
	}
}

bool USHHandCardsLayoutComponent::CanLayoutHandCard(const ASHCard* Card) const
{
	if (!IsValid(Card) || Card->GetCardZone() != ECardZone::Hand || !IsValid(OwningHand))
	{
		return false;
	}
	const ASHHand* RepresentedHand = OwningHand->GetRepresentedHand();
	return IsValid(RepresentedHand) && Card->GetOwningHand() == RepresentedHand;
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
	ASHCard* PreviousCard = DraggedCard;
	DraggedCard = Card;
	if (ASHPlayerController* PC = GetWorld() ? Cast<ASHPlayerController>(GetWorld()->GetFirstPlayerController()) : nullptr;
		IsValid(PC) && PC->IsLocalController())
	{
		if (IsValid(Card))
		{
			PC->BeginLocalCardDrag(Card);
		}
		else
		{
			PC->EndLocalCardDrag(PreviousCard);
		}
	}
}

double USHHandCardsLayoutComponent::GetDistanceToLayout(const FVector& WorldLocation) const
{
	if (!IsValid(OwnerSpline))
	{
		return TNumericLimits<double>::Max();
	}
	return FVector::Dist(WorldLocation,
		OwnerSpline->FindLocationClosestToWorldLocation(WorldLocation, ESplineCoordinateSpace::World));
}

bool USHHandCardsLayoutComponent::GetExternalCardDropPreview(ASHCard*& OutDraggedCard, int32& OutInsertIndex)
{
	OutDraggedCard = nullptr;
	OutInsertIndex = INDEX_NONE;
	if (!IsValid(OwningHand) || OwningHand->IsNPC() || !IsValid(OwnerSpline))
	{
		PreviewTrackingCard = nullptr;
		StablePreviewInsertIndex = INDEX_NONE;
		return false;
	}

	ASHPlayerController* PC = GetWorld() ? Cast<ASHPlayerController>(GetWorld()->GetFirstPlayerController()) : nullptr;
	ASHPlayerState* LocalPS = IsValid(PC) ? PC->GetPlayerState<ASHPlayerState>() : nullptr;
	if (!IsValid(PC) || !PC->IsLocalController() || OwningHand->GetRepresentedPlayerState() != LocalPS)
	{
		return false;
	}

	ASHCard* Card = PC->GetLocallyDraggedCard();
	ASHHand* TargetLogicalHand = OwningHand->GetRepresentedHand();
	if (!IsValid(Card) || !IsValid(TargetLogicalHand) || Card->GetCardZone() != ECardZone::Hand)
	{
		PreviewTrackingCard = nullptr;
		StablePreviewInsertIndex = INDEX_NONE;
		return false;
	}
	const bool bOwnCardReorder = Card->GetOwningHand() == TargetLogicalHand;
	if (bOwnCardReorder && !TargetLogicalHand->HasSeaHorseCard())
	{
		PreviewTrackingCard = nullptr;
		StablePreviewInsertIndex = INDEX_NONE;
		return false;
	}

	OutDraggedCard = Card;
	const int32 CurrentCardCount = TargetLogicalHand->GetCardCount() - (bOwnCardReorder ? 1 : 0);
	const int32 CandidateIndex = CalculateDropInsertIndex(Card, CurrentCardCount);
	if (PreviewTrackingCard != Card || StablePreviewInsertIndex < 0 ||
		StablePreviewInsertIndex > CurrentCardCount)
	{
		PreviewTrackingCard = Card;
		StablePreviewInsertIndex = CandidateIndex;
	}
	else if (CandidateIndex != StablePreviewInsertIndex)
	{
		const double InputKey = OwnerSpline->FindInputKeyClosestToWorldLocation(Card->GetActorLocation());
		const double DragDistance = OwnerSpline->GetDistanceAlongSplineAtSplineInputKey(InputKey);
		const double StableDistance = CalculateCardDistanceAtSpline(CurrentCardCount + 1, StablePreviewInsertIndex);
		const double CandidateDistance = CalculateCardDistanceAtSpline(CurrentCardCount + 1, CandidateIndex);
		if (FMath::Abs(DragDistance - CandidateDistance) + DropPreviewSwitchHysteresis <
			FMath::Abs(DragDistance - StableDistance))
		{
			StablePreviewInsertIndex = CandidateIndex;
		}
	}
	OutInsertIndex = StablePreviewInsertIndex;
	PC->UpdateLocalCardDropPreview(Card, OutInsertIndex, bOwnCardReorder);
	return OutInsertIndex != INDEX_NONE;
}

bool USHHandCardsLayoutComponent::IsLocallyDraggedCard(const ASHCard* Card) const
{
	const ASHPlayerController* PC = GetWorld()
		? Cast<ASHPlayerController>(GetWorld()->GetFirstPlayerController()) : nullptr;
	return IsValid(PC) && PC->IsLocalController() && PC->GetLocallyDraggedCard() == Card;
}

int32 USHHandCardsLayoutComponent::CalculateDropInsertIndex(const ASHCard* PreviewCard, int32 CurrentCardCount) const
{
	if (!IsValid(PreviewCard) || !IsValid(OwnerSpline))
	{
		return INDEX_NONE;
	}

	const double InputKey = OwnerSpline->FindInputKeyClosestToWorldLocation(PreviewCard->GetActorLocation());
	const double DragDistance = OwnerSpline->GetDistanceAlongSplineAtSplineInputKey(InputKey);
	int32 BestIndex = 0;
	double BestDistance = TNumericLimits<double>::Max();
	for (int32 CandidateIndex = 0; CandidateIndex <= CurrentCardCount; ++CandidateIndex)
	{
		const double CandidateDistance = CalculateCardDistanceAtSpline(CurrentCardCount + 1, CandidateIndex);
		const double Difference = FMath::Abs(CandidateDistance - DragDistance);
		if (Difference < BestDistance)
		{
			BestDistance = Difference;
			BestIndex = CandidateIndex;
		}
	}
	return BestIndex;
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
		Location.Z += Index * CardDepthSpacing;
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
		if (!IsPairValid(Pair) || !IsValid(OwningHand) || OwningHand->IsPairMovementBlocked())
		{
			continue;
		}
		const ASHHand* RepresentedHand = OwningHand->GetRepresentedHand();
		if (!IsValid(RepresentedHand) || Pair.CardA->GetOwningHand() != RepresentedHand ||
			Pair.CardB->GetOwningHand() != RepresentedHand ||
			Pair.CardA->GetCardZone() != ECardZone::Activation ||
			Pair.CardB->GetCardZone() != ECardZone::Activation)
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

		if (FVector::Dist(Pair.CardA->GetActorLocation(), CardATarget.GetLocation()) <= 5.0 &&
			FVector::Dist(Pair.CardB->GetActorLocation(), CardBTarget.GetLocation()) <= 5.0)
		{
			// Finish interpolation at an exact transform so presentation code has a
			// deterministic moment at which card movement is complete on every client.
			Pair.CardA->SetActorTransform(CardATarget);
			Pair.CardB->SetActorTransform(CardBTarget);
			SetCardActivatable(Pair.CardA, true);
			SetCardActivatable(Pair.CardB, true);
			OwningHand->NotifyPairSettled(Pair.CardA, Pair.CardB);
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
			Property->GetAuthoredName() == TEXT("IsActivatable?")))
		{
			Property->SetPropertyValue_InContainer(Card, bActivatable);
			return;
		}
	}
}
