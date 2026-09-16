// Fill out your copyright notice in the Description page of Project Settings.


#include "Frontend/Widgets/Options/OptionsDataRegistry.h"
#include "Frontend/Widgets/Options/DataObjects/ListDataObjectCollection.h"
#include "Frontend/Widgets/Options/DataObjects/MyListDataObjectString.h"
#include "Frontend/Widgets/Options/OptionsDataInteractionHelper.h"
#include "Frontend/FrontendFunctionLibrary.h"
#include "Frontend/Settings/FrontendDeveloperSettings.h"
#include "Frontend/FrontendGameplayTags.h"
#include "Frontend/Widgets/Options/DataObjects/ListDataObjectScalar.h"
#include "Frontend/Widgets/Options/DataObjects/MyListDataObjectStringResolution.h"
#include "GameSettings/SHGameUserSettings.h"
#include "Internationalization/StringTableRegistry.h"
#include "EnhancedInputSubsystems.h"
#include "UserSettings/EnhancedInputUserSettings.h"
#include "Internationalization/Internationalization.h"
#include "Frontend/Widgets/Options/DataObjects/ListDataObjectKeyRemap.h"
#include "Kismet/KismetInternationalizationLibrary.h"

#include "Frontend/Debug/FrontendDebugHelper.h"

#define MAKE_OPTIONS_DATA_CONTROL(SetterOrGetterFuncName) \
	MakeShared<FOptionsDataInteractionHelper>(GET_FUNCTION_NAME_STRING_CHECKED(USHGameUserSettings, SetterOrGetterFuncName))

#define GET_DESCRIPTION(InKey) LOCTABLE("SeaHorseFrontend", InKey)

void UOptionsDataRegistry::InitOptionsDataRegistry(ULocalPlayer* InOwningLocalPlayer)
{
	RegisteredOptionsTabCollections.Reset();
	InitGameplayCollectionTab();
	InitAudioCollectionTab();
	InitVideoCollectionTab();
	InitControllCollectionTab(InOwningLocalPlayer);
	InitGamepadCollectionTab(InOwningLocalPlayer);
	InitAccesibilityCollectionTab();
}

TArray<UListDataObjectBase*> UOptionsDataRegistry::GetListSourceItemsBySelectedTabID(const FName& InSelectedTabID) const
{
	UListDataObjectCollection* const* FoundTabCollectionPtr = RegisteredOptionsTabCollections.FindByPredicate(
		[InSelectedTabID](UListDataObjectCollection* AvailableTabCollection)->bool{
			return AvailableTabCollection->GetDataID() == InSelectedTabID;
		}
	);

	checkf(FoundTabCollectionPtr, TEXT("No valid tab found under the ID %s"), *InSelectedTabID.ToString());

	UListDataObjectCollection* FoundTabCollection = *FoundTabCollectionPtr;

	TArray<UListDataObjectBase*> AllChildListItems;

	for (UListDataObjectBase* ChildListData : FoundTabCollection->GetAllChildListData())
	{
		if (!ChildListData)
		{
			continue;
		}

		AllChildListItems.Add(ChildListData);

		if (ChildListData->HasAnyChildListData())
		{
			FindChildListDataRecusively(ChildListData, AllChildListItems);
		}
	}

	return AllChildListItems;
}

void UOptionsDataRegistry::FindChildListDataRecusively(UListDataObjectBase* InParentData, TArray<UListDataObjectBase*>& OutFoundListData) const
{
	if (!InParentData || !InParentData->HasAnyChildListData())
	{
		return;
	}

	for (UListDataObjectBase* SubChildListData : InParentData->GetAllChildListData())
	{
		if (!SubChildListData)
		{
			continue;
		}

		OutFoundListData.Add(SubChildListData);

		if (SubChildListData->HasAnyChildListData())
		{
			FindChildListDataRecusively(SubChildListData, OutFoundListData);
		}
	}
}

void UOptionsDataRegistry::InitGameplayCollectionTab()
{
	UListDataObjectCollection* GameplayTabCollection = NewObject<UListDataObjectCollection>();
	GameplayTabCollection->SetDataID(FName("GameplayTabCollection"));
	GameplayTabCollection->SetDataDisplayName(GET_DESCRIPTION("_gameplay_settings_tab"));
	
	//Localization
	{
		const bool bIncludeGame = true;
		const bool bIncludeEngine = false;
		const bool bIncludeEditor = false;

		TArray <FString> AvailableCultureNames = UKismetInternationalizationLibrary::GetLocalizedCultures(
			bIncludeGame,
			bIncludeEngine,
			bIncludeEditor
		);

		AvailableCultureNames.AddUnique(USHGameUserSettings::Get()->GetCurrentCulture());
		AvailableCultureNames.AddUnique(TEXT("en"));
		UListDataObjectString* Languages = NewObject<UListDataObjectString>();

		Languages->SetDataID(FName("Languages"));
		Languages->SetDataDisplayName(GET_DESCRIPTION("_language"));
		Languages->SetDescriptionRichText(FText::FromString(TEXT("Choose from the available game languages.")));

		for (const FString& Culture : AvailableCultureNames)

		{
			FString DisplayNameString = UKismetInternationalizationLibrary::GetCultureDisplayName(Culture, false);

			if (!DisplayNameString.IsEmpty())
			{
				DisplayNameString[0] = FChar::ToUpper(DisplayNameString[0]);
			}

			Languages->AddDynamicOption(Culture, FText::FromString(DisplayNameString));
		}
		
		Languages->SetDataDynamicGetter(MAKE_OPTIONS_DATA_CONTROL(GetCurrentCulture));
		Languages->SetDataDynamicSetter(MAKE_OPTIONS_DATA_CONTROL(SetCurrentCulture));
		Languages->SetDefaultValueFromString(TEXT("en"));
		Languages->SetSoftDescriptionImage(UFrontendFunctionLibrary::GetOptionsSoftImageByTag(FrontendGameplayTags::Frontedn_Image_TestImage));

		GameplayTabCollection->AddChildListData(Languages);
	}
	
	RegisteredOptionsTabCollections.Add(GameplayTabCollection);
}

void UOptionsDataRegistry::InitAudioCollectionTab()
{
	UListDataObjectCollection* AudioTabCollection = NewObject<UListDataObjectCollection>();
	AudioTabCollection->SetDataID(FName("AudioTabCollection"));
	AudioTabCollection->SetDataDisplayName(GET_DESCRIPTION("_audio_settings_tab"));

	//Volue Category
	{
		UListDataObjectCollection* VolumeCategoryCollection = NewObject<UListDataObjectCollection>();
		VolumeCategoryCollection->SetDataID(FName("VolumeCategoryCollection"));
		VolumeCategoryCollection->SetDataDisplayName(GET_DESCRIPTION("_volume"));

		AudioTabCollection->AddChildListData(VolumeCategoryCollection);

		//Overall Volume
		{
			UListDataObjectScalar* OverallVolume = NewObject<UListDataObjectScalar>();
			OverallVolume->SetDataID(FName("OverallVolume"));
			OverallVolume->SetDataDisplayName(GET_DESCRIPTION("_overall_volume"));
			OverallVolume->SetDescriptionRichText(GET_DESCRIPTION("_overall_volume_description"));
			OverallVolume->SetDisplayValueRange(TRange<float>(0.f, 1.f));
			OverallVolume->SetOutputValueRange(TRange<float>(0.f, 2.f));
			OverallVolume->SetSliderStepSzie(0.01f);
			OverallVolume->SetDefaultValueFromString(LexToString(1.f));
			OverallVolume->SetDisplayNumericType(ECommonNumericType::Percentage);
			OverallVolume->SetNumberFormattingOptions(UListDataObjectScalar::NoDecimal());
			OverallVolume->SetDataDynamicGetter(MAKE_OPTIONS_DATA_CONTROL(GetOverallVolume));
			OverallVolume->SetDataDynamicSetter(MAKE_OPTIONS_DATA_CONTROL(SetOverallVolume));
			OverallVolume->SetShouldApplySettingsImmediately(true);

			VolumeCategoryCollection->AddChildListData(OverallVolume);
		}

		//Music Volume
		{
			UListDataObjectScalar* MusicVolume = NewObject<UListDataObjectScalar>();
			MusicVolume->SetDataID(FName("MusicVolume"));
			MusicVolume->SetDataDisplayName(GET_DESCRIPTION("_music_volume"));
			MusicVolume->SetDescriptionRichText(GET_DESCRIPTION("_music_volume_description"));
			MusicVolume->SetDisplayValueRange(TRange<float>(0.f, 1.f));
			MusicVolume->SetOutputValueRange(TRange<float>(0.f, 2.f));
			MusicVolume->SetSliderStepSzie(0.01f);
			MusicVolume->SetDefaultValueFromString(LexToString(1.f));
			MusicVolume->SetDisplayNumericType(ECommonNumericType::Percentage);
			MusicVolume->SetNumberFormattingOptions(UListDataObjectScalar::NoDecimal());
			MusicVolume->SetDataDynamicGetter(MAKE_OPTIONS_DATA_CONTROL(GetMusicVolume));
			MusicVolume->SetDataDynamicSetter(MAKE_OPTIONS_DATA_CONTROL(SetMusicVolume));
			MusicVolume->SetShouldApplySettingsImmediately(true);

			FOptionsDataEditConditionDescriptor AudioConfigured;
			AudioConfigured.SetEditConditionFunc([]() { return !GetDefault<UFrontendDeveloperSettings>()->MusicSoundClass.IsNull(); });
			AudioConfigured.SetDisabledRichReason(TEXT("This audio category is not available."));
			MusicVolume->AddEditCondition(AudioConfigured);
			VolumeCategoryCollection->AddChildListData(MusicVolume);
		}

		//SFX Volume
		{
			UListDataObjectScalar* SFXVolume = NewObject<UListDataObjectScalar>();
			SFXVolume->SetDataID(FName("SFXVolume"));
			SFXVolume->SetDataDisplayName(GET_DESCRIPTION("_sound_effects_volume"));
			SFXVolume->SetDescriptionRichText(GET_DESCRIPTION("_sound_effects_volume_description"));
			SFXVolume->SetDisplayValueRange(TRange<float>(0.f, 1.f));
			SFXVolume->SetOutputValueRange(TRange<float>(0.f, 2.f));
			SFXVolume->SetSliderStepSzie(0.01f);
			SFXVolume->SetDefaultValueFromString(LexToString(1.f));
			SFXVolume->SetDisplayNumericType(ECommonNumericType::Percentage);
			SFXVolume->SetNumberFormattingOptions(UListDataObjectScalar::NoDecimal());
			SFXVolume->SetDataDynamicGetter(MAKE_OPTIONS_DATA_CONTROL(GetSFXVolume));
			SFXVolume->SetDataDynamicSetter(MAKE_OPTIONS_DATA_CONTROL(SetSFXVolume));
			SFXVolume->SetShouldApplySettingsImmediately(true);

			FOptionsDataEditConditionDescriptor AudioConfigured;
			AudioConfigured.SetEditConditionFunc([]() { return !GetDefault<UFrontendDeveloperSettings>()->SFXSoundClass.IsNull(); });
			AudioConfigured.SetDisabledRichReason(TEXT("This audio category is not available."));
			SFXVolume->AddEditCondition(AudioConfigured);
			VolumeCategoryCollection->AddChildListData(SFXVolume);
		}

	}
	
	//Sound Category
	{
		UListDataObjectCollection* SoundCategoryCollection = NewObject<UListDataObjectCollection>();
		SoundCategoryCollection->SetDataID(FName("SoundCategoryCollection"));
		SoundCategoryCollection->SetDataDisplayName(GET_DESCRIPTION("_sound"));

		AudioTabCollection->AddChildListData(SoundCategoryCollection);

		//Allow Background Audio
		{
			UListDataObjectStringBool* AllowBackgroundAudio = NewObject<UListDataObjectStringBool>();
			AllowBackgroundAudio->SetDataID(FName("AllowBackgroundAudio"));
			AllowBackgroundAudio->SetDataDisplayName(GET_DESCRIPTION("_allow_background_audio"));
			AllowBackgroundAudio->SetFalseAsDefaultValue();

			
			AllowBackgroundAudio->SetDataDynamicGetter(MAKE_OPTIONS_DATA_CONTROL(GetAllowBackgroundAudio));
			AllowBackgroundAudio->SetDataDynamicSetter(MAKE_OPTIONS_DATA_CONTROL(SetAllowBackgroundAudio));

			AllowBackgroundAudio->OverrideTrueDisplayText(GET_DESCRIPTION("_enabled"));
			AllowBackgroundAudio->OverrideFalseDisplayText(GET_DESCRIPTION("_disabled"));

			SoundCategoryCollection->AddChildListData(AllowBackgroundAudio);
		}


	}

	RegisteredOptionsTabCollections.Add(AudioTabCollection);
}

void UOptionsDataRegistry::InitVideoCollectionTab()
{
	UListDataObjectCollection* VideoTabCollection = NewObject<UListDataObjectCollection>();
	VideoTabCollection->SetDataID(FName("VideoTabCollection"));
	VideoTabCollection->SetDataDisplayName(GET_DESCRIPTION("_video_settings_tab"));

	UListDataObjectStringEnum* CreatedWinowMode = nullptr;

	//Display Category
	{
		UListDataObjectCollection* DisplayCategoryCollection = NewObject<UListDataObjectCollection>();
		DisplayCategoryCollection->SetDataID(FName("DisplayCategoryCollection"));
		DisplayCategoryCollection->SetDataDisplayName(GET_DESCRIPTION("_display"));

		VideoTabCollection->AddChildListData(DisplayCategoryCollection);

		FOptionsDataEditConditionDescriptor PackagedBuildOnlyCondition;
		PackagedBuildOnlyCondition.SetEditConditionFunc(
			[]()->bool {
				const bool bIsInEditor = GIsEditor || GIsPlayInEditorWorld;

				return !bIsInEditor;
			}
		);
		PackagedBuildOnlyCondition.SetDisabledRichReason(TEXT("\n\n<Disabled>This setting can only be adjusted in packed build</>"));

		//Window Mode
		{
			UListDataObjectStringEnum* WindowMode = NewObject<UListDataObjectStringEnum>();
			WindowMode->SetDataID(FName("WindowMode"));
			WindowMode->SetDataDisplayName(GET_DESCRIPTION("_window_mode"));
			WindowMode->SetDescriptionRichText(GET_DESCRIPTION("_window_mode_description"));
			WindowMode->AddEnumOption(EWindowMode::Fullscreen, GET_DESCRIPTION("_fullscreen_mode"));
			WindowMode->AddEnumOption(EWindowMode::WindowedFullscreen, GET_DESCRIPTION("_borderless_window"));
			WindowMode->AddEnumOption(EWindowMode::Windowed, GET_DESCRIPTION("_windowed"));
			WindowMode->SetDefaultValueFromEnumOption(EWindowMode::WindowedFullscreen);
			WindowMode->SetDataDynamicGetter(MAKE_OPTIONS_DATA_CONTROL(GetFullscreenMode));
			WindowMode->SetDataDynamicSetter(MAKE_OPTIONS_DATA_CONTROL(SetFullscreenMode));
			WindowMode->SetShouldApplySettingsImmediately(true);

			WindowMode->AddEditCondition(PackagedBuildOnlyCondition);
			
			CreatedWinowMode = WindowMode;

			DisplayCategoryCollection->AddChildListData(WindowMode);
		}

		//Screen Resolution
		{
			UMyListDataObjectStringResolution* ScreenResolution = NewObject<UMyListDataObjectStringResolution>();
			ScreenResolution->SetDataID(FName("ScreenResolution"));
			ScreenResolution->SetDataDisplayName(GET_DESCRIPTION("_screen_resolution"));
			ScreenResolution->SetDescriptionRichText(GET_DESCRIPTION("_screen_resolution_description"));
			ScreenResolution->InitResolutionValue();
			ScreenResolution->SetDataDynamicGetter(MAKE_OPTIONS_DATA_CONTROL(GetScreenResolution));
			ScreenResolution->SetDataDynamicSetter(MAKE_OPTIONS_DATA_CONTROL(SetScreenResolution));

			ScreenResolution->AddEditCondition(PackagedBuildOnlyCondition);

			FOptionsDataEditConditionDescriptor WindowModeEditCondition;
			WindowModeEditCondition.SetEditConditionFunc(
				[CreatedWinowMode]()->bool
				{
					const bool bIsBorderlessWinow = CreatedWinowMode->GetCurrentValueAsEnum<EWindowMode::Type>() == EWindowMode::WindowedFullscreen;

					return !bIsBorderlessWinow;
				}
			);
			WindowModeEditCondition.SetDisabledRichReason(TEXT("\n\n<Disabled>Screen Resolution is not adjustable when the 'Widnow Mode' is set to Borderless Window. The value nust match with the maximum allowed resolution.</>"));
			WindowModeEditCondition.SetDisabledForcedStringValue(ScreenResolution->GetMaximumAllowedResolution());

			ScreenResolution->AddEditCondition(WindowModeEditCondition);

			ScreenResolution->AddEditDependencyData(CreatedWinowMode);

			DisplayCategoryCollection->AddChildListData(ScreenResolution);
		}
	}

	//Graphic Category
	{
		UListDataObjectCollection* GraphicsCategoryCollection = NewObject<UListDataObjectCollection>();
		GraphicsCategoryCollection->SetDataID(FName("GraphicsCategoryCollection"));
		GraphicsCategoryCollection->SetDataDisplayName(GET_DESCRIPTION("_graphics"));

		VideoTabCollection->AddChildListData(GraphicsCategoryCollection);

		//Display Gamma
		{
			UListDataObjectScalar* DisplayGamma = NewObject<UListDataObjectScalar>();
			DisplayGamma->SetDataID(FName("DisplayGamma"));
			DisplayGamma->SetDataDisplayName(GET_DESCRIPTION("_gamma"));
			DisplayGamma->SetDescriptionRichText(GET_DESCRIPTION("_gamma_description"));
			DisplayGamma->SetDisplayValueRange(TRange<float>(0.f, 1.f));
			DisplayGamma->SetOutputValueRange(TRange<float>(1.7f, 2.7f)); //The default value Unreal has is 2.2f
			DisplayGamma->SetSliderStepSzie(0.01f);
			DisplayGamma->SetDisplayNumericType(ECommonNumericType::Percentage);
			DisplayGamma->SetNumberFormattingOptions(UListDataObjectScalar::NoDecimal());
			DisplayGamma->SetDataDynamicGetter(MAKE_OPTIONS_DATA_CONTROL(GetCurrentDisplayGamma));
			DisplayGamma->SetDataDynamicSetter(MAKE_OPTIONS_DATA_CONTROL(SetCurrentDisplayGamma));
			DisplayGamma->SetDefaultValueFromString(LexToString(2.2f));

			GraphicsCategoryCollection->AddChildListData(DisplayGamma);
		}

		UListDataObjectStringInteger* CreatedOverallQuality = nullptr;

		//Overall Quality
		{
			UListDataObjectStringInteger* OverallQuality = NewObject<UListDataObjectStringInteger>();
			OverallQuality->SetDataID(FName("OverallQuality"));
			OverallQuality->SetDataDisplayName(GET_DESCRIPTION("_overall_quality"));
			OverallQuality->SetDescriptionRichText(GET_DESCRIPTION("_overall_quality_description"));
			OverallQuality->AddIntegerOption(0, GET_DESCRIPTION("_low"));
			OverallQuality->AddIntegerOption(1, GET_DESCRIPTION("_medium"));
			OverallQuality->AddIntegerOption(2, GET_DESCRIPTION("_high"));
			OverallQuality->AddIntegerOption(3, GET_DESCRIPTION("_epic"));
			OverallQuality->AddIntegerOption(4, GET_DESCRIPTION("_cinematic"));
			OverallQuality->SetDataDynamicGetter(MAKE_OPTIONS_DATA_CONTROL(GetOverallScalabilityLevel));
			OverallQuality->SetDataDynamicSetter(MAKE_OPTIONS_DATA_CONTROL(SetOverallScalabilityLevel));
			OverallQuality->SetShouldApplySettingsImmediately(true);

			GraphicsCategoryCollection->AddChildListData(OverallQuality);

			CreatedOverallQuality = OverallQuality;
		}

		//Resolution Scale
		{
			UListDataObjectScalar* ResolutionScale = NewObject<UListDataObjectScalar>();
			ResolutionScale->SetDataID(FName("ResolutionScale"));
			ResolutionScale->SetDataDisplayName(GET_DESCRIPTION("_3d_resolution"));
			ResolutionScale->SetDescriptionRichText(GET_DESCRIPTION("_3d_resolution_description"));
			ResolutionScale->SetDisplayValueRange(TRange<float>(0.f, 1.f));
			ResolutionScale->SetOutputValueRange(TRange<float>(0.f, 1.f));
			ResolutionScale->SetSliderStepSzie(0.01f);
			ResolutionScale->SetDisplayNumericType(ECommonNumericType::Percentage);
			ResolutionScale->SetNumberFormattingOptions(UListDataObjectScalar::NoDecimal());
			ResolutionScale->SetDataDynamicGetter(MAKE_OPTIONS_DATA_CONTROL(GetResolutionScaleNormalized));
			ResolutionScale->SetDataDynamicSetter(MAKE_OPTIONS_DATA_CONTROL(SetResolutionScaleNormalized));
			ResolutionScale->SetShouldApplySettingsImmediately(true);

			ResolutionScale->AddEditDependencyData(CreatedOverallQuality);

			GraphicsCategoryCollection->AddChildListData(ResolutionScale);
		}
		
		//Global Illumination Quality
		{
			UListDataObjectStringInteger* GlobalIlluminationQuality = NewObject<UListDataObjectStringInteger>();
			GlobalIlluminationQuality->SetDataID(FName("GlobalIlluminationQuality"));
			GlobalIlluminationQuality->SetDataDisplayName(GET_DESCRIPTION("_global_illumination"));
			GlobalIlluminationQuality->SetDescriptionRichText(GET_DESCRIPTION("_global_illumination_description"));
			GlobalIlluminationQuality->AddIntegerOption(0, GET_DESCRIPTION("_low"));
			GlobalIlluminationQuality->AddIntegerOption(1, GET_DESCRIPTION("_medium"));
			GlobalIlluminationQuality->AddIntegerOption(2, GET_DESCRIPTION("_high"));
			GlobalIlluminationQuality->AddIntegerOption(3, GET_DESCRIPTION("_epic"));
			GlobalIlluminationQuality->AddIntegerOption(4, GET_DESCRIPTION("_cinematic"));
			GlobalIlluminationQuality->SetDataDynamicGetter(MAKE_OPTIONS_DATA_CONTROL(GetGlobalIlluminationQuality));
			GlobalIlluminationQuality->SetDataDynamicSetter(MAKE_OPTIONS_DATA_CONTROL(SetGlobalIlluminationQuality));
			GlobalIlluminationQuality->SetShouldApplySettingsImmediately(true);

			GlobalIlluminationQuality->AddEditDependencyData(CreatedOverallQuality);

			CreatedOverallQuality->AddEditDependencyData(GlobalIlluminationQuality);

			GraphicsCategoryCollection->AddChildListData(GlobalIlluminationQuality);
		}

		//Shadow Quality
		{
			UListDataObjectStringInteger* ShadowQuality = NewObject<UListDataObjectStringInteger>();
			ShadowQuality->SetDataID(FName("ShadowQuality"));
			ShadowQuality->SetDataDisplayName(GET_DESCRIPTION("_shadow_quality"));
			ShadowQuality->SetDescriptionRichText(GET_DESCRIPTION("_shadow_quality_description"));
			ShadowQuality->AddIntegerOption(0, GET_DESCRIPTION("_low"));
			ShadowQuality->AddIntegerOption(1, GET_DESCRIPTION("_medium"));
			ShadowQuality->AddIntegerOption(2, GET_DESCRIPTION("_high"));
			ShadowQuality->AddIntegerOption(3, GET_DESCRIPTION("_epic"));
			ShadowQuality->AddIntegerOption(4, GET_DESCRIPTION("_cinematic"));
			ShadowQuality->SetDataDynamicGetter(MAKE_OPTIONS_DATA_CONTROL(GetShadowQuality));
			ShadowQuality->SetDataDynamicSetter(MAKE_OPTIONS_DATA_CONTROL(SetShadowQuality));
			ShadowQuality->SetShouldApplySettingsImmediately(true);

			ShadowQuality->AddEditDependencyData(CreatedOverallQuality);

			CreatedOverallQuality->AddEditDependencyData(ShadowQuality);

			GraphicsCategoryCollection->AddChildListData(ShadowQuality);
		}

		//AntiAliasing Quality
		{
			UListDataObjectStringInteger* AntiAliasingQuality = NewObject<UListDataObjectStringInteger>();
			AntiAliasingQuality->SetDataID(FName("AntiAliasingQuality"));
			AntiAliasingQuality->SetDataDisplayName(GET_DESCRIPTION("_antialiasing_quality"));
			AntiAliasingQuality->SetDescriptionRichText(GET_DESCRIPTION("_antialiasing_quality_description"));
			AntiAliasingQuality->AddIntegerOption(0, GET_DESCRIPTION("_low"));
			AntiAliasingQuality->AddIntegerOption(1, GET_DESCRIPTION("_medium"));
			AntiAliasingQuality->AddIntegerOption(2, GET_DESCRIPTION("_high"));
			AntiAliasingQuality->AddIntegerOption(3, GET_DESCRIPTION("_epic"));
			AntiAliasingQuality->AddIntegerOption(4, GET_DESCRIPTION("_cinematic"));
			AntiAliasingQuality->SetDataDynamicGetter(MAKE_OPTIONS_DATA_CONTROL(GetAntiAliasingQuality));
			AntiAliasingQuality->SetDataDynamicSetter(MAKE_OPTIONS_DATA_CONTROL(SetAntiAliasingQuality));
			AntiAliasingQuality->SetShouldApplySettingsImmediately(true);

			AntiAliasingQuality->AddEditDependencyData(CreatedOverallQuality);

			CreatedOverallQuality->AddEditDependencyData(AntiAliasingQuality);

			GraphicsCategoryCollection->AddChildListData(AntiAliasingQuality);
		}

		//View Distance Quality
		{
			UListDataObjectStringInteger* ViewDistanceQuality = NewObject<UListDataObjectStringInteger>();
			ViewDistanceQuality->SetDataID(FName("ViewDistanceQuality"));
			ViewDistanceQuality->SetDataDisplayName(GET_DESCRIPTION("_view_distance"));
			ViewDistanceQuality->SetDescriptionRichText(GET_DESCRIPTION("_view_distance_description"));
			ViewDistanceQuality->AddIntegerOption(0, GET_DESCRIPTION("_near"));
			ViewDistanceQuality->AddIntegerOption(1, GET_DESCRIPTION("_medium"));
			ViewDistanceQuality->AddIntegerOption(2, GET_DESCRIPTION("_far"));
			ViewDistanceQuality->AddIntegerOption(3, GET_DESCRIPTION("_very_far"));
			ViewDistanceQuality->AddIntegerOption(4, GET_DESCRIPTION("_cinematic"));
			ViewDistanceQuality->SetDataDynamicGetter(MAKE_OPTIONS_DATA_CONTROL(GetViewDistanceQuality));
			ViewDistanceQuality->SetDataDynamicSetter(MAKE_OPTIONS_DATA_CONTROL(SetViewDistanceQuality));
			ViewDistanceQuality->SetShouldApplySettingsImmediately(true);

			ViewDistanceQuality->AddEditDependencyData(CreatedOverallQuality);

			CreatedOverallQuality->AddEditDependencyData(ViewDistanceQuality);

			GraphicsCategoryCollection->AddChildListData(ViewDistanceQuality);
		}

		//Texture Quality
		{
			UListDataObjectStringInteger* TextureQuality = NewObject<UListDataObjectStringInteger>();
			TextureQuality->SetDataID(FName("TextureQuality"));
			TextureQuality->SetDataDisplayName(GET_DESCRIPTION("_texture_quality"));
			TextureQuality->SetDescriptionRichText(GET_DESCRIPTION("_texture_quality_description"));
			TextureQuality->AddIntegerOption(0, GET_DESCRIPTION("_low"));
			TextureQuality->AddIntegerOption(1, GET_DESCRIPTION("_medium"));
			TextureQuality->AddIntegerOption(2, GET_DESCRIPTION("_high"));
			TextureQuality->AddIntegerOption(3, GET_DESCRIPTION("_epic"));
			TextureQuality->AddIntegerOption(4, GET_DESCRIPTION("_cinematic"));
			TextureQuality->SetDataDynamicGetter(MAKE_OPTIONS_DATA_CONTROL(GetTextureQuality));
			TextureQuality->SetDataDynamicSetter(MAKE_OPTIONS_DATA_CONTROL(SetTextureQuality));
			TextureQuality->SetShouldApplySettingsImmediately(true);

			TextureQuality->AddEditDependencyData(CreatedOverallQuality);

			CreatedOverallQuality->AddEditDependencyData(TextureQuality);

			GraphicsCategoryCollection->AddChildListData(TextureQuality);
		}

		//Visual Effects Quality
		{
			UListDataObjectStringInteger* VisualEffectsQuality = NewObject<UListDataObjectStringInteger>();
			VisualEffectsQuality->SetDataID(FName("VisualEffectsQuality"));
			VisualEffectsQuality->SetDataDisplayName(GET_DESCRIPTION("_visual_effects_quality"));
			VisualEffectsQuality->SetDescriptionRichText(GET_DESCRIPTION("_visual_effects_quality_description"));
			VisualEffectsQuality->AddIntegerOption(0, GET_DESCRIPTION("_low"));
			VisualEffectsQuality->AddIntegerOption(1, GET_DESCRIPTION("_medium"));
			VisualEffectsQuality->AddIntegerOption(2, GET_DESCRIPTION("_high"));
			VisualEffectsQuality->AddIntegerOption(3, GET_DESCRIPTION("_epic"));
			VisualEffectsQuality->AddIntegerOption(4, GET_DESCRIPTION("_cinematic"));
			VisualEffectsQuality->SetDataDynamicGetter(MAKE_OPTIONS_DATA_CONTROL(GetVisualEffectQuality));
			VisualEffectsQuality->SetDataDynamicSetter(MAKE_OPTIONS_DATA_CONTROL(SetVisualEffectQuality));
			VisualEffectsQuality->SetShouldApplySettingsImmediately(true);

			VisualEffectsQuality->AddEditDependencyData(CreatedOverallQuality);

			CreatedOverallQuality->AddEditDependencyData(VisualEffectsQuality);

			GraphicsCategoryCollection->AddChildListData(VisualEffectsQuality);
		}

		//Reflection Quality
		{
			UListDataObjectStringInteger* ReflectionQuality = NewObject<UListDataObjectStringInteger>();
			ReflectionQuality->SetDataID(FName("ReflectionQuality"));
			ReflectionQuality->SetDataDisplayName(GET_DESCRIPTION("_reflection_quality"));
			ReflectionQuality->SetDescriptionRichText(GET_DESCRIPTION("_reflection_quality_description"));
			ReflectionQuality->AddIntegerOption(0, GET_DESCRIPTION("_low"));
			ReflectionQuality->AddIntegerOption(1, GET_DESCRIPTION("_medium"));
			ReflectionQuality->AddIntegerOption(2, GET_DESCRIPTION("_high"));
			ReflectionQuality->AddIntegerOption(3, GET_DESCRIPTION("_epic"));
			ReflectionQuality->AddIntegerOption(4, GET_DESCRIPTION("_cinematic"));
			ReflectionQuality->SetDataDynamicGetter(MAKE_OPTIONS_DATA_CONTROL(GetReflectionQuality));
			ReflectionQuality->SetDataDynamicSetter(MAKE_OPTIONS_DATA_CONTROL(SetReflectionQuality));
			ReflectionQuality->SetShouldApplySettingsImmediately(true);

			ReflectionQuality->AddEditDependencyData(CreatedOverallQuality);

			CreatedOverallQuality->AddEditDependencyData(ReflectionQuality);

			GraphicsCategoryCollection->AddChildListData(ReflectionQuality);
		}

		//PostProcessing Quality
		{
			UListDataObjectStringInteger* PostProcessingQuality = NewObject<UListDataObjectStringInteger>();
			PostProcessingQuality->SetDataID(FName("PostProcessingQuality"));
			PostProcessingQuality->SetDataDisplayName(GET_DESCRIPTION("_post_processing_quality"));
			PostProcessingQuality->SetDescriptionRichText(GET_DESCRIPTION("_post_processing_quality_description"));
			PostProcessingQuality->AddIntegerOption(0, GET_DESCRIPTION("_low"));
			PostProcessingQuality->AddIntegerOption(1, GET_DESCRIPTION("_medium"));
			PostProcessingQuality->AddIntegerOption(2, GET_DESCRIPTION("_high"));
			PostProcessingQuality->AddIntegerOption(3, GET_DESCRIPTION("_epic"));
			PostProcessingQuality->AddIntegerOption(4, GET_DESCRIPTION("_cinematic"));
			PostProcessingQuality->SetDataDynamicGetter(MAKE_OPTIONS_DATA_CONTROL(GetPostProcessingQuality));
			PostProcessingQuality->SetDataDynamicSetter(MAKE_OPTIONS_DATA_CONTROL(SetPostProcessingQuality));
			PostProcessingQuality->SetShouldApplySettingsImmediately(true);

			PostProcessingQuality->AddEditDependencyData(CreatedOverallQuality);

			CreatedOverallQuality->AddEditDependencyData(PostProcessingQuality);

			GraphicsCategoryCollection->AddChildListData(PostProcessingQuality);
		}
	}

	//Advanced Graphic Category
	{
		UListDataObjectCollection* AdvencedGraphicCategoryCollection = NewObject<UListDataObjectCollection>();
		AdvencedGraphicCategoryCollection->SetDataID(FName("AdvencedGraphicCategoryCollection"));
		AdvencedGraphicCategoryCollection->SetDataDisplayName(GET_DESCRIPTION("_advenced_graphic"));

		VideoTabCollection->AddChildListData(AdvencedGraphicCategoryCollection);

		//Vertical Sync
		{
			UListDataObjectStringBool* VerticalSync = NewObject<UListDataObjectStringBool>();
			VerticalSync->SetDataID(FName("VerticalSync"));
			VerticalSync->SetDataDisplayName(GET_DESCRIPTION("_v-sync"));
			VerticalSync->SetDescriptionRichText(GET_DESCRIPTION("_v-sync_description"));
			VerticalSync->SetDataDynamicGetter(MAKE_OPTIONS_DATA_CONTROL(IsVSyncEnabled));
			VerticalSync->SetDataDynamicSetter(MAKE_OPTIONS_DATA_CONTROL(SetVSyncEnabled));
			VerticalSync->SetFalseAsDefaultValue();
			VerticalSync->SetShouldApplySettingsImmediately(true);

			FOptionsDataEditConditionDescriptor FullScreenOnlyCondition;
			FullScreenOnlyCondition.SetEditConditionFunc(
				[CreatedWinowMode]()->bool {
					return CreatedWinowMode->GetCurrentValueAsEnum<EWindowMode::Type>() == EWindowMode::Fullscreen;
				}
			);
			FullScreenOnlyCondition.SetDisabledRichReason(TEXT("\n\n<Disabled>This feature only work if the 'Window Mode' is set to 'Fullscreen'</>"));
			FullScreenOnlyCondition.SetDisabledForcedStringValue(TEXT("false"));

			VerticalSync->AddEditCondition(FullScreenOnlyCondition);

			AdvencedGraphicCategoryCollection->AddChildListData(VerticalSync);
		}

		//Frame Rate Limit
		{
			UListDataObjectString* FrameRateLimit = NewObject<UListDataObjectString>();
			FrameRateLimit->SetDataID(FName("FrameRateLimit"));
			FrameRateLimit->SetDataDisplayName(GET_DESCRIPTION("_frame_rate_limt"));
			FrameRateLimit->SetDescriptionRichText(GET_DESCRIPTION("_frame_rate_limt_description"));
			FrameRateLimit->AddDynamicOption(LexToString(30.f), GET_DESCRIPTION("_30_fps"));
			FrameRateLimit->AddDynamicOption(LexToString(60.f), GET_DESCRIPTION("_60_fps"));
			FrameRateLimit->AddDynamicOption(LexToString(90.f), GET_DESCRIPTION("_90_fps"));
			FrameRateLimit->AddDynamicOption(LexToString(120.f), GET_DESCRIPTION("_120_fps"));
			FrameRateLimit->AddDynamicOption(LexToString(0.f), GET_DESCRIPTION("_no_limit"));
			FrameRateLimit->SetDefaultValueFromString(LexToString(0.f));
			FrameRateLimit->SetDataDynamicGetter(MAKE_OPTIONS_DATA_CONTROL(GetFrameRateLimit));
			FrameRateLimit->SetDataDynamicSetter(MAKE_OPTIONS_DATA_CONTROL(SetFrameRateLimit));
			FrameRateLimit->SetShouldApplySettingsImmediately(true);

			AdvencedGraphicCategoryCollection->AddChildListData(FrameRateLimit);
		}

	}

	RegisteredOptionsTabCollections.Add(VideoTabCollection);
}

void UOptionsDataRegistry::InitControllCollectionTab(ULocalPlayer* InOwningLocalPlayer)
{
	UListDataObjectCollection* ControllTabCollection = NewObject<UListDataObjectCollection>();
	ControllTabCollection->SetDataID(FName("ControllTabCollection"));
	ControllTabCollection->SetDataDisplayName(GET_DESCRIPTION("_controll_settings_tab"));

	UEnhancedInputLocalPlayerSubsystem* EISubsystem = InOwningLocalPlayer ? InOwningLocalPlayer->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>() : nullptr;

	if (!EISubsystem) { return; }

	UEnhancedInputUserSettings* EIUserSettings = EISubsystem->GetUserSettings();

	if (!EIUserSettings) { return; }

	//Keyboard Mouse Category
	{
		UListDataObjectCollection* KeyboardMouseCollection = NewObject<UListDataObjectCollection>();
		KeyboardMouseCollection->SetDataID(FName("KeyboardMouseCollection"));
		KeyboardMouseCollection->SetDataDisplayName(GET_DESCRIPTION("_keyboard_and_mouse"));

		ControllTabCollection->AddChildListData(KeyboardMouseCollection);

		//Keyboard Mouse Inputs
		{
			FPlayerMappableKeyQueryOptions KeyboardMouseOnly;
			KeyboardMouseOnly.KeyToMatch = EKeys::S;
			KeyboardMouseOnly.bMatchBasicKeyTypes = true;

			//FPlayerMappableKeyQueryOptions GamepadOnly;
			//GamepadOnly.KeyToMatch = EKeys::Gamepad_FaceButton_Bottom;
			//GamepadOnly.bMatchBasicKeyTypes = true;

			for (const TPair<FString, TObjectPtr<UEnhancedPlayerMappableKeyProfile>>& ProfilePair : EIUserSettings->GetAllAvailableKeyProfiles())
			{
				UEnhancedPlayerMappableKeyProfile* MappableKeyProfile = ProfilePair.Value;

				check(MappableKeyProfile);

				for (const TPair<FName, FKeyMappingRow>& MappingRowPair : MappableKeyProfile->GetPlayerMappingRows())
				{
					for (const FPlayerKeyMapping& KeyMapping : MappingRowPair.Value.Mappings)
					{
						if (MappableKeyProfile->DoesMappingPassQueryOptions(KeyMapping, KeyboardMouseOnly))
						{
							//Debug::Print(
							//	TEXT(" Mapping ID: ") + KeyMapping.GetMappingName().ToString() +
							//	TEXT(" Display Name: ") + KeyMapping.GetDisplayName().ToString() +
							//	TEXT(" Bound Key: ") + KeyMapping.GetCurrentKey().GetDisplayName().ToString()
							//);

							UListDataObjectKeyRemap* KeyRemapDataObject = NewObject<UListDataObjectKeyRemap>();
							KeyRemapDataObject->SetDataID(KeyMapping.GetMappingName());
							KeyRemapDataObject->SetDataDisplayName(KeyMapping.GetDisplayName());
							KeyRemapDataObject->InitKeyRemapData(EIUserSettings, MappableKeyProfile, ECommonInputType::MouseAndKeyboard, KeyMapping);

							KeyboardMouseCollection->AddChildListData(KeyRemapDataObject);

						}


					}
				}
			}
		}
	}

	RegisteredOptionsTabCollections.Add(ControllTabCollection);
}

void UOptionsDataRegistry::InitGamepadCollectionTab(ULocalPlayer* InOwningLocalPlayer)
{
	UListDataObjectCollection* GamepadTabCollection = NewObject<UListDataObjectCollection>();
	GamepadTabCollection->SetDataID(FName("GamepadTabCollection"));
	GamepadTabCollection->SetDataDisplayName(GET_DESCRIPTION("_gamepad_settings_tab"));

	UEnhancedInputLocalPlayerSubsystem* EISubsystem = InOwningLocalPlayer ? InOwningLocalPlayer->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>() : nullptr;

	if (!EISubsystem) { return; }

	UEnhancedInputUserSettings* EIUserSettings = EISubsystem->GetUserSettings();

	if (!EIUserSettings) { return; }

	//Gamepad Category
	{
		UListDataObjectCollection* GamepadCollection = NewObject<UListDataObjectCollection>();
		GamepadCollection->SetDataID(FName("GamepadCollection"));
		GamepadCollection->SetDataDisplayName(GET_DESCRIPTION("_gamepad"));

		GamepadTabCollection->AddChildListData(GamepadCollection);

		//Gamepad Inputs
		{
			FPlayerMappableKeyQueryOptions GamepadOnly;
			GamepadOnly.KeyToMatch = EKeys::Gamepad_FaceButton_Bottom;
			GamepadOnly.bMatchBasicKeyTypes = true;

			for (const TPair<FString, TObjectPtr<UEnhancedPlayerMappableKeyProfile>>& ProfilePair : EIUserSettings->GetAllAvailableKeyProfiles())
			{
				UEnhancedPlayerMappableKeyProfile* MappableKeyProfile = ProfilePair.Value;

				check(MappableKeyProfile);

				for (const TPair<FName, FKeyMappingRow>& MappingRowPair : MappableKeyProfile->GetPlayerMappingRows())
				{
					for (const FPlayerKeyMapping& KeyMapping : MappingRowPair.Value.Mappings)
					{
						if (MappableKeyProfile->DoesMappingPassQueryOptions(KeyMapping, GamepadOnly))
						{
							UListDataObjectKeyRemap* KeyRemapDataObject = NewObject<UListDataObjectKeyRemap>();
							KeyRemapDataObject->SetDataID(KeyMapping.GetMappingName());
							KeyRemapDataObject->SetDataDisplayName(KeyMapping.GetDisplayName());
							KeyRemapDataObject->InitKeyRemapData(EIUserSettings, MappableKeyProfile, ECommonInputType::Gamepad, KeyMapping);

							GamepadCollection->AddChildListData(KeyRemapDataObject);

						}


					}
				}
			}
		}
	}

	RegisteredOptionsTabCollections.Add(GamepadTabCollection);
}

void UOptionsDataRegistry::InitAccesibilityCollectionTab()
{
	UListDataObjectCollection* AccesibilityTabCollection = NewObject<UListDataObjectCollection>();
	AccesibilityTabCollection->SetDataID(FName("AccesibilityTabCollection"));
	AccesibilityTabCollection->SetDataDisplayName(GET_DESCRIPTION("_accesibility_settings_tab"));

	RegisteredOptionsTabCollections.Add(AccesibilityTabCollection);
}
