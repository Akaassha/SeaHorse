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
	UFrontendSubsystem* Frontend = UFrontendSubsystem::Get(this);
	if (!Frontend || !IsValid(CachedOwningKeyRemapDataObject)) { return; }
	TWeakObjectPtr<UListDataObjectKeyRemap> RequestedData(CachedOwningKeyRemapDataObject);
	TWeakObjectPtr<ThisClass> WeakThis(this);

	Frontend->PushSoftWidgetToStackAsync(
		FrontendGameplayTags::Frontend_WidgetStack_Modal,
		UFrontendFunctionLibrary::GetFrontendSoftWidgetClassByTag(FrontendGameplayTags::Frontend_Widget_KeyRemapScreen),
		[WeakThis, RequestedData](EAsyncPushWdgetState PushState, UWidgetActivatableBase* PushedWidget)
		{
			if (PushState == EAsyncPushWdgetState::OnCreatedBeforePush && RequestedData.IsValid())
			{
				UWidgetKeyRemapScreen* CreatedKeyRemapScreen = Cast<UWidgetKeyRemapScreen>(PushedWidget);
				if (!CreatedKeyRemapScreen) { return; }
				CreatedKeyRemapScreen->OnKeyRemapScreenKeyPressed.BindWeakLambda(RequestedData.Get(),
					[RequestedData](const FKey& Key) { RequestedData->BindNewInputKey(Key); });
				if (WeakThis.IsValid())
				{
					CreatedKeyRemapScreen->OnKeyRemapScreenKeyCanceled.BindUObject(WeakThis.Get(), &ThisClass::OnKeyRemapCanceled);
				}
				CreatedKeyRemapScreen->SetDesiredInputTypeToFilter(RequestedData->GetDesiredInputKeyType());
				
			}
		}
	);
}

void UWidgetListEntryKeyRemap::OnResetKeyBindingButtonClicked()
{
	SelectThisEntryWidget();
	UFrontendSubsystem* Frontend = UFrontendSubsystem::Get(this);

	if (!Frontend || !IsValid(CachedOwningKeyRemapDataObject))
	{
		return;
	}

	if (!CachedOwningKeyRemapDataObject->CanResetBackToDefaultValue())
	{
		Frontend->PushConfirmScreenToModalStackAsync(
			EConfirmScreenType::Ok,
			FText::FromString(TEXT("Reset Key Mapping")),
			FText::FromString(TEXT("The key binding for ") + CachedOwningKeyRemapDataObject->GetDataDisplayName().ToString() + TEXT(" is already set to default")),
			[](EConfirmScreenButtonType ClickedButton) {

			}
		);

		return;
	};

	TWeakObjectPtr<UListDataObjectKeyRemap> RequestedData(CachedOwningKeyRemapDataObject);
	Frontend->PushConfirmScreenToModalStackAsync(
		EConfirmScreenType::YesNo,
		FText::FromString(TEXT("Reset Key Mapping")),
		FText::FromString(TEXT("Are you sure you wat to reset the key binding for ") + CachedOwningKeyRemapDataObject->GetDataDisplayName().ToString() + TEXT("?")),
		[RequestedData](EConfirmScreenButtonType ClickedButton) {

			if (ClickedButton == EConfirmScreenButtonType::Confirmed && RequestedData.IsValid())
			{
				RequestedData->TryResetBackToDefaultValue();
			}
		}
	);

}

void UWidgetListEntryKeyRemap::OnOwningListDataObjectReleased()
{
	CachedOwningKeyRemapDataObject = nullptr;
	Super::OnOwningListDataObjectReleased();
}

void UWidgetListEntryKeyRemap::OnKeyRemapCanceled(const FString& CanceledReason)
{
	UFrontendSubsystem* Frontend = UFrontendSubsystem::Get(this);
	if (!Frontend) { return; }
	Frontend->PushConfirmScreenToModalStackAsync(
		EConfirmScreenType::Ok,
		FText::FromString(TEXT("Key Remap")),
		FText::FromString(CanceledReason),
		[](EConfirmScreenButtonType ClickedButton) {

		}
	);
}
