#include "Gameplay/Presentation/CardEffectMessageWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/TextBlock.h"
#include "Styling/CoreStyle.h"

void UCardEffectMessageWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	// A Blueprint Designer tree always wins. The native layout is only a usable default.
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		auto* Canvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("NoticeCanvas"));
		WidgetTree->RootWidget = Canvas;
		MessageText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("MessageText"));
		MessageText->SetFont(FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), 28));
		MessageText->SetColorAndOpacity(FSlateColor(FLinearColor::White));
		MessageText->SetShadowColorAndOpacity(FLinearColor::Black);
		MessageText->SetShadowOffset(FVector2D(1.f, 2.f));
		MessageText->SetJustification(ETextJustify::Center);
		MessageText->SetWrapTextAt(800.f);
		UCanvasPanelSlot* MessageSlot = Canvas->AddChildToCanvas(MessageText);
		MessageSlot->SetAnchors(FAnchors(0.5f, 0.12f));
		MessageSlot->SetAlignment(FVector2D(0.5f, 0.f));
		MessageSlot->SetAutoSize(true);
		Canvas->SetVisibility(ESlateVisibility::HitTestInvisible);
	}
	SetVisibility(ESlateVisibility::HitTestInvisible);
}

void UCardEffectMessageWidget::SetMessage(const FText& InMessage)
{
	Message = InMessage;
	if (MessageText) { MessageText->SetText(Message); }
	OnMessageSet();
}
