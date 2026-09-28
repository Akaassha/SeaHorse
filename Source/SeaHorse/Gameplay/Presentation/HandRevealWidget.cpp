#include "Gameplay/Presentation/HandRevealWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/TextBlock.h"
#include "Gameplay/Core/SHPlayerController.h"
#include "Gameplay/Presentation/SHHandRevealPawn.h"
#include "Gameplay/SHHand.h"
#include "Styling/CoreStyle.h"

void UHandRevealWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	// Keep the full Designer layout of any Blueprint subclass.
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		auto* Canvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("RevealCanvas"));
		WidgetTree->RootWidget = Canvas;
		Canvas->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		InformationText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("InformationText"));
		InformationText->SetFont(FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), 24));
		InformationText->SetColorAndOpacity(FSlateColor(FLinearColor::White));
		InformationText->SetShadowColorAndOpacity(FLinearColor::Black);
		InformationText->SetShadowOffset(FVector2D(1.f, 2.f));
		InformationText->SetJustification(ETextJustify::Center);
		InformationText->SetWrapTextAt(850.f);
		InformationText->SetVisibility(ESlateVisibility::HitTestInvisible);
		auto* TextSlot = Canvas->AddChildToCanvas(InformationText);
		TextSlot->SetAnchors(FAnchors(0.5f, 0.08f));
		TextSlot->SetAlignment(FVector2D(0.5f, 0.f));
		TextSlot->SetAutoSize(true);
		FinishButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("FinishButton"));
		auto* Label = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("FinishLabel"));
		Label->SetText(NSLOCTEXT("HandReveal", "Done", "Gotowe"));
		Label->SetFont(FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), 24));
		Label->SetColorAndOpacity(FSlateColor(FLinearColor::Black));
		Label->SetJustification(ETextJustify::Center);
		FinishButton->AddChild(Label);
		auto* ButtonSlot = Canvas->AddChildToCanvas(FinishButton);
		ButtonSlot->SetAnchors(FAnchors(0.5f, 0.9f));
		ButtonSlot->SetAlignment(FVector2D(0.5f, 0.5f));
		ButtonSlot->SetSize(FVector2D(220.f, 55.f));
	}
	SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	if (FinishButton) { FinishButton->OnClicked.AddUniqueDynamic(this, &UHandRevealWidget::FinishViewing); }
}

void UHandRevealWidget::InitializeReveal(ASHHandRevealPawn* Pawn, ASHHand* SourceHand, bool bCanFinish)
{
	RevealPawn = Pawn;
	RevealedHand = SourceHand;
	ComparisonReceivingHand = nullptr;
	bShowFinishButton = bCanFinish;
	RefreshReveal();
}

void UHandRevealWidget::InitializeComparison(ASHHandRevealPawn* Pawn, ASHHand* LargerHand, ASHHand* ReceivingHand)
{
	RevealPawn = Pawn;
	RevealedHand = LargerHand;
	ComparisonReceivingHand = ReceivingHand;
	bShowFinishButton = false;
	RefreshReveal();
}

ASHHandRevealPawn* UHandRevealWidget::GetRevealPawn() const
{
	return IsValid(RevealPawn) ? RevealPawn.Get() : nullptr;
}

ASHHand* UHandRevealWidget::GetSourceHand() const
{
	return IsValid(RevealedHand) ? RevealedHand.Get() : nullptr;
}

ASHHand* UHandRevealWidget::GetReceivingHand() const
{
	return IsValid(ComparisonReceivingHand) ? ComparisonReceivingHand.Get() : nullptr;
}

bool UHandRevealWidget::CanFinishViewing() const
{
	const auto* PC = GetOwningPlayer<ASHPlayerController>();
	return IsValid(PC) && IsValid(RevealPawn) && PC->GetActiveHandRevealPawn() == RevealPawn &&
		bShowFinishButton && RevealPawn->CanFinishViewing();
}

void UHandRevealWidget::RefreshReveal()
{
	if (FinishButton)
	{
		FinishButton->SetVisibility(CanFinishViewing() ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
		FinishButton->SetIsEnabled(CanFinishViewing());
	}
	if (InformationText)
	{
		if (IsValid(RevealPawn) && RevealPawn->IsComparingHands())
		{
			const int32 Remaining = RevealPawn->GetRemainingTransfers();
			InformationText->SetText(RevealPawn->CanTransferComparedCards()
				? FText::Format(NSLOCTEXT("HandReveal", "ComparingDrawer",
					"Przeciągnij jeszcze {0} kart z górnej talii do dolnej."), FText::AsNumber(Remaining))
				: FText::Format(NSLOCTEXT("HandReveal", "ComparingObserver",
					"Gracz z mniejszą talią dobiera jeszcze {0} kart z górnej talii."), FText::AsNumber(Remaining)));
		}
		else
		{
			InformationText->SetText(CanFinishViewing()
				? NSLOCTEXT("HandReveal", "Viewing", "Pokazana ręka. Wybierz Gotowe, aby zakończyć oglądanie.")
				: IsValid(RevealPawn) && RevealPawn->CanReorderCards()
					? NSLOCTEXT("HandReveal", "Reordering", "Pokazujesz swoją rękę. Możesz przeciągać karty, aby zmienić ich kolejność.")
					: NSLOCTEXT("HandReveal", "Showing", "Pokazujesz swoją rękę. Oglądający zakończy podgląd."));
		}
	}
	OnRevealChanged();
}

void UHandRevealWidget::FinishViewing()
{
	if (CanFinishViewing()) { GetOwningPlayer<ASHPlayerController>()->FinishHandReveal(); }
}
