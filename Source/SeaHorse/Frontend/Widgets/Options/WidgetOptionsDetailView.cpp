// Fill out your copyright notice in the Description page of Project Settings.


#include "Frontend/Widgets/Options/WidgetOptionsDetailView.h"
#include "CommonTextBlock.h"
#include "CommonLazyImage.h"
#include "CommonRichTextBlock.h"
#include "Frontend/Widgets/Options/DataObjects/ListDataObjectBase.h"

void UWidgetOptionsDetailView::UpdateDetailsViewInfo(UListDataObjectBase* InDataObject, const FString& InEntryWidgetClassName)
{
	if (!InDataObject)
	{
		return;
	}

	CommonTextBlock_Title->SetText(InDataObject->GetDataDisplayName());

	if (!InDataObject->GetSoftDescriptionImage().IsNull())
	{
		CommonLazyImage_DescriptionImage->SetBrushFromLazyTexture(InDataObject->GetSoftDescriptionImage());
		CommonLazyImage_DescriptionImage->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	}
	else
	{
		CommonLazyImage_DescriptionImage->SetVisibility(ESlateVisibility::Collapsed);
	}

	CommonRichText_Description->SetText(InDataObject->GetDescriptionRichText());

	const FString DynamiDetails = FString::Printf(
		TEXT("Data Object Class: <Bold>%s</>\n\nEntry WidgetClass:<Bold>%s</>"),
		*InDataObject->GetClass()->GetName(),
		*InEntryWidgetClassName
	);

	CommonRichText_DynamicDetails->SetText(FText::FromString(DynamiDetails));
	CommonRichText_DisabledReason->SetText(InDataObject->IsDataCurrentlyEditable() ? FText::GetEmpty() : InDataObject->GetDisabledRichText());
}

void UWidgetOptionsDetailView::ClearDetailsViewInfo()
{
	CommonTextBlock_Title->SetText(FText::GetEmpty());
	CommonLazyImage_DescriptionImage->SetVisibility(ESlateVisibility::Collapsed);
	CommonRichText_Description->SetText(FText::GetEmpty());
	CommonRichText_DynamicDetails->SetText(FText::GetEmpty());
	CommonRichText_DisabledReason->SetText(FText::GetEmpty());
}

void UWidgetOptionsDetailView::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	ClearDetailsViewInfo();
}
