// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Frontend/Widgets/WidgetActivatableBase.h"
#include "CommonInputTypeEnum.h"
#include "WidgetKeyRemapScreen.generated.h"

class UCommonRichTextBlock;
class FKeyRemapScreenInputPreprocessor;
/**
 * 
 */
UCLASS(Abstract, BlueprintType, meta = (DisableNaiveTick))
class SEAHORSE_API UWidgetKeyRemapScreen : public UWidgetActivatableBase
{
	GENERATED_BODY()

public:
	void SetDesiredInputTypeToFilter(ECommonInputType InDesiredInputType);

	DECLARE_DELEGATE_OneParam(FOnKeyRemapScreenKeyPressedDelegate, const FKey& PressedKey)
	FOnKeyRemapScreenKeyPressedDelegate OnKeyRemapScreenKeyPressed;

	DECLARE_DELEGATE_OneParam(FOnKeyRemapScreenKeyCanceledDelegate, const FString&)
	FOnKeyRemapScreenKeyCanceledDelegate OnKeyRemapScreenKeyCanceled;

protected:
	//~Begin UWidgetActivatableBase Interface
	virtual void NativeOnActivated() override;
	virtual void NativeOnDeactivated() override;
	//~End UWidgetActivatableBase Interface

private:
	void OnValidKeyPressedDetected(const FKey& PressedKey);
	void OnKeySelectedCanceled(const FString& CanceledReason);

	void RequestDeactivateWidget(TFunction<void()> PreDeactivateCallback);

	//***** Bound Widgets *****//
	UPROPERTY(meta = (BindWidget))
	UCommonRichTextBlock* CommonRichText_RemapMessage;
	//***** Bound Widgets *****//

	TSharedPtr<FKeyRemapScreenInputPreprocessor> CachedInputPreprocessor;

	ECommonInputType CachedDesiredInputType;
};
