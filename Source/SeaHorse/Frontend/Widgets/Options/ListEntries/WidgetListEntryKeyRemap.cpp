// Fill out your copyright notice in the Description page of Project Settings.


#include "Frontend/Widgets/Options/ListEntries/WidgetListEntryKeyRemap.h"
#include "Frontend/Widgets/Options/DataObjects/ListDataObjectKeyRemap.h"
#include "Frontend/Widgets/Components/FronendCommonButtonBase.h"
#include "Frontend/FrontendSubsystem.h"
#include "Frontend/FrontendGameplayTags.h"
#include "Frontend/FrontendFunctionLibrary.h"
#include "Frontend/Widgets/Options/WidgetKeyRemapScreen.h"

void UWidgetListEntryKeyRemap::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	CommonButton_RemapKey->OnClicked().AddUObject(this, &ThisClass::OnRemapKeyButtonClicked);
	CommonButton_ResetKeyBinding->OnClicked().AddUObject(this, &ThisClass::OnResetKeyBindingButtonClicked);
}

void UWidgetListEntryKeyRemap::OnOwningListDataObjectSet(UListDataObjectBase* InOwninigListDataObject)
{
	Super::OnOwningListDataObjectSet(InOwninigListDataObject);

	CachedOwningKeyRemapDataObject = CastChecked<UListDataObjectKeyRemap>(InOwninigListDataObject);

	CommonButton_RemapKey->SetButtonDisplayImagel(CachedOwningKeyRemapDataObject->GetIconFromCurrentKey());
}

void UWidgetListEntryKeyRemap::OnOwningListDataObjectModified(UListDataObjectBase* OwningModifiedData, EOptionsListDataModifyReason ModifyReason)
{
	if (CachedOwningKeyRemapDataObject)
	{
		CommonButton_RemapKey->SetButtonDisplayImagel(CachedOwningKeyRemapDataObject->GetIconFromCurrentKey());
	}
	
}

void UWidgetListEntryKeyRemap::OnRemapKeyButtonClicked()
{
	SelectThisEntryWidget();

	UFrontendSubsystem::UFrontendSubsystem::Get(this)->PushSoftWidgetToStackAsync(
		FrontendGameplayTags::Frontend_WidgetStack_Modal,
		UFrontendFunctionLibrary::GetFrontendSoftWidgetClassByTag(FrontendGameplayTags::Frontend_Widget_KeyRemapScreen),
		[this](EAsyncPushWdgetState PushState, UWidgetActivatableBase* PushedWidget) 
		{
			if (PushState == EAsyncPushWdgetState::OnCreatedBeforePush)
			{
				UWidgetKeyRemapScreen* CreatedKeyRemapScreen = CastChecked<UWidgetKeyRemapScreen>(PushedWidget);
				CreatedKeyRemapScreen->OnKeyRemapScreenKeyPressed.BindUObject(this, &ThisClass::OnKeyToRemapPressed);
				CreatedKeyRemapScreen->OnKeyRemapScreenKeyCanceled.BindUObject(this, &ThisClass::OnKeyRemapCanceled);

				if (CachedOwningKeyRemapDataObject)
				{
					CreatedKeyRemapScreen->SetDesiredInputTypeToFilter(CachedOwningKeyRemapDataObject->GetDesiredInputKeyType());
				}
				
			}
		}
	);
}

void UWidgetListEntryKeyRemap::OnResetKeyBindingButtonClicked()
{
	SelectThisEntryWidget();

	if (!CachedOwningKeyRemapDataObject)
	{
		return;
	}

	if (!CachedOwningKeyRemapDataObject->CanResetBackToDefaultValue())
	{
		UFrontendSubsystem::Get(this)->PushConfirmScreenToModalStackAsync(
			EConfirmScreenType::Ok,
			FText::FromString(TEXT("Reset Key Mapping")),
			FText::FromString(TEXT("The key binding for ") + CachedOwningKeyRemapDataObject->GetDataDisplayName().ToString() + TEXT(" is already set to default")),
			[](EConfirmScreenButtonType ClickedButton) {

			}
		);

		return;
	};

	UFrontendSubsystem::Get(this)->PushConfirmScreenToModalStackAsync(
		EConfirmScreenType::YesNo,
		FText::FromString(TEXT("Reset Key Mapping")),
		FText::FromString(TEXT("Are you sure you wat to reset the key binding for ") + CachedOwningKeyRemapDataObject->GetDataDisplayName().ToString() + TEXT("?")),
		[this](EConfirmScreenButtonType ClickedButton) {

			if (ClickedButton == EConfirmScreenButtonType::Confirmed)
			{
				CachedOwningKeyRemapDataObject->TryResetBackToDefaultValue();
			}
		}
	);

}

void UWidgetListEntryKeyRemap::OnKeyToRemapPressed(const FKey& PressedKey)
{
	if (CachedOwningKeyRemapDataObject)
	{
		CachedOwningKeyRemapDataObject->BindNewInputKey(PressedKey);
	}
}

void UWidgetListEntryKeyRemap::OnKeyRemapCanceled(const FString& CanceledReason)
{
	UFrontendSubsystem::Get(this)->PushConfirmScreenToModalStackAsync(
		EConfirmScreenType::Ok,
		FText::FromString(TEXT("Key Remap")),
		FText::FromString(CanceledReason),
		[](EConfirmScreenButtonType ClickedButton) {

		}
	);
}
