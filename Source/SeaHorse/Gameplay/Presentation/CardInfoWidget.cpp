#include "Gameplay/Presentation/CardInfoWidget.h"
#include "Gameplay/Core/SHPlayerController.h"
#include "Gameplay/Cards/SHCard.h"
#include "Gameplay/Cards/CardDefinition.h"
#include "Input/Reply.h"
#include "InputCoreTypes.h"
#include "Engine/TextureRenderTarget2D.h"

ASHPlayerController* UCardInfoWidget::GetInspectionController() const
{
	ASHPlayerController* PC = GetOwningPlayer<ASHPlayerController>();
	return IsValid(PC) && PC->ActiveCardInfoWidget == this ? PC : nullptr;
}

ASHCard* UCardInfoWidget::GetInspectedCard() const
{
	const ASHPlayerController* PC = GetInspectionController();
	return PC ? PC->GetInspectedCard() : nullptr;
}

TSubclassOf<UCardDefinition> UCardInfoWidget::GetCardDefinitionClass() const
{
	const ASHCard* Card = GetInspectedCard();
	return Card ? Card->GetInspectableDefinition(GetInspectionController()) : nullptr;
}

const UCardDefinition* UCardInfoWidget::GetCardDefinition() const
{
	const TSubclassOf<UCardDefinition> Definition = GetCardDefinitionClass();
	return Definition ? Definition.GetDefaultObject() : nullptr;
}

UTextureRenderTarget2D* UCardInfoWidget::GetCardFaceRenderTarget() const
{
	const ASHCard* Card = GetInspectedCard();
	return Card ? Card->CardFaceRenderTarget.Get() : nullptr;
}

FSlateBrush UCardInfoWidget::GetCardFaceBrush() const
{
	FSlateBrush Brush;
	if (UTextureRenderTarget2D* Texture = GetCardFaceRenderTarget())
	{
		Brush.SetResourceObject(Texture);
		Brush.ImageSize = FVector2D(Texture->SizeX, Texture->SizeY);
		Brush.DrawAs = ESlateBrushDrawType::Image;
	}
	else
	{
		Brush.DrawAs = ESlateBrushDrawType::NoDrawType;
	}
	return Brush;
}

void UCardInfoWidget::SetClickOutsideTargets(UWidget* CardImage, UWidget* CloseButton)
{
	ClickOutsideCardImage = CardImage;
	ClickOutsideCloseButton = CloseButton;
}

FReply UCardInfoWidget::NativeOnPreviewMouseButtonDown(const FGeometry& Geometry, const FPointerEvent& Event)
{
	if (Event.GetEffectingButton() == EKeys::LeftMouseButton && GetInspectionController() && IsValid(ClickOutsideCardImage))
	{
		const FVector2D Position = Event.GetScreenSpacePosition();
		auto Contains = [&Position](const UWidget* Widget)
		{
			return IsValid(Widget) && Widget->IsVisible() && Widget->GetCachedGeometry().IsUnderLocation(Position);
		};
		if (Contains(ClickOutsideCardImage) && !Contains(ClickOutsideCloseButton))
		{
			// The image is the only non-button surface that consumes a press.
			// Leave the close button's normal mouse handling intact.
			return FReply::Handled();
		}
	}
	return Super::NativeOnPreviewMouseButtonDown(Geometry, Event);
}

void UCardInfoWidget::CloseCardInfo()
{
	if (ASHPlayerController* PC = GetInspectionController()) { PC->CloseCardInfo(); }
}

FReply UCardInfoWidget::NativeOnPreviewKeyDown(const FGeometry& Geometry, const FKeyEvent& Event)
{
	if (Event.GetKey() == EKeys::Escape)
	{
		CloseCardInfo();
		return FReply::Handled();
	}
	return Super::NativeOnPreviewKeyDown(Geometry, Event);
}

FReply UCardInfoWidget::NativeOnMouseButtonDown(const FGeometry& Geometry, const FPointerEvent& Event)
{
	if (Event.GetEffectingButton() == EKeys::RightMouseButton)
	{
		CloseCardInfo();
		return FReply::Handled();
	}
	return Super::NativeOnMouseButtonDown(Geometry, Event);
}
