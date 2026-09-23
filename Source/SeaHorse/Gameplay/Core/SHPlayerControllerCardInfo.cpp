#include "Gameplay/Core/SHPlayerController.h"
#include "Gameplay/Cards/SHCard.h"
#include "Gameplay/Cards/CardDefinition.h"
#include "Gameplay/Presentation/CardInfoWidget.h"

bool ASHPlayerController::CanInspectCard(const ASHCard* Card) const
{
	return IsValid(Card) && Card->GetInspectableDefinition(this) != nullptr;
}

ASHCard* ASHPlayerController::GetInspectedCard() const
{
	ASHCard* Card = InspectedCard.Get();
	return IsValid(ActiveCardInfoWidget) && CanInspectCard(Card) ? Card : nullptr;
}

bool ASHPlayerController::ShowCardInfo(ASHCard* Card)
{
	if (!CanInspectCard(Card) || ActiveReactionOfferId != INDEX_NONE || IsValid(LocallyDraggedCard) ||
		!GetWorld()->GetGameViewport() || !CardInfoWidgetClass || CardInfoWidgetClass->HasAnyClassFlags(CLASS_Abstract))
	{
		return false;
	}
	CloseCardInfo();
	InspectedCard = Card;
	ActiveCardInfoWidget = CreateWidget<UCardInfoWidget>(this, CardInfoWidgetClass);
	if (!ActiveCardInfoWidget)
	{
		InspectedCard.Reset();
		return false;
	}
	// Context is available during Construct and the explicit Blueprint event.
	UCardInfoWidget* Widget = ActiveCardInfoWidget;
	Widget->AddToViewport(150);
	if (ActiveCardInfoWidget == Widget && GetInspectedCard()) { Widget->OnCardInfoOpened(); }
	return ActiveCardInfoWidget == Widget && GetInspectedCard() != nullptr;
}

void ASHPlayerController::CloseCardInfo()
{
	UCardInfoWidget* Widget = ActiveCardInfoWidget;
	// Invalidate before Destruct or any Blueprint callback can use the old context.
	ActiveCardInfoWidget = nullptr;
	InspectedCard.Reset();
	if (IsValid(Widget)) { Widget->RemoveFromParent(); }
}

void ASHPlayerController::ValidateCardInfoAccess()
{
	if (ActiveCardInfoWidget && (!GetInspectedCard() || !ActiveCardInfoWidget->IsInViewport()))
	{
		CloseCardInfo();
	}
}
