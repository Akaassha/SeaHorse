// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "WidgetOptionsDetailView.generated.h"

class UCommonTextBlock;
class UCommonLazyImage;
class UCommonRichTextBlock;
class UListDataObjectBase;
/**
 * 
 */
UCLASS(Abstract, BlueprintType, meta = (DisableNaiveTick))
class SEAHORSE_API UWidgetOptionsDetailView : public UUserWidget
{
	GENERATED_BODY()
	
public:
	void UpdateDetailsViewInfo(UListDataObjectBase* InDataObject, const FString& InEntryWidgetClassName = FString());
	void ClearDetailsViewInfo();

protected:
	//~ Begin UUserWidget Interface
	virtual void NativeOnInitialized() override;
	//~ Begin UUserWidget Interface

private:
	//***** Bound Widget *****//
	UPROPERTY(meta = (BindWidget))
	UCommonTextBlock* CommonTextBlock_Title;

	UPROPERTY(meta = (BindWidget))
	UCommonLazyImage* CommonLazyImage_DescriptionImage;

	UPROPERTY(meta = (BindWidget))
	UCommonRichTextBlock* CommonRichText_Description;

	UPROPERTY(meta = (BindWidget))
	UCommonRichTextBlock* CommonRichText_DynamicDetails;

	UPROPERTY(meta = (BindWidget))
	UCommonRichTextBlock* CommonRichText_DisabledReason;
	//***** Bound Widget *****//
};
