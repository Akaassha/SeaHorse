#include "Gameplay/Core/SHPlayerController.h"
#include "Gameplay/Cards/SHCard.h"
#include "Gameplay/SHHand.h"
#include "Gameplay/Components/CardsLayoutComponent.h"
#include "Gameplay/Presentation/CardInfoWidget.h"
#include "Components/PrimitiveComponent.h"

namespace
{
bool IsHoverableHandCard(const ASHCard* Card)
{
	return IsValid(Card) && !Card->IsActorBeingDestroyed() && !Card->IsHidden() &&
		Card->GetCardZone() == ECardZone::Hand && IsValid(Card->GetOwningHand()) &&
		!Card->GetOwningHand()->IsLogicalNPC();
}

// Test the card's real collision geometry in its resting pose, without moving
// the actor, adding collision proxies, or changing replicated gameplay state.
bool TraceRestingCard(ASHCard* Card, const FTransform& Resting, const FVector& Start, const FVector& End, FHitResult& OutHit)
{
	const FTransform Current = Card->GetActorTransform();
	const FVector MappedStart = Current.TransformPosition(Resting.InverseTransformPosition(Start));
	const FVector MappedEnd = Current.TransformPosition(Resting.InverseTransformPosition(End));
	bool bFound = false;
	TInlineComponentArray<UPrimitiveComponent*> Components(Card);
	for (UPrimitiveComponent* Component : Components)
	{
		if (!Component->IsQueryCollisionEnabled() || Component->GetCollisionResponseToChannel(ECC_Visibility) != ECR_Block) { continue; }
		FHitResult Hit;
		if (!Component->LineTraceComponent(Hit, MappedStart, MappedEnd, FCollisionQueryParams(NAME_None, true))) { continue; }
		// Component traces report geometry touches, not channel-blocking hits.
		// We already checked that this component blocks Visibility; preserve that
		// contract when the public cursor query consumes the virtual resting hit.
		Hit.bBlockingHit = true;
		Hit.Location = Resting.TransformPosition(Current.InverseTransformPosition(Hit.Location));
		Hit.ImpactPoint = Resting.TransformPosition(Current.InverseTransformPosition(Hit.ImpactPoint));
		Hit.Distance = FVector::Distance(Start, Hit.ImpactPoint);
		Hit.TraceStart = Start; Hit.TraceEnd = End;
		if (!bFound || Hit.Distance < OutHit.Distance) { OutHit = Hit; bFound = true; }
	}
	return bFound;
}

// Stretch the hover area across both endpoint poses in the card's own axes.
// Endpoints stay fixed while it animates; no physics component is resized.
// This volume is used only to retain an existing hover, never to acquire one.
bool TraceHoverCorridor(ASHCard* Card, const FTransform& Resting, const FTransform& Focused,
	const FVector& Start, const FVector& End, FHitResult& OutHit)
{
	const FVector LocalStart = Resting.InverseTransformPosition(Start);
	const FVector LocalEnd = Resting.InverseTransformPosition(End);
	const FTransform FocusedToResting = Focused.GetRelativeTransform(Resting);
	bool bFound = false;
	TInlineComponentArray<UPrimitiveComponent*> Components(Card);
	for (UPrimitiveComponent* Component : Components)
	{
		if (!Component->IsQueryCollisionEnabled() || Component->GetCollisionResponseToChannel(ECC_Visibility) != ECR_Block) { continue; }
		const FTransform ComponentToCard = Component->GetComponentTransform().GetRelativeTransform(Card->GetActorTransform());
		FBox Corridor = Component->CalcBounds(ComponentToCard).GetBox();
		Corridor += Component->CalcBounds(ComponentToCard * FocusedToResting).GetBox();
		FVector LocalHit, LocalNormal;
		float Time = 0.0f;
		if (!FMath::LineExtentBoxIntersection(Corridor, LocalStart, LocalEnd, FVector::ZeroVector, LocalHit, LocalNormal, Time)) { continue; }
		const FVector Point = Resting.TransformPosition(LocalHit);
		const double Distance = FVector::Distance(Start, Point);
		if (!bFound || Distance < OutHit.Distance)
		{
			OutHit = FHitResult(Card, Component, Point, Resting.TransformVectorNoScale(LocalNormal).GetSafeNormal());
			OutHit.bBlockingHit = true;
			OutHit.Time = Time;
			OutHit.Distance = Distance;
			OutHit.TraceStart = Start; OutHit.TraceEnd = End;
			bFound = true;
		}
	}
	return bFound;
}
}

void ASHPlayerController::ResetHandCursorHover()
{
	HandCursorHoverCard.Reset();
	HandCursorHoverOwner.Reset();
	bHasHandCursorHoverFocusedTransform = false;
}

FVector ASHPlayerController::GetCardDragCursorLocation() const
{
	FVector Start, Direction;
	if (DeprojectMousePositionToWorld(Start, Direction) && !FMath::IsNearlyZero(Direction.Z))
	{
		const double Distance = (PointerPressedLocation.Z - Start.Z) / Direction.Z;
		if (Distance >= 0.0) { return Start + Direction * Distance; }
	}
	return PointerPressedLocation;
}

bool ASHPlayerController::GetCardInteractionUnderCursor(ASHCard*& Card, FVector& Location)
{
	Card = nullptr; Location = FVector::ZeroVector;
	float X = 0.0f, Y = 0.0f;
	FVector Start, Direction;
	if (!IsLocalController() || !bShowMouseCursor || !GetMousePosition(X, Y) ||
		!DeprojectScreenPositionToWorld(X, Y, Start, Direction) ||
		(ActiveCardInfoWidget && ActiveCardInfoWidget->IsHovered()))
	{
		ResetHandCursorHover();
		return false;
	}
	const FVector End = Start + Direction * HitResultTraceDistance;
	FHitResult ActualHit, ResolvedHit;
	GetWorld()->LineTraceSingleByChannel(ActualHit, Start, End, ECC_Visibility, FCollisionQueryParams(NAME_None, true));
	ResolveCardCursorHit(ActualHit, Start, End, ResolvedHit);
	Card = ResolvedHit.bBlockingHit ? Cast<ASHCard>(ResolvedHit.GetActor()) : nullptr;
	Location = ResolvedHit.Location;
	return IsValid(Card);
}

ASHCard* ASHPlayerController::GetHandCardUnderCursor()
{
	ASHCard* Card = nullptr;
	FVector Location;
	GetCardInteractionUnderCursor(Card, Location);
	return IsHoverableHandCard(Card) && !bCardHoverSuppressedForTargeting &&
		!IsValid(LocallyDraggedCard) && ActiveReactionOfferId == INDEX_NONE ? Card : nullptr;
}

void ASHPlayerController::ResolveCardCursorHit(const FHitResult& Hit, const FVector& RayStart, const FVector& RayEnd, FHitResult& OutHit)
{
	OutHit = Hit;
	if (!IsLocalController() || bCardHoverSuppressedForTargeting ||
		IsValid(LocallyDraggedCard) || ActiveReactionOfferId != INDEX_NONE)
	{
		ResetHandCursorHover();
		return;
	}
	// A returning card can cross an otherwise empty cursor position and start
	// lifting again. Normalize its animated surface before both hover retention
	// and acquisition, including when there is no previously hovered card.
	FHitResult StableHit = Hit;
	FHitResult NearestRestingHit;
	FCollisionQueryParams Query(NAME_None, true);
	while (ASHCard* MovingCard = Cast<ASHCard>(StableHit.GetActor()))
	{
		if (!IsHoverableHandCard(MovingCard)) { break; }
		ASHHand* VisualHand = FindVisualHandForLogicalHand(MovingCard->GetOwningHand());
		auto* Layout = VisualHand ? VisualHand->FindComponentByClass<USHHandCardsLayoutComponent>() : nullptr;
		FTransform Base;
		if (!Layout || !Layout->GetHoverReturnTransform(MovingCard, Base)) { break; }
		FHitResult RestingHit;
		if (TraceRestingCard(MovingCard, Base, RayStart, RayEnd, RestingHit) &&
			(!NearestRestingHit.bBlockingHit || RestingHit.Distance < NearestRestingHit.Distance))
		{
			NearestRestingHit = RestingHit;
		}
		// Keep cards and other blockers behind the transient animation clickable.
		Query.AddIgnoredActor(MovingCard);
		GetWorld()->LineTraceSingleByChannel(StableHit, RayStart, RayEnd, ECC_Visibility, Query);
	}
	if (NearestRestingHit.bBlockingHit && (!StableHit.bBlockingHit || NearestRestingHit.Distance < StableHit.Distance))
	{
		StableHit = NearestRestingHit;
	}
	OutHit = StableHit;
	ASHCard* Previous = HandCursorHoverCard.Get();
	auto CacheLayoutPoses = [this](ASHCard* HoveredCard)
	{
		ASHHand* VisualHand = FindVisualHandForLogicalHand(HoveredCard->GetOwningHand());
		auto* Layout = VisualHand ? VisualHand->FindComponentByClass<USHHandCardsLayoutComponent>() : nullptr;
		if (Layout && Layout->GetUnfocusedCardTransform(HoveredCard, HandCursorHoverBaseTransform))
		{
			HandCursorHoverFocusedTransform = Layout->MakeFocusedCardTransform(HandCursorHoverBaseTransform);
			bHasHandCursorHoverFocusedTransform = true;
		}
	};
	// The first cursor query may precede layout initialization. Retry only until
	// the endpoints are available, then keep them fixed for this entire hover.
	if (!bHasHandCursorHoverFocusedTransform && IsHoverableHandCard(Previous) &&
		Previous->GetOwningHand() == HandCursorHoverOwner.Get())
	{
		CacheLayoutPoses(Previous);
	}
	const bool bPreviousValid = IsHoverableHandCard(Previous) && Previous->GetOwningHand() == HandCursorHoverOwner.Get();
	FHitResult RestingHit;
	const bool bInRestingFootprint = bPreviousValid &&
		TraceRestingCard(Previous, HandCursorHoverBaseTransform, RayStart, RayEnd, RestingHit);
	if (bInRestingFootprint)
	{
		const bool bOccluded = StableHit.bBlockingHit && StableHit.GetActor() != Previous &&
			StableHit.Distance < RestingHit.Distance - KINDA_SMALL_NUMBER;
		if (!bOccluded)
		{
			OutHit = StableHit.GetActor() == Previous ? StableHit : RestingHit;
			return;
		}
	}
	// The gap below the lifted card still belongs to its hover. A visible other
	// card in this extra area takes priority, so neighboring cards remain usable.
	const ASHCard* OtherCard = Cast<ASHCard>(StableHit.GetActor());
	FHitResult CorridorHit;
	const bool bInCorridor = bPreviousValid && bHasHandCursorHoverFocusedTransform && (!OtherCard || OtherCard == Previous) &&
		TraceHoverCorridor(Previous, HandCursorHoverBaseTransform, HandCursorHoverFocusedTransform, RayStart, RayEnd, CorridorHit);
	if (bInCorridor)
	{
		const bool bOccluded = StableHit.bBlockingHit && StableHit.GetActor() != Previous &&
			StableHit.Distance < CorridorHit.Distance - KINDA_SMALL_NUMBER;
		if (!bOccluded)
		{
			OutHit = StableHit.GetActor() == Previous ? StableHit : CorridorHit;
			return;
		}
	}
	ASHCard* Card = StableHit.bBlockingHit ? Cast<ASHCard>(StableHit.GetActor()) : nullptr;
	if (!IsHoverableHandCard(Card)) { Card = nullptr; }
	if (Card != Previous || (Card && Card->GetOwningHand() != HandCursorHoverOwner.Get()))
	{
		bHasHandCursorHoverFocusedTransform = false;
		if (Card)
		{
			HandCursorHoverBaseTransform = Card->GetActorTransform();
			CacheLayoutPoses(Card);
		}
	}
	HandCursorHoverCard = Card;
	HandCursorHoverOwner = Card ? Card->GetOwningHand() : nullptr;
}
