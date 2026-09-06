// Fill out your copyright notice in the Description page of Project Settings.


#include "Frontend/Widgets/Components/FronendCommonButtonBase.h"
#include "Frontend/FrontendSubsystem.h"
#include "CommonTextBlock.h"
#include "CommonLazyImage.h"

void UFrontendCommonButtonBase::SetButtonText(FText InText)
{
	if (CommonTextBlock_ButtonText && !InText.IsEmpty())
	{
		CommonTextBlock_ButtonText->SetText(bUserUpperCaseForButtonText ? InText.ToUpper() : InText);
	}
}

FText UFrontendCommonButtonBase::GetButtonDisplayText() const
{
	if (CommonTextBlock_ButtonText)
	{
		return CommonTextBlock_ButtonText->GetText();
	}

	return FText();
}

void UFrontendCommonButtonBase::SetButtonDisplayImagel(const FSlateBrush& InBrush)
{
	if (CommonLazyImage_ButtonImage)
	{
		CommonLazyImage_ButtonImage->SetBrush(InBrush);
	}
}

void UFrontendCommonButtonBase::NativePreConstruct()
{
	Super::NativePreConstruct();

	SetButtonText(ButtonDisplayText);
}

void UFrontendCommonButtonBase::NativeOnCurrentTextStyleChanged()
{
	Super::NativeOnCurrentTextStyleChanged();

	if (CommonTextBlock_ButtonText && GetCurrentTextStyleClass())
	{
		CommonTextBlock_ButtonText->SetStyle(GetCurrentTextStyleClass());
	}
}

void UFrontendCommonButtonBase::NativeOnHovered()
{
	Super::NativeOnHovered();

	if (!ButtonDescriptionText.IsEmpty())
	{
		UFrontendSubsystem::Get(this)->OnButtonDescriptionTextUpdatedDelegate.Broadcast(this, ButtonDescriptionText);
	}
}

void UFrontendCommonButtonBase::NativeOnUnhovered()
{
	Super::NativeOnUnhovered();

	UFrontendSubsystem::Get(this)->OnButtonDescriptionTextUpdatedDelegate.Broadcast(this, FText::GetEmpty());
}

//void UFrontendCommonButtonBase::NativeOnSelected(bool bBroadcast)
//{
//	Super::NativeOnSelected(bBroadcast);
//	if (!ButtonDescriptionText.IsEmpty())
//	{
//		UFrontendSubsystem::Get(this)->OnButtonDescriptionTextUpdatedDelegate.Broadcast(this, ButtonDescriptionText);
//	}
//}
//
//void UFrontendCommonButtonBase::NativeOnDeselected(bool bBroadcast)
//{
//	Super::NativeOnDeselected(bBroadcast);
//	UFrontendSubsystem::Get(this)->OnButtonDescriptionTextUpdatedDelegate.Broadcast(this, FText::GetEmpty());
//}