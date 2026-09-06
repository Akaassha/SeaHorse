// Fill out your copyright notice in the Description page of Project Settings.


#include "Frontend/Widgets/Options/DataObjects/ListDataObjectKeyRemap.h"
#include "CommonInputBaseTypes.h"
#include "CommonInputSubsystem.h"

#include "Frontend/Debug/FrontendDebugHelper.h"

void UListDataObjectKeyRemap::InitKeyRemapData(UEnhancedInputUserSettings* InOwningInputUserSettings, UEnhancedPlayerMappableKeyProfile* InKeyProfile, ECommonInputType InDesiredInputKeyType, const FPlayerKeyMapping& InOwningPlayerKeyMapping)
{
	CachedOwningInputUserSettings = InOwningInputUserSettings;
	CachedOwningKeyProfile = InKeyProfile;
	CachedDesiredInputKeyType = InDesiredInputKeyType;
	CachedOwningMappingName = InOwningPlayerKeyMapping.GetMappingName();
	CachedOwningMappableKeySlot = InOwningPlayerKeyMapping.GetSlot();
}

FSlateBrush UListDataObjectKeyRemap::GetIconFromCurrentKey() const
{
	check(CachedOwningInputUserSettings);

	FSlateBrush FoundBrush;

	UCommonInputSubsystem* CommonInputSubsystem = UCommonInputSubsystem::Get(CachedOwningInputUserSettings->GetLocalPlayer());

	check(CommonInputSubsystem);

	const bool bHasFoundBrush = UCommonInputPlatformSettings::Get()->TryGetInputBrush(
		FoundBrush,
		GetOwningKeyMapping()->GetCurrentKey(),
		CachedDesiredInputKeyType,
		CommonInputSubsystem->GetCurrentGamepadName()
	);

	if (!bHasFoundBrush)
	{
		Debug::Print(
			TEXT("Unable to find an icon for the key") +
			GetOwningKeyMapping()->GetCurrentKey().GetDisplayName().ToString() +
			TEXT(" Emply Brush was applied.")
		);
	}

	return FoundBrush;
}

void UListDataObjectKeyRemap::BindNewInputKey(const FKey& InNewKey)
{
	check(CachedOwningInputUserSettings);

	FMapPlayerKeyArgs KeyArgs;
	KeyArgs.ProfileIdString = CachedOwningKeyProfile->GetProfileIdString();
	KeyArgs.MappingName = CachedOwningMappingName;
	KeyArgs.Slot = CachedOwningMappableKeySlot;
	KeyArgs.NewKey = InNewKey;

	FGameplayTagContainer Container;

	CachedOwningInputUserSettings->MapPlayerKey(KeyArgs, Container);
	if (!Container.IsEmpty()) { return; }
	CachedOwningInputUserSettings->SaveSettings();

	NotifyListDataModified(this);
}

bool UListDataObjectKeyRemap::HasDefauldValue() const
{
	return GetOwningKeyMapping()->GetDefaultKey().IsValid();
}

bool UListDataObjectKeyRemap::CanResetBackToDefaultValue() const
{
	return HasDefauldValue() && GetOwningKeyMapping()->IsCustomized();
}

bool UListDataObjectKeyRemap::TryResetBackToDefaultValue()
{
	if (CanResetBackToDefaultValue())
	{
		check(CachedOwningInputUserSettings);

		FMapPlayerKeyArgs KeyArgs;
		KeyArgs.ProfileIdString = CachedOwningKeyProfile->GetProfileIdString();
		KeyArgs.MappingName = CachedOwningMappingName;
		KeyArgs.Slot = CachedOwningMappableKeySlot;
		KeyArgs.NewKey = GetOwningKeyMapping()->GetDefaultKey();
		FGameplayTagContainer Failures;
		CachedOwningInputUserSettings->MapPlayerKey(KeyArgs, Failures);
		if (!Failures.IsEmpty()) { return false; }

		CachedOwningInputUserSettings->SaveSettings();

		NotifyListDataModified(this, EOptionsListDataModifyReason::ResetToDefault);

		return true;
	}

	return false;
}

FPlayerKeyMapping* UListDataObjectKeyRemap::GetOwningKeyMapping() const
{
	check(CachedOwningKeyProfile);

	FMapPlayerKeyArgs KeyArgs;
	KeyArgs.ProfileIdString = CachedOwningKeyProfile->GetProfileIdString();
	KeyArgs.MappingName = CachedOwningMappingName;
	KeyArgs.Slot = CachedOwningMappableKeySlot;

	return CachedOwningKeyProfile->FindKeyMapping(KeyArgs);
}
