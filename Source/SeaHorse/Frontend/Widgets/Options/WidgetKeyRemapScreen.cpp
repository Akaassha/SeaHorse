// Fill out your copyright notice in the Description page of Project Settings.


#include "Frontend/Widgets/Options/WidgetKeyRemapScreen.h"
#include "CommonRichTextBlock.h"
#include "CommonInputSubsystem.h"
#include "Framework/Application/IInputProcessor.h"
#include "ICommonInputModule.h"
#include "CommonUITypes.h"


class FKeyRemapScreenInputPreprocessor : public IInputProcessor
{
public:
	FKeyRemapScreenInputPreprocessor(ECommonInputType InInputTypeToListenTo, ULocalPlayer* InOwningLocalPlayer)
		: CachedInputTypeToListenTo(InInputTypeToListenTo)
		, CachedWeakOwningLocalPlayer(InOwningLocalPlayer)
	{

	}

	DECLARE_DELEGATE_OneParam(FOnInputPreProcessorKeyPressedDelegate, const FKey& PressedKey)
	FOnInputPreProcessorKeyPressedDelegate OnInputPreProcessorKeyPressed;

	DECLARE_DELEGATE_OneParam(FOnInputPreProcessorKeySelectCanceledDelegate, const FString& CanceledReason)
	FOnInputPreProcessorKeySelectCanceledDelegate OnInputPreProcessorKeySelectedCanceled;

protected:
	virtual void Tick(const float DeltaTime, FSlateApplication& SlateApp, TSharedRef<ICursor> Cursor) override 
	{ };

	virtual bool HandleKeyDownEvent(FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent) override
	{
		//UEnum* StaticCommonInputType = StaticEnum<ECommonInputType>();
		//
		//StaticCommonInputType->GetValueAsString(CachedInputTypeToListenTo);

		ProcessPressedKey(InKeyEvent.GetKey());

		return false;
	}

	virtual bool HandleMouseButtonDownEvent(FSlateApplication& SlateApp, const FPointerEvent& MouseEvent) override
	{
		ProcessPressedKey(MouseEvent.GetEffectingButton());

		return false;
	}

	void ProcessPressedKey(const FKey& InPressedKey)
	{
		if (InPressedKey == EKeys::Escape)
		{
			OnInputPreProcessorKeySelectedCanceled.ExecuteIfBound(TEXT("Key Remap has been canceled"));

			return;
		}

		UCommonInputSubsystem* CommonInputSubsystem = UCommonInputSubsystem::Get(CachedWeakOwningLocalPlayer.Get());

		check(CommonInputSubsystem);

		ECommonInputType CurrentInputType = CommonInputSubsystem->GetCurrentInputType();

		switch (CachedInputTypeToListenTo)
		{
		case ECommonInputType::MouseAndKeyboard:

			if (InPressedKey.IsGamepadKey() || CurrentInputType == ECommonInputType::Gamepad)
			{
				OnInputPreProcessorKeySelectedCanceled.ExecuteIfBound(TEXT("Detected Gamepad key pressed for keyboard inputs. Key Remap has been canceled."));

				return;
			}

			break;

		case ECommonInputType::Gamepad:

			if (CurrentInputType == ECommonInputType::Gamepad && InPressedKey == EKeys::LeftMouseButton)
			{
				FCommonInputActionDataBase* InputActionData = ICommonInputModule::GetSettings().GetDefaultClickAction().GetRow<FCommonInputActionDataBase>(TEXT(""));

				check(InputActionData);

				OnInputPreProcessorKeyPressed.ExecuteIfBound(InputActionData->GetDefaultGamepadInputTypeInfo().GetKey());

				return;
			}

			if (!InPressedKey.IsGamepadKey())
			{
				OnInputPreProcessorKeySelectedCanceled.ExecuteIfBound(TEXT("Detected non Gamepad key pressed for Gamepad inputs. Key Remap has been canceled."));

				return;
			}

			break;
		case ECommonInputType::Touch:
			break;
		case ECommonInputType::Count:
			break;
		default:
			break;
		}

		OnInputPreProcessorKeyPressed.ExecuteIfBound(InPressedKey);
	}

private:
	ECommonInputType  CachedInputTypeToListenTo;
	TWeakObjectPtr<ULocalPlayer> CachedWeakOwningLocalPlayer;
};

void UWidgetKeyRemapScreen::SetDesiredInputTypeToFilter(ECommonInputType InDesiredInputType)
{
	CachedDesiredInputType = InDesiredInputType;
}

void UWidgetKeyRemapScreen::NativeOnActivated()
{
	Super::NativeOnActivated();

	CachedInputPreprocessor = MakeShared<FKeyRemapScreenInputPreprocessor>(CachedDesiredInputType, GetOwningLocalPlayer());
	CachedInputPreprocessor->OnInputPreProcessorKeyPressed.BindUObject(this, &ThisClass::OnValidKeyPressedDetected);
	CachedInputPreprocessor->OnInputPreProcessorKeySelectedCanceled.BindUObject(this, &ThisClass::OnKeySelectedCanceled);

	FSlateApplication::Get().RegisterInputPreProcessor(CachedInputPreprocessor, -1);

	FString InputDevideName;

	switch (CachedDesiredInputType)
	{
	case ECommonInputType::MouseAndKeyboard:
		InputDevideName = TEXT("Mouse & Keyboard");

		break;
	case ECommonInputType::Gamepad:
		InputDevideName = TEXT("Gamepad");

		break;
	case ECommonInputType::Touch:
		break;
	case ECommonInputType::Count:
		break;
	default:
		break;
	}

	const FString DisplayRichMessage = FString::Printf(
		TEXT("<KeyRemapDefault>Press any</> <KeyRemapHighlight>%s</> <KeyRemapDefault>key.</>"), *InputDevideName
		);
	CommonRichText_RemapMessage->SetText(FText::FromString(DisplayRichMessage));
}

void UWidgetKeyRemapScreen::NativeOnDeactivated()
{
	Super::NativeOnDeactivated();

	if (CachedInputPreprocessor)
	{
		FSlateApplication::Get().UnregisterInputPreProcessor(CachedInputPreprocessor);

		CachedInputPreprocessor.Reset();
	}
}

void UWidgetKeyRemapScreen::OnValidKeyPressedDetected(const FKey& PressedKey)
{
	RequestDeactivateWidget(
		[this, PressedKey]() {
			OnKeyRemapScreenKeyPressed.ExecuteIfBound(PressedKey);
		}
	);
}

void UWidgetKeyRemapScreen::OnKeySelectedCanceled(const FString& CanceledReason)
{
	RequestDeactivateWidget(
		[this, CanceledReason]() {
			OnKeyRemapScreenKeyCanceled.ExecuteIfBound(CanceledReason);
		}
	);
}

void UWidgetKeyRemapScreen::RequestDeactivateWidget(TFunction<void()> PreDeactivateCallback)
{
	FTSTicker::GetCoreTicker().AddTicker(
		FTickerDelegate::CreateLambda(
			[PreDeactivateCallback, this](float DeltaTime)->bool {
				PreDeactivateCallback();

				DeactivateWidget();

				return false;
			}
		)
	);
}
