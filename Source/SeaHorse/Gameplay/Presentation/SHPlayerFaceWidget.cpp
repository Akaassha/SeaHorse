#include "Gameplay/Presentation/SHPlayerFaceWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Image.h"
#include "Components/ScaleBox.h"
#include "Components/SizeBox.h"
#include "Components/SizeBoxSlot.h"
#include "Components/TextBlock.h"
#include "Gameplay/Player/SHPlayerRepresentation.h"

TSharedRef<SWidget> USHPlayerFaceWidget::RebuildWidget()
{
	if (!WidgetTree)
	{
		Initialize();
	}
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		BuildDefaultLayout();
	}
	return Super::RebuildWidget();
}

void USHPlayerFaceWidget::BuildDefaultLayout()
{
	// A fixed design canvas scales with render resolution without changing font/layout proportions.
	auto* Scale = WidgetTree->ConstructWidget<UScaleBox>(UScaleBox::StaticClass(), TEXT("FaceScale"));
	Scale->SetStretch(EStretch::ScaleToFit);
	WidgetTree->RootWidget = Scale;
	auto* Size = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("FaceDesignSize"));
	Size->SetWidthOverride(512.f);
	Size->SetHeightOverride(512.f);
	Scale->AddChild(Size);
	auto* Canvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("FaceCanvas"));
	Canvas->SetClipping(EWidgetClipping::ClipToBoundsAlways);
	Size->AddChild(Canvas);

	auto* AvatarScale = WidgetTree->ConstructWidget<UScaleBox>(UScaleBox::StaticClass(), TEXT("AvatarScale"));
	AvatarScale->SetStretch(EStretch::ScaleToFit);
	auto* AvatarSlot = Canvas->AddChildToCanvas(AvatarScale);
	AvatarSlot->SetPosition(FVector2D(96.f, 24.f));
	AvatarSlot->SetSize(FVector2D(320.f, 320.f));
	AvatarImage = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("AvatarImage"));
	AvatarImage->SetDesiredSizeOverride(FVector2D(320.f, 320.f));
	AvatarScale->AddChild(AvatarImage);

	PlayerNameText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("PlayerNameText"));
	FSlateFontInfo Font = PlayerNameText->GetFont();
	Font.Size = 28;
	PlayerNameText->SetFont(Font);
	PlayerNameText->SetColorAndOpacity(FSlateColor(FLinearColor(0.92f, 0.84f, 0.67f)));
	PlayerNameText->SetAutoWrapText(false);
	PlayerNameText->SetJustification(ETextJustify::Left);
	PlayerNameText->SetTextOverflowPolicy(ETextOverflowPolicy::Ellipsis);
	PlayerNameText->SetClipping(EWidgetClipping::ClipToBoundsAlways);
	// Slate 5.7 does not ellipsize centered text. Center the bounded control instead.
	auto* NameBounds = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("NameBounds"));
	auto* NameSlot = Canvas->AddChildToCanvas(NameBounds);
	NameSlot->SetPosition(FVector2D(24.f, 376.f));
	NameSlot->SetSize(FVector2D(464.f, 56.f));
	auto* TextSlot = CastChecked<USizeBoxSlot>(NameBounds->AddChild(PlayerNameText));
	TextSlot->SetHorizontalAlignment(HAlign_Center);
	TextSlot->SetVerticalAlignment(VAlign_Center);
}

ASHPlayerRepresentation* USHPlayerFaceWidget::GetRepresentation() const
{
	return Representation.Get();
}

void USHPlayerFaceWidget::UpdatePresentation(ASHPlayerRepresentation* InRepresentation)
{
	Representation = InRepresentation;
	DisplayName = IsValid(InRepresentation) ? InRepresentation->GetPlayerDisplayName() : FText::GetEmpty();
	AvatarTexture = IsValid(InRepresentation) ? InRepresentation->GetPlayerAvatar() : nullptr;
	bSelectable = IsValid(InRepresentation) && InRepresentation->IsPlayerSelectionEnabled();
	if (PlayerNameText)
	{
		// Designer controls own their formatting. In particular, SetTextOverflowPolicy
		// calls SynchronizeProperties, which reapplies CommonTextBlock's entire style.
		FString SingleLineName = DisplayName.ToString();
		SingleLineName.ReplaceInline(TEXT("\r"), TEXT(" "));
		SingleLineName.ReplaceInline(TEXT("\n"), TEXT(" "));
		SingleLineName.ReplaceInline(TEXT("\t"), TEXT(" "));
		PlayerNameText->SetText(FText::FromString(SingleLineName));
	}
	if (AvatarImage)
	{
		AvatarImage->SetBrushFromTexture(AvatarTexture, false);
		AvatarImage->SetVisibility(AvatarTexture ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden);
	}
	OnPresentationUpdated();
}
